// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"

#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/form_group.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/webdata/common/web_database.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;

namespace autofill {

namespace {

std::string LimitData(const std::string& data) {
  std::string sanitized_value(data);
  if (sanitized_value.length() > AutofillTable::kMaxDataLength)
    sanitized_value.resize(AutofillTable::kMaxDataLength);
  return sanitized_value;
}

void* UserDataKey() {
  // Use the address of a static that COMDAT folding won't ever fold
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

}  // namespace

const char kAutofillProfileTag[] = "google_chrome_autofill_profiles";

AutofillProfileSyncableService::AutofillProfileSyncableService(
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale)
    : webdata_backend_(webdata_backend),
      app_locale_(app_locale),
      scoped_observer_(this) {
  DCHECK(webdata_backend_);

  scoped_observer_.Add(webdata_backend_);
}

AutofillProfileSyncableService::~AutofillProfileSyncableService() {
  DCHECK(CalledOnValidThread());
}

// static
void AutofillProfileSyncableService::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale) {
  web_data_service->GetDBUserData()->SetUserData(
      UserDataKey(),
      new AutofillProfileSyncableService(webdata_backend, app_locale));
}

// static
AutofillProfileSyncableService*
AutofillProfileSyncableService::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillProfileSyncableService*>(
      web_data_service->GetDBUserData()->GetUserData(UserDataKey()));
}

AutofillProfileSyncableService::AutofillProfileSyncableService()
    : webdata_backend_(NULL),
      scoped_observer_(this) {
}

syncer::SyncMergeResult
AutofillProfileSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(CalledOnValidThread());
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());
  DVLOG(1) << "Associating Autofill: MergeDataAndStartSyncing";

  syncer::SyncMergeResult merge_result(type);
  sync_error_factory_ = sync_error_factory.Pass();
  if (!LoadAutofillData(&profiles_.get())) {
    merge_result.set_error(sync_error_factory_->CreateAndUploadError(
        FROM_HERE, "Could not get the autofill data from WebDatabase."));
    return merge_result;
  }

  if (DLOG_IS_ON(INFO)) {
    DVLOG(2) << "[AUTOFILL MIGRATION]"
             << "Printing profiles from web db";

    for (ScopedVector<AutofillProfile>::const_iterator ix =
         profiles_.begin(); ix != profiles_.end(); ++ix) {
      AutofillProfile* p = *ix;
      DVLOG(2) << "[AUTOFILL MIGRATION]  "
               << UTF16ToUTF8(p->GetRawInfo(NAME_FIRST))
               << UTF16ToUTF8(p->GetRawInfo(NAME_LAST))
               << p->guid();
    }
  }

  sync_processor_ = sync_processor.Pass();

  GUIDToProfileMap remaining_profiles;
  CreateGUIDToProfileMap(profiles_.get(), &remaining_profiles);

  DataBundle bundle;
  // Go through and check for all the profiles that sync already knows about.
  for (syncer::SyncDataList::const_iterator sync_iter =
           initial_sync_data.begin();
       sync_iter != initial_sync_data.end();
       ++sync_iter) {
    GUIDToProfileMap::iterator it =
        CreateOrUpdateProfile(*sync_iter, &remaining_profiles, &bundle);
    // |it| points to created/updated profile. Add it to the |profiles_map_| and
    // then remove it from |remaining_profiles|. After this loop is completed
    // |remaining_profiles| will have only those profiles that are not in the
    // sync.
    profiles_map_[it->first] = it->second;
    remaining_profiles.erase(it);
  }

  // Check for similar unmatched profiles - they are created independently on
  // two systems, so merge them.
  for (GUIDToProfileMap::iterator it = bundle.candidates_to_merge.begin();
       it != bundle.candidates_to_merge.end(); ++it) {
    GUIDToProfileMap::iterator profile_to_merge =
        remaining_profiles.find(it->first);
    if (profile_to_merge != remaining_profiles.end()) {
      bundle.profiles_to_delete.push_back(profile_to_merge->second->guid());
      if (MergeProfile(*(profile_to_merge->second), it->second, app_locale_))
        bundle.profiles_to_sync_back.push_back(it->second);
      DVLOG(2) << "[AUTOFILL SYNC]"
               << "Found similar profile in sync db but with a different guid: "
               << UTF16ToUTF8(it->second->GetRawInfo(NAME_FIRST))
               << UTF16ToUTF8(it->second->GetRawInfo(NAME_LAST))
               << "New guid " << it->second->guid()
               << ". Profile to be deleted "
               << profile_to_merge->second->guid();
      remaining_profiles.erase(profile_to_merge);
    }
  }

  if (!SaveChangesToWebData(bundle)) {
    merge_result.set_error(sync_error_factory_->CreateAndUploadError(
        FROM_HERE,
        "Failed to update webdata."));
    return merge_result;
  }

  syncer::SyncChangeList new_changes;
  for (GUIDToProfileMap::iterator i = remaining_profiles.begin();
       i != remaining_profiles.end(); ++i) {
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           CreateData(*(i->second))));
    profiles_map_[i->first] = i->second;
  }

  for (size_t i = 0; i < bundle.profiles_to_sync_back.size(); ++i) {
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_UPDATE,
                           CreateData(*(bundle.profiles_to_sync_back[i]))));
  }

  if (!new_changes.empty()) {
    merge_result.set_error(
        sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  }

  if (webdata_backend_)
    webdata_backend_->NotifyOfMultipleAutofillChanges();

  return merge_result;
}

void AutofillProfileSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, syncer::AUTOFILL_PROFILE);

  sync_processor_.reset();
  sync_error_factory_.reset();
  profiles_.clear();
  profiles_map_.clear();
}

syncer::SyncDataList AutofillProfileSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_processor_.get());
  DCHECK_EQ(type, syncer::AUTOFILL_PROFILE);

  syncer::SyncDataList current_data;

  for (GUIDToProfileMap::const_iterator i = profiles_map_.begin();
       i != profiles_map_.end(); ++i) {
    current_data.push_back(CreateData(*(i->second)));
  }
  return current_data;
}

syncer::SyncError AutofillProfileSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(CalledOnValidThread());
  if (!sync_processor_.get()) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Models not yet associated.",
                            syncer::AUTOFILL_PROFILE);
    return error;
  }

  DataBundle bundle;

  for (syncer::SyncChangeList::const_iterator i = change_list.begin();
       i != change_list.end(); ++i) {
    DCHECK(i->IsValid());
    switch (i->change_type()) {
      case syncer::SyncChange::ACTION_ADD:
      case syncer::SyncChange::ACTION_UPDATE:
        CreateOrUpdateProfile(i->sync_data(), &profiles_map_, &bundle);
        break;
      case syncer::SyncChange::ACTION_DELETE: {
        std::string guid = i->sync_data().GetSpecifics().
             autofill_profile().guid();
        bundle.profiles_to_delete.push_back(guid);
        profiles_map_.erase(guid);
      } break;
      default:
        NOTREACHED() << "Unexpected sync change state.";
        return sync_error_factory_->CreateAndUploadError(
              FROM_HERE,
              "ProcessSyncChanges failed on ChangeType " +
                  syncer::SyncChange::ChangeTypeToString(i->change_type()));
    }
  }

  if (!SaveChangesToWebData(bundle)) {
    return sync_error_factory_->CreateAndUploadError(
        FROM_HERE,
        "Failed to update webdata.");
  }

  if (webdata_backend_)
    webdata_backend_->NotifyOfMultipleAutofillChanges();

  return syncer::SyncError();
}

void AutofillProfileSyncableService::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  // Check if sync is on. If we receive notification prior to the sync being set
  // up we are going to process all when MergeData..() is called. If we receive
  // notification after the sync exited, it will be sinced next time Chrome
  // starts.
  if (sync_processor_.get()) {
    ActOnChange(change);
  } else if (!flare_.is_null()) {
    flare_.Run(syncer::AUTOFILL_PROFILE);
    flare_.Reset();
  }
}

bool AutofillProfileSyncableService::LoadAutofillData(
    std::vector<AutofillProfile*>* profiles) {
  return GetAutofillTable()->GetAutofillProfiles(profiles);
}

bool AutofillProfileSyncableService::SaveChangesToWebData(
    const DataBundle& bundle) {
  DCHECK(CalledOnValidThread());

  AutofillTable* autofill_table = GetAutofillTable();

  bool success = true;
  for (size_t i = 0; i< bundle.profiles_to_delete.size(); ++i) {
    if (!autofill_table->RemoveAutofillProfile(bundle.profiles_to_delete[i]))
      success = false;
  }

  for (size_t i = 0; i < bundle.profiles_to_add.size(); i++) {
    if (!autofill_table->AddAutofillProfile(*bundle.profiles_to_add[i]))
      success = false;
  }

  for (size_t i = 0; i < bundle.profiles_to_update.size(); i++) {
    if (!autofill_table->UpdateAutofillProfile(*bundle.profiles_to_update[i]))
      success = false;
  }
  return success;
}

