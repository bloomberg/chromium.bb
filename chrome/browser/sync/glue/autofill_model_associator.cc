// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_model_associator.h"

#include <functional>
#include <vector>

#include "base/task.h"
#include "base/time.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_change_processor.h"
#include "chrome/browser/sync/glue/autofill_profile_model_associator.h"
#include "chrome/browser/sync/glue/do_optimistic_refresh_task.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/guid.h"
#include "net/base/escape.h"

using base::TimeTicks;

namespace browser_sync {

const char kAutofillTag[] = "google_chrome_autofill";
const char kAutofillEntryNamespaceTag[] = "autofill_entry|";

struct AutofillModelAssociator::DataBundle {
  std::set<AutofillKey> current_entries;
  std::vector<AutofillEntry> new_entries;
  std::set<string16> current_profiles;
  std::vector<AutoFillProfile*> updated_profiles;
  std::vector<AutoFillProfile*> new_profiles;  // We own these pointers.
  ~DataBundle() { STLDeleteElements(&new_profiles); }
};

AutofillModelAssociator::AutofillModelAssociator(
    ProfileSyncService* sync_service,
    WebDatabase* web_database,
    PersonalDataManager* personal_data)
    : sync_service_(sync_service),
      web_database_(web_database),
      personal_data_(personal_data),
      autofill_node_id_(sync_api::kInvalidId),
      abort_association_pending_(false),
      number_of_entries_created_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(sync_service_);
  DCHECK(web_database_);
  DCHECK(personal_data_);
}

AutofillModelAssociator::~AutofillModelAssociator() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
}

bool AutofillModelAssociator::TraverseAndAssociateChromeAutofillEntries(
    sync_api::WriteTransaction* write_trans,
    const sync_api::ReadNode& autofill_root,
    const std::vector<AutofillEntry>& all_entries_from_db,
    std::set<AutofillKey>* current_entries,
    std::vector<AutofillEntry>* new_entries) {

  const std::vector<AutofillEntry>& entries = all_entries_from_db;
  for (std::vector<AutofillEntry>::const_iterator ix = entries.begin();
       ix != entries.end(); ++ix) {
    std::string tag = KeyToTag(ix->key().name(), ix->key().value());
    if (id_map_.find(tag) != id_map_.end()) {
      // It seems that name/value pairs are not unique in the web database.
      // As a result, we have to filter out duplicates here.  This is probably
      // a bug in the database.
      continue;
    }

    sync_api::ReadNode node(write_trans);
    if (node.InitByClientTagLookup(syncable::AUTOFILL, tag)) {
      const sync_pb::AutofillSpecifics& autofill(node.GetAutofillSpecifics());
      DCHECK_EQ(tag, KeyToTag(UTF8ToUTF16(autofill.name()),
                              UTF8ToUTF16(autofill.value())));

      std::vector<base::Time> timestamps;
      if (MergeTimestamps(autofill, ix->timestamps(), &timestamps)) {
        AutofillEntry new_entry(ix->key(), timestamps);
        new_entries->push_back(new_entry);

        sync_api::WriteNode write_node(write_trans);
        if (!write_node.InitByClientTagLookup(syncable::AUTOFILL, tag)) {
          LOG(ERROR) << "Failed to write autofill sync node.";
          return false;
        }
        AutofillChangeProcessor::WriteAutofillEntry(new_entry, &write_node);
      }

      Associate(&tag, node.GetId());
    } else {
      sync_api::WriteNode node(write_trans);
      if (!node.InitUniqueByCreation(syncable::AUTOFILL,
                                     autofill_root, tag)) {
        LOG(ERROR) << "Failed to create autofill sync node.";
        return false;
      }
      node.SetTitle(UTF8ToWide(tag));
      AutofillChangeProcessor::WriteAutofillEntry(*ix, &node);
      Associate(&tag, node.GetId());
      number_of_entries_created_++;
    }

    current_entries->insert(ix->key());
  }
  return true;
}

