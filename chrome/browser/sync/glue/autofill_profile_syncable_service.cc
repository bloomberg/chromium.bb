// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_profile_syncable_service.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/guid.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

namespace {

// Helper to compare the local value and cloud value of a field, merge into
// the local value if they differ, and return whether the merge happened.
bool MergeField(FormGroup* form_group,
                AutofillFieldType field_type,
                const std::string& specifics_field) {
  if (UTF16ToUTF8(form_group->GetInfo(field_type)) == specifics_field)
    return false;
  form_group->SetInfo(field_type, UTF8ToUTF16(specifics_field));
  return true;
}

}  // namespace

namespace browser_sync {

const char kAutofillProfileTag[] = "google_chrome_autofill_profiles";

AutofillProfileSyncableService::AutofillProfileSyncableService(
    WebDatabase* web_database,
    Profile* profile)
    : web_database_(web_database),
      profile_(profile),
      sync_processor_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(web_database_);
  DCHECK(profile);
  notification_registrar_.Add(this,
      chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
      Source<WebDataService>(
          profile_->GetWebDataService(Profile::EXPLICIT_ACCESS)));
}

AutofillProfileSyncableService::~AutofillProfileSyncableService() {
  DCHECK(CalledOnValidThread());
}

AutofillProfileSyncableService::AutofillProfileSyncableService()
    : web_database_(NULL),
      profile_(NULL),
      sync_processor_(NULL) {
}

SyncError AutofillProfileSyncableService::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    SyncChangeProcessor* sync_processor) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_processor_ == NULL);
  VLOG(1) << "Associating Autofill: MergeDataAndStartSyncing";

  if (!LoadAutofillData(&profiles_.get())) {
    return SyncError(
        FROM_HERE, "Could not get the autofill data from WebDatabase.",
        model_type());
  }

  if (VLOG_IS_ON(2)) {
    VLOG(2) << "[AUTOFILL MIGRATION]"
            << "Printing profiles from web db";

    for (ScopedVector<AutofillProfile>::const_iterator ix =
        profiles_.begin(); ix != profiles_.end(); ++ix) {
      AutofillProfile* p = *ix;
      VLOG(2) << "[AUTOFILL MIGRATION]  "
              << p->GetInfo(NAME_FIRST)
              << p->GetInfo(NAME_LAST)
              << p->guid();
    }
  }

  sync_processor_ = sync_processor;

  GUIDToProfileMap remaining_profiles;
  CreateGUIDToProfileMap(profiles_.get(), &remaining_profiles);

  DataBundle bundle;
  // Go through and check for all the profiles that sync already knows about.
  for (SyncDataList::const_iterator sync_iter = initial_sync_data.begin();
       sync_iter != initial_sync_data.end();
       ++sync_iter) {
    GUIDToProfileMap::iterator it =
        CreateOrUpdateProfile(*sync_iter, &remaining_profiles, &bundle);
    profiles_map_[it->first] = it->second;
    remaining_profiles.erase(it);
  }

  if (!SaveChangesToWebData(bundle))
    return SyncError(FROM_HERE, "Failed to update webdata.", model_type());

  SyncChangeList new_changes;
  for (GUIDToProfileMap::iterator i = remaining_profiles.begin();
       i != remaining_profiles.end(); ++i) {
    new_changes.push_back(
        SyncChange(SyncChange::ACTION_ADD, CreateData(*(i->second))));
    profiles_map_[i->first] = i->second;
  }

  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);

  WebDataService::NotifyOfMultipleAutofillChanges(profile_);

  return error;
}

void AutofillProfileSyncableService::StopSyncing(syncable::ModelType type) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_processor_ != NULL);
  DCHECK_EQ(type, syncable::AUTOFILL_PROFILE);

  sync_processor_ = NULL;
  profiles_.reset();
  profiles_map_.clear();
}

SyncDataList AutofillProfileSyncableService::GetAllSyncData(
    syncable::ModelType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_processor_ != NULL);
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
  DCHECK(sync_processor_ != NULL);
  if (sync_processor_ == NULL) {
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
             GetExtension(sync_pb::autofill_profile).guid();
        bundle.profiles_to_delete.push_back(guid);
        profiles_map_.erase(guid);
      } break;
      default:
        NOTREACHED() << "Unexpected sync change state.";
        return SyncError(FROM_HERE, "ProcessSyncChanges failed on ChangeType " +
                         SyncChange::ChangeTypeToString(i->change_type()),
                         syncable::AUTOFILL_PROFILE);
    }
  }

  if (!SaveChangesToWebData(bundle))
    return SyncError(FROM_HERE, "Failed to update webdata.", model_type());

  WebDataService::NotifyOfMultipleAutofillChanges(profile_);

  return SyncError();
}

void AutofillProfileSyncableService::Observe(int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED);
  // Check if sync is on. If we receive notification prior to the sync being set
  // up we are going to process all when MergeData..() is called. If we receive
  // notification after the sync exited, it will be sinced next time Chrome
  // starts.
  if (!sync_processor_)
    return;
  WebDataService* wds = Source<WebDataService>(source).ptr();

  DCHECK(wds && wds->GetDatabase() == web_database_);

  AutofillProfileChange* change = Details<AutofillProfileChange>(details).ptr();

  ActOnChange(*change);
}

bool AutofillProfileSyncableService::LoadAutofillData(
    std::vector<AutofillProfile*>* profiles) {
  return web_database_->GetAutofillTable()->GetAutofillProfiles(profiles);
}

