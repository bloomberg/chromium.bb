// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autofill_profile_syncable_service.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/guid.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

using content::BrowserThread;

namespace {

std::string LimitData(const std::string& data) {
  std::string sanitized_value(data);
  if (sanitized_value.length() > AutofillTable::kMaxDataLength)
    sanitized_value.resize(AutofillTable::kMaxDataLength);
  return sanitized_value;
}

}  // namespace

const char kAutofillProfileTag[] = "google_chrome_autofill_profiles";

AutofillProfileSyncableService::AutofillProfileSyncableService(
    WebDataService* web_data_service)
    : web_data_service_(web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(web_data_service_);
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
      content::Source<WebDataService>(web_data_service_));
}

AutofillProfileSyncableService::~AutofillProfileSyncableService() {
  DCHECK(CalledOnValidThread());
}

AutofillProfileSyncableService::AutofillProfileSyncableService()
    : web_data_service_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
}

SyncError AutofillProfileSyncableService::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    scoped_ptr<SyncChangeProcessor> sync_processor,
    scoped_ptr<SyncErrorFactory> sync_error_factory) {
  DCHECK(CalledOnValidThread());
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());
  DVLOG(1) << "Associating Autofill: MergeDataAndStartSyncing";

  sync_error_factory_ = sync_error_factory.Pass();
  if (!LoadAutofillData(&profiles_.get())) {
    return sync_error_factory_->CreateAndUploadError(
        FROM_HERE, "Could not get the autofill data from WebDatabase.");
  }

  if (DLOG_IS_ON(INFO)) {
    DVLOG(2) << "[AUTOFILL MIGRATION]"
             << "Printing profiles from web db";

    for (ScopedVector<AutofillProfile>::const_iterator ix =
         profiles_.begin(); ix != profiles_.end(); ++ix) {
      AutofillProfile* p = *ix;
      DVLOG(2) << "[AUTOFILL MIGRATION]  "
               << p->GetInfo(NAME_FIRST)
               << p->GetInfo(NAME_LAST)
               << p->guid();
    }
  }

  sync_processor_ = sync_processor.Pass();

  GUIDToProfileMap remaining_profiles;
  CreateGUIDToProfileMap(profiles_.get(), &remaining_profiles);

  DataBundle bundle;
  // Go through and check for all the profiles that sync already knows about.
  for (SyncDataList::const_iterator sync_iter = initial_sync_data.begin();
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
      if (MergeProfile(*(profile_to_merge->second), it->second))
        bundle.profiles_to_sync_back.push_back(it->second);
      DVLOG(2) << "[AUTOFILL SYNC]"
               << "Found similar profile in sync db but with a different guid: "
               << UTF16ToUTF8(it->second->GetInfo(NAME_FIRST))
               << UTF16ToUTF8(it->second->GetInfo(NAME_LAST))
               << "New guid " << it->second->guid()
               << ". Profile to be deleted "
               << profile_to_merge->second->guid();
      remaining_profiles.erase(profile_to_merge);
    }
  }

  if (!SaveChangesToWebData(bundle)) {
    return sync_error_factory_->CreateAndUploadError(
        FROM_HERE,
        "Failed to update webdata.");
  }

  SyncChangeList new_changes;
  for (GUIDToProfileMap::iterator i = remaining_profiles.begin();
       i != remaining_profiles.end(); ++i) {
    new_changes.push_back(
        SyncChange(SyncChange::ACTION_ADD, CreateData(*(i->second))));
    profiles_map_[i->first] = i->second;
  }

  for (size_t i = 0; i < bundle.profiles_to_sync_back.size(); ++i) {
    new_changes.push_back(
        SyncChange(SyncChange::ACTION_UPDATE,
                   CreateData(*(bundle.profiles_to_sync_back[i]))));
  }

  SyncError error;
  if (!new_changes.empty())
    error = sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);

  WebDataService::NotifyOfMultipleAutofillChanges(web_data_service_);

  return error;
}

void AutofillProfileSyncableService::StopSyncing(syncable::ModelType type) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, syncable::AUTOFILL_PROFILE);

  sync_processor_.reset();
  sync_error_factory_.reset();
  profiles_.reset();
  profiles_map_.clear();
}