bool AutofillModelAssociator::LoadAutofillData(
    std::vector<AutofillEntry>* entries,
    std::vector<AutoFillProfile*>* profiles) {
  if (IsAbortPending())
    return false;
  if (!web_database_->GetAllAutofillEntries(entries))
    return false;

  if (IsAbortPending())
    return false;
  if (!web_database_->GetAutoFillProfiles(profiles))
    return false;

  return true;
}

bool AutofillModelAssociator::AssociateModels() {
  VLOG(1) << "Associating Autofill Models";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  {
    base::AutoLock lock(abort_association_pending_lock_);
    abort_association_pending_ = false;
  }

  // TODO(zork): Attempt to load the model association from storage.
  std::vector<AutofillEntry> entries;
  ScopedVector<AutoFillProfile> profiles;

  if (!LoadAutofillData(&entries, &profiles.get())) {
    LOG(ERROR) << "Could not get the autofill data from WebDatabase.";
    return false;
  }

  DataBundle bundle;
  {
    sync_api::WriteTransaction trans(sync_service_->GetUserShare());

    sync_api::ReadNode autofill_root(&trans);
    if (!autofill_root.InitByTagLookup(kAutofillTag)) {
      LOG(ERROR) << "Server did not create the top-level autofill node. We "
                 << "might be running against an out-of-date server.";
      return false;
    }

    if (!TraverseAndAssociateChromeAutofillEntries(&trans, autofill_root,
        entries, &bundle.current_entries, &bundle.new_entries)) {
      return false;
    }

    if (!TraverseAndAssociateAllSyncNodes(
        &trans,
        autofill_root,
        &bundle,
        profiles.get())) {
      return false;
    }
  }

  // Since we're on the DB thread, we don't have to worry about updating
  // the autofill database after closing the write transaction, since
  // this is the only thread that writes to the database.  We also don't have
  // to worry about the sync model getting out of sync, because changes are
  // propogated to the ChangeProcessor on this thread.
  if (!SaveChangesToWebData(bundle)) {
    LOG(ERROR) << "Failed to update autofill entries.";
    return false;
  }

  if (sync_service_->GetAutofillMigrationState() !=
      syncable::MIGRATED) {
    syncable::AutofillMigrationDebugInfo debug_info;
    debug_info.autofill_entries_added_during_migration =
        number_of_entries_created_;
    sync_service_->SetAutofillMigrationDebugInfo(
        syncable::AutofillMigrationDebugInfo::ENTRIES_ADDED,
        debug_info);
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      new DoOptimisticRefreshForAutofill(personal_data_));
  return true;
}

bool AutofillModelAssociator::SaveChangesToWebData(const DataBundle& bundle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  if (IsAbortPending())
    return false;

  if (bundle.new_entries.size() &&
      !web_database_->UpdateAutofillEntries(bundle.new_entries)) {
    return false;
  }

  for (size_t i = 0; i < bundle.new_profiles.size(); i++) {
    if (IsAbortPending())
      return false;
    if (!web_database_->AddAutoFillProfile(*bundle.new_profiles[i]))
      return false;
  }

  for (size_t i = 0; i < bundle.updated_profiles.size(); i++) {
    if (IsAbortPending())
      return false;
    if (!web_database_->UpdateAutoFillProfile(*bundle.updated_profiles[i]))
      return false;
  }
  return true;
}