// static
bool AutofillProfileSyncableService::OverwriteProfileWithServerData(
    const sync_pb::AutofillProfileSpecifics& specifics,
    AutofillProfile* profile,
    const std::string& app_locale) {
  bool diff = false;
  if (specifics.has_origin() && profile->origin() != specifics.origin()) {
    bool was_verified = profile->IsVerified();
    profile->set_origin(specifics.origin());
    diff = true;

    // Verified profiles should never be overwritten by unverified ones.
    DCHECK(!was_verified || profile->IsVerified());
  }

  // Update all multivalued fields: names, emails, and phones.
  diff = UpdateMultivaluedField(NAME_FIRST,
                                specifics.name_first(), profile) || diff;
  diff = UpdateMultivaluedField(NAME_MIDDLE,
                                specifics.name_middle(), profile) || diff;
  diff = UpdateMultivaluedField(NAME_LAST,
                                specifics.name_last(), profile) || diff;
  // Older versions don't have a separate full name; don't overwrite full name
  // in this case.
  if (specifics.name_full().size() > 0) {
    diff = UpdateMultivaluedField(NAME_FULL,
                                  specifics.name_full(), profile) || diff;
  }
  diff = UpdateMultivaluedField(EMAIL_ADDRESS,
                                specifics.email_address(), profile) || diff;
  diff = UpdateMultivaluedField(PHONE_HOME_WHOLE_NUMBER,
                                specifics.phone_home_whole_number(),
                                profile) || diff;

  // Update all simple single-valued address fields.
  diff = UpdateField(COMPANY_NAME, specifics.company_name(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_CITY,
                     specifics.address_home_city(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_STATE,
                     specifics.address_home_state(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_ZIP,
                     specifics.address_home_zip(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_SORTING_CODE,
                     specifics.address_home_sorting_code(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     specifics.address_home_dependent_locality(),
                     profile) || diff;

  // Update the country field, which can contain either a country code (if set
  // by a newer version of Chrome), or a country name (if set by an older
  // version of Chrome).
  base::string16 country_name_or_code =
      ASCIIToUTF16(specifics.address_home_country());
  std::string country_code =
      AutofillCountry::GetCountryCode(country_name_or_code, app_locale);
  diff = UpdateField(ADDRESS_HOME_COUNTRY, country_code, profile) || diff;

  // Update the street address.  In newer versions of Chrome (M34+), this data
  // is stored in the |address_home_street_address| field.  In older versions,
  // this data is stored separated out by address line.
  if (specifics.has_address_home_street_address()) {
    diff = UpdateField(ADDRESS_HOME_STREET_ADDRESS,
                       specifics.address_home_street_address(),
                       profile) || diff;
  } else {
    diff = UpdateField(ADDRESS_HOME_LINE1,
                       specifics.address_home_line1(), profile) || diff;
    diff = UpdateField(ADDRESS_HOME_LINE2,
                       specifics.address_home_line2(), profile) || diff;
  }

  // Update the BCP 47 language code that can be used to format the address for
  // display.
  if (specifics.has_address_home_language_code() &&
      specifics.address_home_language_code() != profile->language_code()) {
    profile->set_language_code(specifics.address_home_language_code());
    diff = true;
  }

  return diff;
}

// static
void AutofillProfileSyncableService::WriteAutofillProfile(
    const AutofillProfile& profile,
    sync_pb::EntitySpecifics* profile_specifics) {
  sync_pb::AutofillProfileSpecifics* specifics =
      profile_specifics->mutable_autofill_profile();

  DCHECK(base::IsValidGUID(profile.guid()));

  // Reset all multi-valued fields in the protobuf.
  specifics->clear_name_first();
  specifics->clear_name_middle();
  specifics->clear_name_last();
  specifics->clear_name_full();
  specifics->clear_email_address();
  specifics->clear_phone_home_whole_number();

  specifics->set_guid(profile.guid());
  specifics->set_origin(profile.origin());

  std::vector<base::string16> values;
  profile.GetRawMultiInfo(NAME_FIRST, &values);
  for (size_t i = 0; i < values.size(); ++i) {
    specifics->add_name_first(LimitData(UTF16ToUTF8(values[i])));
  }

  profile.GetRawMultiInfo(NAME_MIDDLE, &values);
  for (size_t i = 0; i < values.size(); ++i) {
    specifics->add_name_middle(LimitData(UTF16ToUTF8(values[i])));
  }

  profile.GetRawMultiInfo(NAME_LAST, &values);
  for (size_t i = 0; i < values.size(); ++i) {
    specifics->add_name_last(LimitData(UTF16ToUTF8(values[i])));
  }

  profile.GetRawMultiInfo(NAME_FULL, &values);
  for (size_t i = 0; i < values.size(); ++i) {
    specifics->add_name_full(LimitData(UTF16ToUTF8(values[i])));
  }

  specifics->set_address_home_line1(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE1))));
  specifics->set_address_home_line2(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE2))));
  specifics->set_address_home_city(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_CITY))));
  specifics->set_address_home_state(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_STATE))));
  specifics->set_address_home_zip(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_ZIP))));
  specifics->set_address_home_country(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY))));
  specifics->set_address_home_street_address(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS))));
  specifics->set_address_home_sorting_code(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE))));
  specifics->set_address_home_dependent_locality(
      LimitData(
          UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY))));
  specifics->set_address_home_language_code(LimitData(profile.language_code()));

  profile.GetRawMultiInfo(EMAIL_ADDRESS, &values);
  for (size_t i = 0; i < values.size(); ++i) {
    specifics->add_email_address(LimitData(UTF16ToUTF8(values[i])));
  }

  specifics->set_company_name(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(COMPANY_NAME))));

  profile.GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &values);
  for (size_t i = 0; i < values.size(); ++i) {
    specifics->add_phone_home_whole_number(LimitData(UTF16ToUTF8(values[i])));
  }
}

