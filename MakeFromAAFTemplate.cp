// MakeFromAAFTemplate.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "AAF.h"
#include "AAFFileKinds.h"
#include "AAFResult.h"
#include "AAFTypes.h"
#include "AAFSmartPointer.h"

#include <algorithm>
#include <map>
#include <list>
#include <iostream>
#include <iomanip>
#include <vector>

#include <time.h>

// A GUID in all but name.
//
struct MediaObjectIdentification
{
    aafUInt32 Data1;
    aafUInt16 Data2;
    aafUInt16 Data3;
    aafUInt8  Data4[8];
};
const MediaObjectIdentification nullMediaObjectIdentification =
{0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};


MediaObjectIdentification createUniqueIdentifier(void)
{
  MediaObjectIdentification result = nullMediaObjectIdentification;
  // {FFFFFFFF-3B78-47ec-98DD-68AC60D2539E}
  static MediaObjectIdentification id =
    {0xffffffff, 0x3b78, 0x47ec,
    {0x98, 0xdd, 0x68, 0xac, 0x60, 0xd2, 0x53, 0x9e}};

  if (id.Data1 == 0xffffffff) {
    aafUInt32 ticks = clock();
    time_t timer = time(0);
    id.Data1 += timer + ticks;
  }
  ++id.Data1;
  result = id;
  return result;
}

struct UniqueMaterialIdentification // a.k.a., "UMID"
{
  aafUInt8 SMPTELabel[12];
  aafUInt8 length;
  aafUInt8 instanceHigh;
  aafUInt8 instanceMid;
  aafUInt8 instanceLow;
  MediaObjectIdentification material;
};

AAFRESULT aafMobIDNew( aafMobID_t *mobID)     /* OUT - Newly created Mob ID */
{
    UniqueMaterialIdentification id =
    {{0x06, 0x0a, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01, 0x01, 0x0f, 0x00},
     0x13, 0x00, 0x00, 0x00};
    id.material = createUniqueIdentifier();
    memcpy( mobID, reinterpret_cast<aafMobID_t*>(&id), sizeof(id));

    return AAFRESULT_SUCCESS;
} 

inline void checkResult(HRESULT r)
{
  if (FAILED(r))
    throw r;
}


void checkExpression(bool expression, AAFRESULT r)
{
  if (!expression)
    throw r;
}

// Release a COM object and zero out the pointer.
#define ReleaseCOMObject( p_object )    if( p_object )\
                                        {\
                                            p_object->Release();\
                                            p_object = 0;\
                                        }

static aafProductVersion_t  product_version =
    { 0, 0, 0, 0, kAAFVersionUnknown };
static aafProductIdentification_t  product_id =
    {
        L"Avid Technology",
        L"MakeFromAAFTemplate",
        L"",
        { 0x2c0974e2, 0x3675, 0x4b79,
          { 0xa0, 0xd7, 0xc9, 0x24,
            0xc5, 0x41, 0xfb, 0xed } },
        L"",
        &product_version
    };