bool AutofillModelAssociator::TraverseAndAssociateAllSyncNodes(
    sync_api::WriteTransaction* write_trans,
    const sync_api::ReadNode& autofill_root,
    DataBundle* bundle,
    const std::vector<AutoFillProfile*>& all_profiles_from_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  bool autofill_profile_not_migrated = HasNotMigratedYet(write_trans);

  if (VLOG_IS_ON(2) && autofill_profile_not_migrated) {
    VLOG(2) << "[AUTOFILL MIGRATION]"
            << "Printing profiles from web db";

    for (std::vector<AutoFillProfile*>::const_iterator ix =
        all_profiles_from_db.begin(); ix != all_profiles_from_db.end(); ++ix) {
      AutoFillProfile* p = *ix;
      VLOG(2) << "[AUTOFILL MIGRATION]  "
              << p->GetFieldText(AutoFillType(NAME_FIRST))
              << p->GetFieldText(AutoFillType(NAME_LAST));
    }
  }

  if (autofill_profile_not_migrated) {
    VLOG(1) << "[AUTOFILL MIGRATION]"
            << "Iterating over sync db";
  }

  int64 sync_child_id = autofill_root.GetFirstChildId();
  while (sync_child_id != sync_api::kInvalidId) {
    sync_api::ReadNode sync_child(write_trans);
    if (!sync_child.InitByIdLookup(sync_child_id)) {
      LOG(ERROR) << "Failed to fetch child node.";
      return false;
    }
    const sync_pb::AutofillSpecifics& autofill(
        sync_child.GetAutofillSpecifics());

    if (autofill.has_value()) {
      AddNativeEntryIfNeeded(autofill, bundle, sync_child);
    } else if (autofill.has_profile()) {
      // Ignore autofill profiles if we are not upgrading.
      if (autofill_profile_not_migrated) {
        VLOG(2) << "[AUTOFILL MIGRATION] Looking for "
                << autofill.profile().name_first()
                << autofill.profile().name_last();
        AddNativeProfileIfNeeded(
            autofill.profile(),
            bundle,
            sync_child,
            all_profiles_from_db);
      }
    } else {
      NOTREACHED() << "AutofillSpecifics has no autofill data!";
    }

    sync_child_id = sync_child.GetSuccessorId();
  }
  return true;
}

// Define the functor to be used as the predicate in find_if call.
struct CompareProfiles
  : public std::binary_function<AutoFillProfile*, AutoFillProfile*, bool> {
  bool operator() (AutoFillProfile* p1, AutoFillProfile* p2) const {
    if (p1->Compare(*p2) == 0)
      return true;
    else
      return false;
  }
};

AutoFillProfile* AutofillModelAssociator::FindCorrespondingNodeFromWebDB(
    const sync_pb::AutofillProfileSpecifics& profile,
    const std::vector<AutoFillProfile*>& all_profiles_from_db) {
  static std::string guid(guid::GenerateGUID());
  AutoFillProfile p;
  p.set_guid(guid);
  if (!FillProfileWithServerData(&p, profile)) {
    // Not a big deal. We encountered an error. Just say this profile does not
    // exist.
    LOG(ERROR) << " Profile could not be associated";
    return NULL;
  }

  // Now instantiate the functor and call find_if.
  std::vector<AutoFillProfile*>::const_iterator ix =
      std::find_if(all_profiles_from_db.begin(),
                   all_profiles_from_db.end(),
                   std::bind2nd(CompareProfiles(), &p));

  return (ix == all_profiles_from_db.end()) ? NULL : *ix;
}

void AutofillModelAssociator::AddNativeEntryIfNeeded(
    const sync_pb::AutofillSpecifics& autofill, DataBundle* bundle,
    const sync_api::ReadNode& node) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  AutofillKey key(UTF8ToUTF16(autofill.name()), UTF8ToUTF16(autofill.value()));

  if (bundle->current_entries.find(key) == bundle->current_entries.end()) {
    std::vector<base::Time> timestamps;
    int timestamps_count = autofill.usage_timestamp_size();
    for (int c = 0; c < timestamps_count; ++c) {
      timestamps.push_back(base::Time::FromInternalValue(
          autofill.usage_timestamp(c)));
    }
    std::string tag(KeyToTag(key.name(), key.value()));
    Associate(&tag, node.GetId());
    bundle->new_entries.push_back(AutofillEntry(key, timestamps));
  }
}