void AutofillProfileSyncableService::CreateGUIDToProfileMap(
    const std::vector<AutofillProfile*>& profiles,
    GUIDToProfileMap* profile_map) {
  DCHECK(profile_map);
  profile_map->clear();
  for (size_t i = 0; i < profiles.size(); ++i)
    (*profile_map)[profiles[i]->guid()] = profiles[i];
}

AutofillProfileSyncableService::GUIDToProfileMap::iterator
AutofillProfileSyncableService::CreateOrUpdateProfile(
    const syncer::SyncData& data,
    GUIDToProfileMap* profile_map,
    DataBundle* bundle) {
  DCHECK(profile_map);
  DCHECK(bundle);

  DCHECK_EQ(syncer::AUTOFILL_PROFILE, data.GetDataType());

  const sync_pb::EntitySpecifics& specifics = data.GetSpecifics();
  const sync_pb::AutofillProfileSpecifics& autofill_specifics(
      specifics.autofill_profile());

  GUIDToProfileMap::iterator existing_profile = profile_map->find(
        autofill_specifics.guid());
  if (existing_profile != profile_map->end()) {
    // The synced profile already exists locally.  It might need to be updated.
    if (OverwriteProfileWithServerData(
            autofill_specifics, existing_profile->second, app_locale_)) {
      bundle->profiles_to_update.push_back(existing_profile->second);
    }
    return existing_profile;
  }


  // New profile synced.
  AutofillProfile* new_profile = new AutofillProfile(
      autofill_specifics.guid(), autofill_specifics.origin());
  OverwriteProfileWithServerData(autofill_specifics, new_profile, app_locale_);

  // Check if profile appears under a different guid. Compares only profile
  // contents. (Ignores origin and language code in comparison.)
  //
  // Unverified profiles should never overwrite verified ones.
  for (GUIDToProfileMap::iterator it = profile_map->begin();
       it != profile_map->end(); ++it) {
    AutofillProfile* local_profile = it->second;
    if (local_profile->Compare(*new_profile) == 0) {
      // Ensure that a verified profile can never revert back to an unverified
      // one.
      if (local_profile->IsVerified() && !new_profile->IsVerified()) {
        new_profile->set_origin(local_profile->origin());
        bundle->profiles_to_sync_back.push_back(new_profile);
      }

      bundle->profiles_to_delete.push_back(local_profile->guid());
      DVLOG(2) << "[AUTOFILL SYNC]"
               << "Found in sync db but with a different guid: "
               << UTF16ToUTF8(local_profile->GetRawInfo(NAME_FIRST))
               << UTF16ToUTF8(local_profile->GetRawInfo(NAME_LAST))
               << "New guid " << new_profile->guid()
               << ". Profile to be deleted " << local_profile->guid();
      profile_map->erase(it);
      break;
    } else if (!local_profile->IsVerified() &&
               !new_profile->IsVerified() &&
               !local_profile->PrimaryValue().empty() &&
               local_profile->PrimaryValue() == new_profile->PrimaryValue()) {
      // Add it to candidates for merge - if there is no profile with this
      // guid we will merge them.
      bundle->candidates_to_merge.insert(
          std::make_pair(local_profile->guid(), new_profile));
    }
  }
  profiles_.push_back(new_profile);
  bundle->profiles_to_add.push_back(new_profile);
  return profile_map->insert(std::make_pair(new_profile->guid(),
                                            new_profile)).first;
}