bool AutofillProfileSyncableService::SaveChangesToWebData(
    const DataBundle& bundle) {
  DCHECK(CalledOnValidThread());

  for (size_t i = 0; i < bundle.profiles_to_add.size(); i++) {
    if (!web_database_->GetAutofillTable()->AddAutofillProfile(
        *bundle.profiles_to_add[i]))
      return false;
  }

  for (size_t i = 0; i < bundle.profiles_to_update.size(); i++) {
    if (!web_database_->GetAutofillTable()->UpdateAutofillProfile(
        *bundle.profiles_to_update[i]))
      return false;
  }

  for (size_t i = 0; i< bundle.profiles_to_delete.size(); ++i) {
    if (!web_database_->GetAutofillTable()->RemoveAutofillProfile(
        bundle.profiles_to_delete[i]))
      return false;
  }
  return true;
}

// static
bool AutofillProfileSyncableService::OverwriteProfileWithServerData(
    const sync_pb::AutofillProfileSpecifics& specifics,
    AutofillProfile* profile) {
  bool diff = false;
  diff = MergeField(profile, NAME_FIRST, specifics.name_first()) || diff;
  diff = MergeField(profile, NAME_LAST, specifics.name_last()) || diff;
  diff = MergeField(profile, NAME_MIDDLE, specifics.name_middle()) || diff;
  diff = MergeField(profile, ADDRESS_HOME_LINE1,
                    specifics.address_home_line1()) || diff;
  diff = MergeField(profile, ADDRESS_HOME_LINE2,
                    specifics.address_home_line2()) || diff;
  diff = MergeField(profile, ADDRESS_HOME_CITY,
                    specifics.address_home_city()) || diff;
  diff = MergeField(profile, ADDRESS_HOME_STATE,
                    specifics.address_home_state()) || diff;
  diff = MergeField(profile, ADDRESS_HOME_COUNTRY,
                    specifics.address_home_country()) || diff;
  diff = MergeField(profile, ADDRESS_HOME_ZIP,
                    specifics.address_home_zip()) || diff;
  diff = MergeField(profile, EMAIL_ADDRESS, specifics.email_address()) || diff;
  diff = MergeField(profile, COMPANY_NAME, specifics.company_name()) || diff;
  diff = MergeField(profile, PHONE_HOME_WHOLE_NUMBER,
                    specifics.phone_home_whole_number()) || diff;
  return diff;
}

// static
void AutofillProfileSyncableService::WriteAutofillProfile(
    const AutofillProfile& profile,
    sync_pb::EntitySpecifics* profile_specifics) {
  sync_pb::AutofillProfileSpecifics* specifics =
      profile_specifics->MutableExtension(sync_pb::autofill_profile);

  DCHECK(guid::IsValidGUID(profile.guid()));

  specifics->set_guid(profile.guid());
  specifics->set_name_first(UTF16ToUTF8(profile.GetInfo(NAME_FIRST)));
  specifics->set_name_middle(UTF16ToUTF8(profile.GetInfo(NAME_MIDDLE)));
  specifics->set_name_last(UTF16ToUTF8(profile.GetInfo(NAME_LAST)));
  specifics->set_address_home_line1(
      UTF16ToUTF8(profile.GetInfo(ADDRESS_HOME_LINE1)));
  specifics->set_address_home_line2(
      UTF16ToUTF8(profile.GetInfo(ADDRESS_HOME_LINE2)));
  specifics->set_address_home_city(UTF16ToUTF8(profile.GetInfo(
      ADDRESS_HOME_CITY)));
  specifics->set_address_home_state(UTF16ToUTF8(profile.GetInfo(
      ADDRESS_HOME_STATE)));
  specifics->set_address_home_country(UTF16ToUTF8(profile.GetInfo(
      ADDRESS_HOME_COUNTRY)));
  specifics->set_address_home_zip(UTF16ToUTF8(profile.GetInfo(
      ADDRESS_HOME_ZIP)));
  specifics->set_email_address(UTF16ToUTF8(profile.GetInfo(EMAIL_ADDRESS)));
  specifics->set_company_name(UTF16ToUTF8(profile.GetInfo(COMPANY_NAME)));
  specifics->set_phone_home_whole_number(UTF16ToUTF8(profile.GetInfo(
      PHONE_HOME_WHOLE_NUMBER)));
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
      specifics.GetExtension(sync_pb::autofill_profile));

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
        VLOG(2) << "[AUTOFILL SYNC]"
                << "Found in sync db but with a different guid: "
                << UTF16ToUTF8(it->second->GetInfo(NAME_FIRST))
                << UTF16ToUTF8(it->second->GetInfo(NAME_LAST))
                << "New guid " << new_profile->guid()
                << ". Profile to be deleted " << it->second->guid();
        profile_map->erase(i);
        break;
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
  DCHECK(sync_processor_);
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
    LOG(WARNING) << "[AUTOFILL SYNC]"
                 << " Failed processing change:"
                 << " Error:" << error.message()
                 << " Guid:" << change.key();
  }
}

SyncData AutofillProfileSyncableService::CreateData(
    const AutofillProfile& profile) {
  sync_pb::EntitySpecifics specifics;
  WriteAutofillProfile(profile, &specifics);
  return SyncData::CreateLocalData(profile.guid(), profile.guid(), specifics);
}

AutofillProfileSyncableService::DataBundle::DataBundle() {}

AutofillProfileSyncableService::DataBundle::~DataBundle() {
}

}  // namespace browser_sync