void AutofillModelAssociator::AddNativeProfileIfNeeded(
    const sync_pb::AutofillProfileSpecifics& profile,
    DataBundle* bundle,
    const sync_api::ReadNode& node,
    const std::vector<AutoFillProfile*>& all_profiles_from_db) {

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  AutoFillProfile* profile_in_web_db = FindCorrespondingNodeFromWebDB(
      profile, all_profiles_from_db);

  if (profile_in_web_db != NULL) {
    VLOG(1) << "[AUTOFILL MIGRATION]"
            << "Node found in web db. So associating";
    int64 sync_id = node.GetId();
    std::string guid = profile_in_web_db->guid();
    Associate(&guid, sync_id);
    return;
  } else {  // Create a new node.
    VLOG(1) << "[AUTOFILL MIGRATION]"
            << "Node not found in web db so creating and associating";
    std::string guid = guid::GenerateGUID();
    if (guid::IsValidGUID(guid) == false) {
      DCHECK(false) << "Guid generated is invalid " << guid;
      return;
    }
    Associate(&guid, node.GetId());
    AutoFillProfile* p = new AutoFillProfile(guid);
    FillProfileWithServerData(p, profile);
    bundle->new_profiles.push_back(p);
  }
}

bool AutofillModelAssociator::DisassociateModels() {
  id_map_.clear();
  id_map_inverse_.clear();
  return true;
}

bool AutofillModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  int64 autofill_sync_id;
  if (!GetSyncIdForTaggedNode(kAutofillTag, &autofill_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level autofill node. We "
               << "might be running against an out-of-date server.";
    return false;
  }
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());

  sync_api::ReadNode autofill_node(&trans);
  if (!autofill_node.InitByIdLookup(autofill_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level autofill node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  // The sync model has user created nodes if the autofill folder has any
  // children.
  *has_nodes = sync_api::kInvalidId != autofill_node.GetFirstChildId();
  return true;
}

void AutofillModelAssociator::AbortAssociation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(abort_association_pending_lock_);
  abort_association_pending_ = true;
}

const std::string*
AutofillModelAssociator::GetChromeNodeFromSyncId(int64 sync_id) {
  SyncIdToAutofillMap::const_iterator iter = id_map_inverse_.find(sync_id);
  return iter == id_map_inverse_.end() ? NULL : &(iter->second);
}

bool AutofillModelAssociator::InitSyncNodeFromChromeId(
    const std::string& node_id,
    sync_api::BaseNode* sync_node) {
  return false;
}