void AutofillProfileSyncableService::ActOnChange(
     const AutofillProfileChange& change) {
  DCHECK((change.type() == AutofillProfileChange::REMOVE &&
          !change.profile()) ||
         (change.type() != AutofillProfileChange::REMOVE && change.profile()));
  DCHECK(sync_processor_.get());
  syncer::SyncChangeList new_changes;
  DataBundle bundle;
  switch (change.type()) {
    case AutofillProfileChange::ADD:
      new_changes.push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_ADD,
                             CreateData(*(change.profile()))));
      DCHECK(profiles_map_.find(change.profile()->guid()) ==
             profiles_map_.end());
      profiles_.push_back(new AutofillProfile(*(change.profile())));
      profiles_map_[change.profile()->guid()] = profiles_.get().back();
      break;
    case AutofillProfileChange::UPDATE: {
      GUIDToProfileMap::iterator it = profiles_map_.find(
          change.profile()->guid());
      DCHECK(it != profiles_map_.end());
      *(it->second) = *(change.profile());
      new_changes.push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_UPDATE,
                             CreateData(*(change.profile()))));
      break;
    }
    case AutofillProfileChange::REMOVE: {
      AutofillProfile empty_profile(change.key(), std::string());
      new_changes.push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_DELETE,
                             CreateData(empty_profile)));
      profiles_map_.erase(change.key());
      break;
    }
    default:
      NOTREACHED();
  }
  syncer::SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  if (error.IsSet()) {
    // TODO(isherman): Investigating http://crbug.com/121592
    VLOG(1) << "[AUTOFILL SYNC] "
            << "Failed processing change:\n"
            << "  Error: " << error.message() << "\n"
            << "  Guid: " << change.key();
  }
}

syncer::SyncData AutofillProfileSyncableService::CreateData(
    const AutofillProfile& profile) {
  sync_pb::EntitySpecifics specifics;
  WriteAutofillProfile(profile, &specifics);
  return
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics);
}

bool AutofillProfileSyncableService::UpdateField(
    ServerFieldType field_type,
    const std::string& new_value,
    AutofillProfile* autofill_profile) {
  if (UTF16ToUTF8(autofill_profile->GetRawInfo(field_type)) == new_value)
    return false;
  autofill_profile->SetRawInfo(field_type, UTF8ToUTF16(new_value));
  return true;
}

bool AutofillProfileSyncableService::UpdateMultivaluedField(
    ServerFieldType field_type,
    const ::google::protobuf::RepeatedPtrField<std::string>& new_values,
    AutofillProfile* autofill_profile) {
  std::vector<base::string16> values;
  autofill_profile->GetRawMultiInfo(field_type, &values);
  bool changed = false;
  if (static_cast<size_t>(new_values.size()) != values.size()) {
    values.clear();
    values.resize(static_cast<size_t>(new_values.size()));
    changed = true;
  }
  for (size_t i = 0; i < values.size(); ++i) {
    base::string16 synced_value(
        UTF8ToUTF16(new_values.Get(static_cast<int>(i))));
    if (values[i] != synced_value) {
      values[i] = synced_value;
      changed = true;
    }
  }
  if (changed)
    autofill_profile->SetRawMultiInfo(field_type, values);
  return changed;
}

bool AutofillProfileSyncableService::MergeProfile(
    const AutofillProfile& merge_from,
    AutofillProfile* merge_into,
    const std::string& app_locale) {
  // Overwrites all single values and adds to mutli-values. Does not overwrite
  // GUID.
  merge_into->OverwriteWithOrAddTo(merge_from, app_locale);
  return !merge_into->EqualsSansGuid(merge_from);
}

AutofillTable* AutofillProfileSyncableService::GetAutofillTable() const {
  return AutofillTable::FromWebDatabase(webdata_backend_->GetDatabase());
}

void AutofillProfileSyncableService::InjectStartSyncFlare(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

AutofillProfileSyncableService::DataBundle::DataBundle() {}

AutofillProfileSyncableService::DataBundle::~DataBundle() {}

}  // namespace autofill