int _tmain(int argc, _TCHAR* argv[])
{
	bool success = true;

	AAFRESULT  hr = AAFRESULT_SUCCESS;
	try {
		aafCharacter* inFilename = L"D:\\Temp\\1080p_24_a1a2.aaf";
		aafCharacter* outFilename = L"D:\\Temp\\1080p_24_a1a2_out.aaf";

		// the following steps need to be done:
		//
		// 1. Set Master Mob ID to match the MXF file - this is optional.
		// 1b. Set Master Mob name
		//
		// 2. Set Source Mob ID to match the MXF file - this is mandatory
		// 2b. Set Source Mob name
		// 
		// 3. Set all SourceClip durations to the MXF video duration
		// 
		// 4. Set all Descriptor lengths to the MXF video duration
		//
		// 5. Set File Mob SourceCip StartPosition values to the desired timecode
		//
		// 6. Set Descriptor locators to NULL
		//
		// 7. Replace File Mob IDs (optional)


		IAAFSmartPointer<IAAFFile>  outFile( NULL );
		hr = AAFFileOpenNewModify( outFilename,
										0,
										&product_id,
										&outFile); checkResult( hr );

		try {

			{

				IAAFSmartPointer<IAAFFile>  inFile( NULL );
				hr = AAFFileOpenExistingRead( inFilename,
												0,
												&inFile); checkResult( hr );
				{
					IAAFSmartPointer<IAAFHeader> inHeader( NULL );
					hr = inFile -> GetHeader( &inHeader ); checkResult( hr );
					aafSearchCrit_t inCriteria;
					IAAFSmartPointer<IEnumAAFMobs>   p_in_master_mobs( NULL );
					IAAFSmartPointer<IAAFMob>        p_in_master_mob( NULL );
					inCriteria.tags.mobKind = kAAFMasterMob;
					inCriteria.searchTag = kAAFByMobKind;
					hr = inHeader -> GetMobs ( &inCriteria, &p_in_master_mobs ); checkResult( hr );
					hr = p_in_master_mobs -> NextOne( &p_in_master_mob ); checkResult( hr );
					IAAFSmartPointer<IAAFMob> clone_mob( NULL );
					hr = p_in_master_mob -> CloneExternal( kAAFFollowDepend, kAAFNoIncludeMedia, outFile, &clone_mob ); checkResult( hr );
				}
			}

			IAAFSmartPointer<IAAFHeader>  header( NULL );

			hr = outFile->GetHeader( &header ); checkResult( hr );

			IAAFSmartPointer<IAAFMob>  mob( NULL );

			aafLength_t desired_length = 77; // test value
			aafPosition_t desired_source_offset = 3395;

			aafSearchCrit_t criteria;
			IAAFSmartPointer<IEnumAAFMobs>   p_master_mobs( NULL );
			IAAFSmartPointer<IAAFMob>        p_master_mob( NULL );
			IAAFSmartPointer<IEnumAAFMobs>   p_tape_mobs( NULL );
			IAAFSmartPointer<IAAFMob>        p_tape_mob( NULL );
			std::list<std::pair<aafMobID_t, aafMobID_t>> file_mob_id_mapper;
			aafMobID_t     master_mob_id; // set to appropriate value (for now, use the constructed value)
			aafMobID_t     source_mob_id = {{0x6, 0xa, 0x2b, 0x34, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0xf, 0x0}, 0x13, 0x0, 0x0, 0x0, { 0x5418df6d, 0xb156, 0x19ae, { 0x6, 0xe, 0x2b, 0x34, 0x7f, 0x7f, 0x2a, 0x80 } } };
			aafMobID_t     old_master_mob_id; // set to appropriate value (for now, use the constructed value)
			aafMobID_t     old_source_mob_id; // set to appropriate value (for now, use the constructed value)

			hr = aafMobIDNew( &master_mob_id );

			// look for master mob
			criteria.tags.mobKind = kAAFMasterMob;
			criteria.searchTag = kAAFByMobKind;
			hr = header -> GetMobs ( &criteria, &p_master_mobs ); checkResult( hr );
			hr = p_master_mobs -> NextOne( &p_master_mob ); checkResult( hr );
			hr = p_master_mob -> GetMobID( &old_master_mob_id ); checkResult( hr ); // set master mob ID to desired value (step 1)
			hr = p_master_mob -> SetMobID( master_mob_id ); checkResult( hr ); // set master mob ID to desired value (step 1)
			hr = p_master_mob -> SetName( L"zmk10_300_anim_review_mono_v004.Exported.01" ); checkResult( hr );

			// optional, but recommended...  look for "file" mobs
			IAAFSmartPointer<IEnumAAFMobs>   p_file_mobs( NULL );
			IAAFMob        *p_file_mob = NULL;
			criteria.tags.mobKind = kAAFFileMob;
			criteria.searchTag = kAAFByMobKind;
			hr = header -> GetMobs ( &criteria, &p_file_mobs ); checkResult( hr );
			try {
				std::vector<IAAFMob *> file_mobs;
				while ( ( hr = p_file_mobs -> NextOne( &p_file_mob ) ) == AAFRESULT_SUCCESS ) {
					file_mobs.push_back( p_file_mob );
				}

				for ( size_t i = 0; i < file_mobs.size(); i++ ) {

					aafMobID_t old_id;
					hr = file_mobs[i] -> GetMobID( &old_id ); checkResult( hr );
					aafMobID_t new_id; // constructor does not generate a new id, need to call aafMobIDNew
					hr = aafMobIDNew( &new_id ); checkResult( hr );

					hr = file_mobs[i] -> SetMobID( new_id ); checkResult( hr ); // set file mob ID to newly-generated value (step 7)
					file_mob_id_mapper.push_back( std::pair<aafMobID_t, aafMobID_t>( old_id, new_id ) );

					ReleaseCOMObject( file_mobs[i] );

				}
			} catch ( HRESULT err ) {
				std::cout << "Warning - AAFRESULT exception while resetting file mob IDs: 0x" << std::hex << std::setfill('0') << std::setw(8) << err << std::dec << std::endl;
				throw err;
			} catch ( ... ) {
				std::cout << "Warning - unknown exception while resetting file mob IDs: 0x" << std::endl;
				throw AAFRESULT_INTERNAL_ERROR;
			}

			// look for "tape" source mob
			criteria.tags.mobKind = kAAFTapeMob;
			hr = header -> GetMobs ( &criteria, &p_tape_mobs ); checkResult( hr );
			hr = p_tape_mobs -> NextOne( &p_tape_mob );
			if ( hr == AAFRESULT_NO_MORE_OBJECTS ) { // if no tape mob, look for any physical mob
				criteria.tags.mobKind = kAAFPhysicalMob;
				hr = header -> GetMobs ( &criteria, &p_tape_mobs ); checkResult( hr );
				hr = p_tape_mobs -> NextOne( &p_tape_mob ); checkResult( hr );

			}
			hr = p_tape_mob -> GetMobID( &old_source_mob_id ); checkResult( hr ); // set master mob ID to desired value (step 1)
			hr = p_tape_mob -> SetMobID( source_mob_id ); checkResult( hr ); // set source mob ID to desired value (step 2)
			hr = p_tape_mob -> SetName( L"zmk10_300_anim_review_mono_v004" ); checkResult( hr );

			// walk the master mob slots
			IAAFSmartPointer<IEnumAAFMobSlots> p_mob_slots( NULL );
			IAAFSmartPointer<IAAFMobSlot> p_mob_slot( NULL );

			hr = p_master_mob -> GetSlots( &p_mob_slots );

			while ( ( hr = p_mob_slots -> NextOne( &p_mob_slot ) ) == AAFRESULT_SUCCESS ) {

				try {
					IAAFSmartPointer<IAAFSegment> p_segment( NULL );
					IAAFSmartPointer<IAAFTimelineMobSlot> p_timeline_slot( NULL );
					hr = p_mob_slot -> QueryInterface( IID_IAAFTimelineMobSlot, reinterpret_cast <void**> ( &p_timeline_slot ) );

					if (hr == AAFRESULT_SUCCESS) {
						p_mob_slot -> GetSegment( &p_segment );

						IAAFSmartPointer<IAAFSourceClip> p_source_clip( NULL );
						IAAFSmartPointer<IAAFSequence> p_sequence( NULL );
						bool fix_segment = false;
						hr = p_segment -> QueryInterface (IID_IAAFSequence, reinterpret_cast <void **> ( &p_sequence ));
						if ( hr == AAFRESULT_SUCCESS ) {
							fix_segment = true;
							aafUInt32 numSubSegments = 0;
							p_sequence -> CountComponents ( &numSubSegments );
							bool found_source_clip = false;

							// ASSUMES ONLY ONE SOURCE CLIP PER SLOT!!!
							for (aafUInt32 i = 0; i < numSubSegments; ++i)
							{
								IAAFSmartPointer<IAAFComponent> component( NULL );
								p_sequence -> GetComponentAt ( i, &component );

								// Change the reference if sourceClip, ignore otherwise
								hr = component -> QueryInterface ( IID_IAAFSourceClip, reinterpret_cast <void **> ( &p_source_clip ));
								if ( hr == AAFRESULT_SUCCESS ) {
									found_source_clip = true;
									break;
								}
							}

							// was a source clip found?
							if ( ! found_source_clip )
								hr = AAFRESULT_NO_MORE_OBJECTS;

						} else {
							hr = p_segment -> QueryInterface ( IID_IAAFSourceClip, reinterpret_cast <void **> ( &p_source_clip ));
						}

						if ( hr == AAFRESULT_SUCCESS ) {
							aafSourceRef_t source_ref;
							hr = p_source_clip -> GetSourceReference( &source_ref ); checkResult( hr );
							std::list<std::pair<aafMobID_t, aafMobID_t>>::const_iterator iter = file_mob_id_mapper.begin();
							bool found_id = false;
							while ( iter != file_mob_id_mapper.end() ) {
								std::pair<aafMobID_t, aafMobID_t> id_pair = *iter;
								if (memcmp( &( id_pair.first ), &( source_ref.sourceID ), sizeof( aafMobID_t ) ) == 0) {
									source_ref.sourceID = id_pair.second;  // replace file mob reference from the map
									found_id = true;
									break;
								}
								iter++;
							}
							
							if (! found_id)
								checkResult( AAFRESULT_MOB_NOT_FOUND );
							
							hr = p_source_clip -> SetSourceReference( source_ref ); checkResult( hr );
							IAAFSmartPointer<IAAFComponent> p_sclp_component( NULL );
							hr = p_source_clip -> QueryInterface( IID_IAAFComponent, reinterpret_cast<void **> ( &p_sclp_component )); checkResult( hr );
							p_sclp_component -> SetLength ( desired_length ); // set sourceClip durations to the MXF video duration (step 3)

							if ( fix_segment ) {
								IAAFSmartPointer<IAAFComponent> p_seq_component( NULL );
								hr = p_sequence -> QueryInterface( IID_IAAFComponent, reinterpret_cast<void**>( &p_seq_component ) ); checkResult( hr );
								hr = p_seq_component -> SetLength( desired_length );
							}
					}
					}

				} catch ( HRESULT err ) {
					std::cout << "Warning - AAFRESULT exception while processing master mob slots: 0x" << std::hex << std::setfill('0') << std::setw(8) << err << std::dec << std::endl;
					throw err;
				} catch ( ... ) {
					std::cout << "Warning - unknown exception while processing master mob slots" << std::endl;
					throw AAFRESULT_INTERNAL_ERROR;
					
				}
			}

			criteria.tags.mobKind = kAAFFileMob;
			criteria.searchTag = kAAFByMobKind;
			hr = header -> GetMobs  ( &criteria, &p_file_mobs ); checkResult( hr );
			while ( ( hr = p_file_mobs -> NextOne( &p_file_mob ) ) == AAFRESULT_SUCCESS ) {
				// fix descriptor length
				IAAFSmartPointer<IAAFSourceMob> p_source_mob( NULL );
				IAAFSmartPointer<IAAFEssenceDescriptor> p_essence_descriptor( NULL );
				IAAFSmartPointer<IAAFFileDescriptor> p_file_descriptor( NULL );

				hr = p_file_mob -> QueryInterface( IID_IAAFSourceMob, reinterpret_cast<void **>( &p_source_mob ) ); checkResult( hr );
				hr = p_source_mob -> GetEssenceDescriptor( &p_essence_descriptor ); checkResult( hr );
				hr = p_essence_descriptor ->QueryInterface( IID_IAAFFileDescriptor, reinterpret_cast<void **>( &p_file_descriptor ) );
				hr = p_file_descriptor -> SetLength( desired_length ); checkResult( hr );

				// Now fix segment lengths and timecode
				IAAFSmartPointer<IEnumAAFMobSlots> p_file_mob_slots( NULL );
				IAAFSmartPointer<IAAFMobSlot> p_file_mob_slot( NULL );

				hr = p_file_mob -> GetSlots( &p_file_mob_slots );

				while ( ( hr = p_file_mob_slots -> NextOne( &p_file_mob_slot ) ) == AAFRESULT_SUCCESS ) {

					try {
						IAAFSmartPointer<IAAFSegment> p_segment( NULL );
						IAAFSmartPointer<IAAFTimelineMobSlot> p_timeline_slot( NULL );
						hr = p_file_mob_slot -> QueryInterface( IID_IAAFTimelineMobSlot, reinterpret_cast <void**> ( &p_timeline_slot ) );

						if (hr == AAFRESULT_SUCCESS) {
							p_file_mob_slot -> GetSegment( &p_segment );

							IAAFSmartPointer<IAAFSourceClip> p_source_clip( NULL );
							IAAFSmartPointer<IAAFSequence> p_sequence( NULL );
							bool fix_segment = false;
							hr = p_segment -> QueryInterface (IID_IAAFSequence, reinterpret_cast <void **> ( &p_sequence ));
							if ( hr == AAFRESULT_SUCCESS ) {
								fix_segment = true;
								aafUInt32 numSubSegments = 0;
								p_sequence -> CountComponents ( &numSubSegments );
								bool found_source_clip = false;

								// ASSUMES ONLY ONE SOURCE CLIP PER SLOT!!!
								for (aafUInt32 i = 0; i < numSubSegments; ++i)
								{
									IAAFSmartPointer<IAAFComponent> component( NULL );
									p_sequence -> GetComponentAt ( i, &component );

									// Change the reference if sourceClip, ignore otherwise
									hr = component -> QueryInterface ( IID_IAAFSourceClip, reinterpret_cast <void **> ( &p_source_clip ));
									if ( hr == AAFRESULT_SUCCESS ) {
										found_source_clip = true;
										break;
									}
								}

								// was a source clip found?
								if ( ! found_source_clip )
									hr = AAFRESULT_NO_MORE_OBJECTS;

							} else {
								hr = p_segment -> QueryInterface ( IID_IAAFSourceClip, reinterpret_cast <void **> ( &p_source_clip ));
							}

							if ( hr == AAFRESULT_SUCCESS ) {
								aafSourceRef_t source_ref;
								hr = p_source_clip -> GetSourceReference( &source_ref ); checkResult( hr );

								source_ref.sourceID = source_mob_id;  // replace file mob reference from the map
								source_ref.startTime = desired_source_offset;

								hr = p_source_clip -> SetSourceReference( source_ref ); checkResult( hr );
								IAAFSmartPointer<IAAFComponent> p_sclp_component( NULL );
								hr = p_source_clip -> QueryInterface( IID_IAAFComponent, reinterpret_cast<void **> ( &p_sclp_component )); checkResult( hr );
								p_sclp_component -> SetLength ( desired_length ); // set sourceClip durations to the MXF video duration (step 3)

								if ( fix_segment ) {
									IAAFSmartPointer<IAAFComponent> p_seq_component( NULL );
									hr = p_sequence -> QueryInterface( IID_IAAFComponent, reinterpret_cast<void**>( &p_seq_component ) ); checkResult( hr );
									hr = p_seq_component -> SetLength( desired_length );
								}
							}
						}

					} catch ( HRESULT err ) {
						std::cout << "Warning - AAFRESULT exception while processing file mob 0x" << std::hex << std::setfill('0') << std::setw(8) << err << std::dec << std::endl;
						throw err;
					} catch ( ... ) {
						std::cout << "Warning - unknown exception while processing file mob" << std::endl;
						throw AAFRESULT_INTERNAL_ERROR;
					
					}

				}
			}

		} catch ( ... ) {
			success = false;
			outFile -> Close() ;

		}

		try {
			if ( success ) {
				hr = outFile -> Save(); checkResult( hr );
				hr = outFile -> Close(); checkResult( hr );
			}
		} catch ( HRESULT err ) {
			std::cout << "Warning - AAFRESULT exception while saving.closing new file: 0x" << std::hex << std::setfill('0') << std::setw(8) << err << std::dec << std::endl;
			throw err;
		} catch ( ... ) {
			std::cout << "Warning - unknown exception while saving.closing new file" << std::endl;
			throw AAFRESULT_INTERNAL_ERROR;
					
		}


	} catch ( HRESULT err ) {
		std::cout << "Warning - AAFRESULT exception while opening new file: 0x" << std::hex << std::setfill('0') << std::setw(8) << err << std::dec << std::endl;
		throw err;
	} catch ( ... ) {
		std::cout << "Warning - unknown exception while opening new file" << std::endl;
		throw AAFRESULT_INTERNAL_ERROR;
					
	}

	return 0;
}