SyncDataList AutofillProfileSyncableService::GetAllSyncData(
    syncable::ModelType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_processor_.get());
  DCHECK_EQ(type, syncable::AUTOFILL_PROFILE);

  SyncDataList current_data;

  for (GUIDToProfileMap::const_iterator i = profiles_map_.begin();
       i != profiles_map_.end(); ++i) {
    current_data.push_back(CreateData(*(i->second)));
  }
  return current_data;
}

SyncError AutofillProfileSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  DCHECK(CalledOnValidThread());
  if (!sync_processor_.get()) {
    SyncError error(FROM_HERE, "Models not yet associated.",
                    syncable::AUTOFILL_PROFILE);
    return error;
  }

  DataBundle bundle;

  for (SyncChangeList::const_iterator i = change_list.begin();
       i != change_list.end(); ++i) {
    DCHECK(i->IsValid());
    switch (i->change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE:
        CreateOrUpdateProfile(i->sync_data(), &profiles_map_, &bundle);
        break;
      case SyncChange::ACTION_DELETE: {
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
                  SyncChange::ChangeTypeToString(i->change_type()));
    }
  }

  if (!SaveChangesToWebData(bundle)) {
    return sync_error_factory_->CreateAndUploadError(
        FROM_HERE,
        "Failed to update webdata.");
  }

  WebDataService::NotifyOfMultipleAutofillChanges(web_data_service_);

  return SyncError();
}

void AutofillProfileSyncableService::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED);
  DCHECK_EQ(web_data_service_, content::Source<WebDataService>(source).ptr());
  // Check if sync is on. If we receive notification prior to the sync being set
  // up we are going to process all when MergeData..() is called. If we receive
  // notification after the sync exited, it will be sinced next time Chrome
  // starts.
  if (!sync_processor_.get())
    return;

  AutofillProfileChange* change =
      content::Details<AutofillProfileChange>(details).ptr();
  ActOnChange(*change);
}

bool AutofillProfileSyncableService::LoadAutofillData(
    std::vector<AutofillProfile*>* profiles) {
  return GetAutofillTable()->GetAutofillProfiles(profiles);
}

bool AutofillProfileSyncableService::SaveChangesToWebData(
    const DataBundle& bundle) {
  DCHECK(CalledOnValidThread());

  bool success = true;
  for (size_t i = 0; i< bundle.profiles_to_delete.size(); ++i) {
    if (!GetAutofillTable()->RemoveAutofillProfile(
        bundle.profiles_to_delete[i]))
      success = false;
  }

  for (size_t i = 0; i < bundle.profiles_to_add.size(); i++) {
    if (!GetAutofillTable()->AddAutofillProfile(
        *bundle.profiles_to_add[i]))
      success = false;
  }

  for (size_t i = 0; i < bundle.profiles_to_update.size(); i++) {
    if (!GetAutofillTable()->UpdateAutofillProfileMulti(
        *bundle.profiles_to_update[i]))
      success = false;
  }
  return success;
}