int64 AutofillModelAssociator::GetSyncIdFromChromeId(
    const std::string& autofill) {
  AutofillToSyncIdMap::const_iterator iter = id_map_.find(autofill);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

void AutofillModelAssociator::Associate(
    const std::string* autofill, int64 sync_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK_NE(sync_api::kInvalidId, sync_id);
  DCHECK(id_map_.find(*autofill) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[*autofill] = sync_id;
  id_map_inverse_[sync_id] = *autofill;
}

void AutofillModelAssociator::Disassociate(int64 sync_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  SyncIdToAutofillMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  CHECK(id_map_.erase(iter->second));
  id_map_inverse_.erase(iter);
}

bool AutofillModelAssociator::GetSyncIdForTaggedNode(const std::string& tag,
                                                     int64* sync_id) {
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

bool AutofillModelAssociator::IsAbortPending() {
  base::AutoLock lock(abort_association_pending_lock_);
  return abort_association_pending_;
}

// static
std::string AutofillModelAssociator::KeyToTag(const string16& name,
                                              const string16& value) {
  std::string ns(kAutofillEntryNamespaceTag);
  return ns + EscapePath(UTF16ToUTF8(name)) + "|" +
         EscapePath(UTF16ToUTF8(value));
}

// static
bool AutofillModelAssociator::MergeTimestamps(
    const sync_pb::AutofillSpecifics& autofill,
    const std::vector<base::Time>& timestamps,
    std::vector<base::Time>* new_timestamps) {
  DCHECK(new_timestamps);
  std::set<base::Time> timestamp_union(timestamps.begin(),
                                       timestamps.end());

  size_t timestamps_count = autofill.usage_timestamp_size();

  bool different = timestamps.size() != timestamps_count;
  for (size_t c = 0; c < timestamps_count; ++c) {
    if (timestamp_union.insert(base::Time::FromInternalValue(
            autofill.usage_timestamp(c))).second) {
      different = true;
    }
  }

  if (different) {
    new_timestamps->insert(new_timestamps->begin(),
                           timestamp_union.begin(),
                           timestamp_union.end());
  }
  return different;
}

// Helper to compare the local value and cloud value of a field, merge into
// the local value if they differ, and return whether the merge happened.
bool MergeField(FormGroup* f, AutoFillFieldType t,
                const std::string& specifics_field) {
  if (UTF16ToUTF8(f->GetFieldText(AutoFillType(t))) == specifics_field)
    return false;
  f->SetInfo(AutoFillType(t), UTF8ToUTF16(specifics_field));
  return true;
}

// static
bool AutofillModelAssociator::FillProfileWithServerData(
    AutoFillProfile* merge_into,
    const sync_pb::AutofillProfileSpecifics& specifics) {
  bool diff = false;
  AutoFillProfile* p = merge_into;
  const sync_pb::AutofillProfileSpecifics& s(specifics);
  diff = MergeField(p, NAME_FIRST, s.name_first()) || diff;
  diff = MergeField(p, NAME_LAST, s.name_last()) || diff;
  diff = MergeField(p, NAME_MIDDLE, s.name_middle()) || diff;
  diff = MergeField(p, ADDRESS_HOME_LINE1, s.address_home_line1()) || diff;
  diff = MergeField(p, ADDRESS_HOME_LINE2, s.address_home_line2()) || diff;
  diff = MergeField(p, ADDRESS_HOME_CITY, s.address_home_city()) || diff;
  diff = MergeField(p, ADDRESS_HOME_STATE, s.address_home_state()) || diff;
  diff = MergeField(p, ADDRESS_HOME_COUNTRY, s.address_home_country()) || diff;
  diff = MergeField(p, ADDRESS_HOME_ZIP, s.address_home_zip()) || diff;
  diff = MergeField(p, EMAIL_ADDRESS, s.email_address()) || diff;
  diff = MergeField(p, COMPANY_NAME, s.company_name()) || diff;
  diff = MergeField(p, PHONE_FAX_WHOLE_NUMBER, s.phone_fax_whole_number())
      || diff;
  diff = MergeField(p, PHONE_HOME_WHOLE_NUMBER, s.phone_home_whole_number())
      || diff;
  return diff;
}

bool AutofillModelAssociator::HasNotMigratedYet(
    const sync_api::BaseTransaction* trans) {

  // Now read the current value from the directory.
  syncable::AutofillMigrationState autofill_migration_state =
      sync_service_->GetAutofillMigrationState();

  DCHECK_NE(autofill_migration_state, syncable::NOT_DETERMINED);

  if (autofill_migration_state== syncable::NOT_DETERMINED) {
    VLOG(1) << "Autofill migration state is not determined inside "
            << " model associator";
  }

  if (autofill_migration_state == syncable::NOT_MIGRATED) {
    return true;
  }

  if (autofill_migration_state == syncable::INSUFFICIENT_INFO_TO_DETERMINE) {
      VLOG(1) << "[AUTOFILL MIGRATION]"
              << "current autofill migration state is insufficient info to"
              << "determine.";
      sync_api::ReadNode autofill_profile_root_node(trans);
      if (!autofill_profile_root_node.InitByTagLookup(
          browser_sync::kAutofillProfileTag) ||
          autofill_profile_root_node.GetFirstChildId()==
            static_cast<int64>(0)) {
        sync_service_->SetAutofillMigrationState(
            syncable::NOT_MIGRATED);

        VLOG(1) << "[AUTOFILL MIGRATION]"
                << "Current autofill migration state is NOT Migrated because"
                << "legacy autofill root node is present whereas new "
                << "Autofill profile root node is absent.";
        return true;
      }

      sync_service_->SetAutofillMigrationState(syncable::MIGRATED);

      VLOG(1) << "[AUTOFILL MIGRATION]"
              << "Current autofill migration state is migrated.";
  }

  return false;
}

}  // namespace browser_sync