// static
bool AutofillProfileSyncableService::OverwriteProfileWithServerData(
    const sync_pb::AutofillProfileSpecifics& specifics,
    AutofillProfile* profile) {
  bool diff = false;
  diff = UpdateMultivaluedField(NAME_FIRST,
                                specifics.name_first(), profile) || diff;
  diff = UpdateMultivaluedField(NAME_MIDDLE,
                                specifics.name_middle(), profile) || diff;
  diff = UpdateMultivaluedField(NAME_LAST,
                                specifics.name_last(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_LINE1,
                     specifics.address_home_line1(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_LINE2,
                     specifics.address_home_line2(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_CITY,
                     specifics.address_home_city(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_STATE,
                     specifics.address_home_state(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_COUNTRY,
                     specifics.address_home_country(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_ZIP,
                     specifics.address_home_zip(), profile) || diff;
  diff = UpdateMultivaluedField(EMAIL_ADDRESS,
                                specifics.email_address(), profile) || diff;
  diff = UpdateField(COMPANY_NAME, specifics.company_name(), profile) || diff;
  diff = UpdateMultivaluedField(PHONE_HOME_WHOLE_NUMBER,
                                specifics.phone_home_whole_number(),
                                profile) || diff;
  return diff;
}

// static
void AutofillProfileSyncableService::WriteAutofillProfile(
    const AutofillProfile& profile,
    sync_pb::EntitySpecifics* profile_specifics) {
  sync_pb::AutofillProfileSpecifics* specifics =
      profile_specifics->mutable_autofill_profile();

  DCHECK(guid::IsValidGUID(profile.guid()));

  // Reset all multi-valued fields in the protobuf.
  specifics->clear_name_first();
  specifics->clear_name_middle();
  specifics->clear_name_last();
  specifics->clear_email_address();
  specifics->clear_phone_home_whole_number();

  specifics->set_guid(profile.guid());
  std::vector<string16> values;
  profile.GetMultiInfo(NAME_FIRST, &values);
  for (size_t i = 0; i < values.size(); ++i)
    specifics->add_name_first(LimitData(UTF16ToUTF8(values[i])));
  profile.GetMultiInfo(NAME_MIDDLE, &values);
  for (size_t i = 0; i < values.size(); ++i)
    specifics->add_name_middle(LimitData(UTF16ToUTF8(values[i])));
  profile.GetMultiInfo(NAME_LAST, &values);
  for (size_t i = 0; i < values.size(); ++i)
    specifics->add_name_last(LimitData(UTF16ToUTF8(values[i])));
  specifics->set_address_home_line1(
      LimitData(UTF16ToUTF8(profile.GetInfo(ADDRESS_HOME_LINE1))));
  specifics->set_address_home_line2(
      LimitData(UTF16ToUTF8(profile.GetInfo(ADDRESS_HOME_LINE2))));
  specifics->set_address_home_city(
      LimitData(UTF16ToUTF8(profile.GetInfo(ADDRESS_HOME_CITY))));
  specifics->set_address_home_state(
      LimitData(UTF16ToUTF8(profile.GetInfo(ADDRESS_HOME_STATE))));
  specifics->set_address_home_country(
      LimitData(UTF16ToUTF8(profile.GetInfo(ADDRESS_HOME_COUNTRY))));
  specifics->set_address_home_zip(
      LimitData(UTF16ToUTF8(profile.GetInfo(ADDRESS_HOME_ZIP))));
  profile.GetMultiInfo(EMAIL_ADDRESS, &values);
  for (size_t i = 0; i < values.size(); ++i)
    specifics->add_email_address(LimitData(UTF16ToUTF8(values[i])));
  specifics->set_company_name(
      LimitData(UTF16ToUTF8(profile.GetInfo(COMPANY_NAME))));
  profile.GetMultiInfo(PHONE_HOME_WHOLE_NUMBER, &values);
  for (size_t i = 0; i < values.size(); ++i)
    specifics->add_phone_home_whole_number(LimitData(UTF16ToUTF8(values[i])));
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
    const SyncData& data, GUIDToProfileMap* profile_map, DataBundle* bundle) {
  DCHECK(profile_map);
  DCHECK(bundle);

  DCHECK_EQ(syncable::AUTOFILL_PROFILE, data.GetDataType());

  const sync_pb::EntitySpecifics& specifics = data.GetSpecifics();
  const sync_pb::AutofillProfileSpecifics& autofill_specifics(
      specifics.autofill_profile());

  GUIDToProfileMap::iterator it = profile_map->find(
        autofill_specifics.guid());
  if (it != profile_map->end()) {
    // Some profile that already present is synced.
    if (OverwriteProfileWithServerData(autofill_specifics, it->second))
      bundle->profiles_to_update.push_back(it->second);
  } else {
    // New profile synced.
    AutofillProfile* new_profile(
        new AutofillProfile(autofill_specifics.guid()));
    OverwriteProfileWithServerData(autofill_specifics, new_profile);

    // Check if profile appears under a different guid.
    for (GUIDToProfileMap::iterator i = profile_map->begin();
         i != profile_map->end(); ++i) {
      if (i->second->Compare(*new_profile) == 0) {
        bundle->profiles_to_delete.push_back(i->second->guid());
        DVLOG(2) << "[AUTOFILL SYNC]"
                 << "Found in sync db but with a different guid: "
                 << UTF16ToUTF8(i->second->GetInfo(NAME_FIRST))
                 << UTF16ToUTF8(i->second->GetInfo(NAME_LAST))
                 << "New guid " << new_profile->guid()
                 << ". Profile to be deleted " << i->second->guid();
        profile_map->erase(i);
        break;
      } else if (!i->second->PrimaryValue().empty() &&
                 i->second->PrimaryValue() == new_profile->PrimaryValue()) {
        // Add it to candidates for merge - if there is no profile with this
        // guid we will merge them.
        bundle->candidates_to_merge.insert(std::make_pair(i->second->guid(),
                                                          new_profile));
      }
    }
    profiles_.push_back(new_profile);
    it = profile_map->insert(std::make_pair(new_profile->guid(),
                                            new_profile)).first;
    bundle->profiles_to_add.push_back(new_profile);
  }
  return it;
}

void AutofillProfileSyncableService::ActOnChange(
     const AutofillProfileChange& change) {
  DCHECK((change.type() == AutofillProfileChange::REMOVE &&
          !change.profile()) ||
         (change.type() != AutofillProfileChange::REMOVE && change.profile()));
  DCHECK(sync_processor_.get());
  SyncChangeList new_changes;
  DataBundle bundle;
  switch (change.type()) {
    case AutofillProfileChange::ADD:
      new_changes.push_back(
          SyncChange(SyncChange::ACTION_ADD, CreateData(*(change.profile()))));
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
          SyncChange(SyncChange::ACTION_UPDATE,
                     CreateData(*(change.profile()))));
      break;
    }
    case AutofillProfileChange::REMOVE: {
      AutofillProfile empty_profile(change.key());
      new_changes.push_back(SyncChange(SyncChange::ACTION_DELETE,
                                       CreateData(empty_profile)));
      profiles_map_.erase(change.key());
      break;
    }
    default:
      NOTREACHED();
  }
  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  if (error.IsSet()) {
    // TODO(isherman): Investigating http://crbug.com/121592
    VLOG(1) << "[AUTOFILL SYNC] "
            << "Failed processing change:\n"
            << "  Error: " << error.message() << "\n"
            << "  Guid: " << change.key();
  }
}

SyncData AutofillProfileSyncableService::CreateData(
    const AutofillProfile& profile) {
  sync_pb::EntitySpecifics specifics;
  WriteAutofillProfile(profile, &specifics);
  return SyncData::CreateLocalData(profile.guid(), profile.guid(), specifics);
}

bool AutofillProfileSyncableService::UpdateField(
    AutofillFieldType field_type,
    const std::string& new_value,
    AutofillProfile* autofill_profile) {
  if (UTF16ToUTF8(autofill_profile->GetInfo(field_type)) == new_value)
    return false;
  autofill_profile->SetInfo(field_type, UTF8ToUTF16(new_value));
  return true;
}

bool AutofillProfileSyncableService::UpdateMultivaluedField(
    AutofillFieldType field_type,
    const ::google::protobuf::RepeatedPtrField<std::string>& new_values,
    AutofillProfile* autofill_profile) {
  std::vector<string16> values;
  autofill_profile->GetMultiInfo(field_type, &values);
  bool changed = false;
  if (static_cast<size_t>(new_values.size()) != values.size()) {
    values.clear();
    values.resize(static_cast<size_t>(new_values.size()));
    changed = true;
  }
  for (size_t i = 0; i < values.size(); ++i) {
    string16 synced_value(
        UTF8ToUTF16(new_values.Get(static_cast<int>(i))));
    if (values[i] != synced_value) {
      values[i] = synced_value;
      changed = true;
    }
  }
  if (changed)
    autofill_profile->SetMultiInfo(field_type, values);
  return changed;
}

bool AutofillProfileSyncableService::MergeProfile(
    const AutofillProfile& merge_from,
    AutofillProfile* merge_into) {
  merge_into->OverwriteWithOrAddTo(merge_from);
  return (merge_into->Compare(merge_from) != 0);
}

AutofillTable* AutofillProfileSyncableService::GetAutofillTable() const {
  return web_data_service_->GetDatabase()->GetAutofillTable();
}

AutofillProfileSyncableService::DataBundle::DataBundle() {}

AutofillProfileSyncableService::DataBundle::~DataBundle() {
}
