// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_change_processor.h"

#include <string>
#include <vector>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/glue/autofill_profile_model_associator.h"
#include "chrome/browser/sync/glue/do_optimistic_refresh_task.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/guid.h"
#include "chrome/common/notification_service.h"

namespace browser_sync {

struct AutofillChangeProcessor::AutofillChangeRecord {
  sync_api::SyncManager::ChangeRecord::Action action_;
  int64 id_;
  sync_pb::AutofillSpecifics autofill_;
  AutofillChangeRecord(sync_api::SyncManager::ChangeRecord::Action action,
                       int64 id, const sync_pb::AutofillSpecifics& autofill)
      : action_(action),
        id_(id),
        autofill_(autofill) { }
};

AutofillChangeProcessor::AutofillChangeProcessor(
    AutofillModelAssociator* model_associator,
    WebDatabase* web_database,
    PersonalDataManager* personal_data,
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      model_associator_(model_associator),
      web_database_(web_database),
      personal_data_(personal_data),
      observing_(false) {
  DCHECK(model_associator);
  DCHECK(web_database);
  DCHECK(error_handler);
  DCHECK(personal_data);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  StartObserving();
}

AutofillChangeProcessor::~AutofillChangeProcessor() {}

void AutofillChangeProcessor::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  // Ensure this notification came from our web database.
  WebDataService* wds = Source<WebDataService>(source).ptr();
  if (!wds || wds->GetDatabase() != web_database_)
    return;

  DCHECK(running());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!observing_)
    return;

  sync_api::WriteTransaction trans(share_handle());
  sync_api::ReadNode autofill_root(&trans);
  if (!autofill_root.InitByTagLookup(kAutofillTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Server did not create the top-level autofill node. "
        "We might be running against an out-of-date server.");
    return;
  }

  DCHECK(type.value == NotificationType::AUTOFILL_ENTRIES_CHANGED);

  AutofillChangeList* changes = Details<AutofillChangeList>(details).ptr();
  ObserveAutofillEntriesChanged(changes, &trans, autofill_root);
}

void AutofillChangeProcessor::PostOptimisticRefreshTask() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      new DoOptimisticRefreshForAutofill(
           personal_data_));
}

void AutofillChangeProcessor::ObserveAutofillEntriesChanged(
    AutofillChangeList* changes, sync_api::WriteTransaction* trans,
    const sync_api::ReadNode& autofill_root) {
  for (AutofillChangeList::iterator change = changes->begin();
       change != changes->end(); ++change) {
    switch (change->type()) {
      case AutofillChange::ADD:
        {
          sync_api::WriteNode sync_node(trans);
          std::string tag =
              AutofillModelAssociator::KeyToTag(change->key().name(),
                                                change->key().value());
          if (!sync_node.InitUniqueByCreation(syncable::AUTOFILL,
                                              autofill_root, tag)) {
            error_handler()->OnUnrecoverableError(FROM_HERE,
                "Failed to create autofill sync node.");
            return;
          }

          std::vector<base::Time> timestamps;
          if (!web_database_->GetAutofillTimestamps(
                  change->key().name(),
                  change->key().value(),
                  &timestamps)) {
            error_handler()->OnUnrecoverableError(FROM_HERE,
                "Failed to get timestamps.");
            return;
          }

          sync_node.SetTitle(UTF8ToWide(tag));

          WriteAutofillEntry(AutofillEntry(change->key(), timestamps),
                             &sync_node);
          model_associator_->Associate(&tag, sync_node.GetId());
        }
        break;

      case AutofillChange::UPDATE:
        {
          sync_api::WriteNode sync_node(trans);
          std::string tag = AutofillModelAssociator::KeyToTag(
              change->key().name(), change->key().value());
          int64 sync_id = model_associator_->GetSyncIdFromChromeId(tag);
          if (sync_api::kInvalidId == sync_id) {
            std::string err = "Unexpected notification for: " +
                UTF16ToUTF8(change->key().name());
            error_handler()->OnUnrecoverableError(FROM_HERE, err);
            return;
          } else {
            if (!sync_node.InitByIdLookup(sync_id)) {
              error_handler()->OnUnrecoverableError(FROM_HERE,
                  "Autofill node lookup failed.");
              return;
            }
          }

          std::vector<base::Time> timestamps;
          if (!web_database_->GetAutofillTimestamps(
                   change->key().name(),
                   change->key().value(),
                   &timestamps)) {
            error_handler()->OnUnrecoverableError(FROM_HERE,
                "Failed to get timestamps.");
            return;
          }

          WriteAutofillEntry(AutofillEntry(change->key(), timestamps),
                             &sync_node);
        }
        break;
      case AutofillChange::REMOVE: {
        std::string tag = AutofillModelAssociator::KeyToTag(
            change->key().name(), change->key().value());
        RemoveSyncNode(tag, trans);
        }
        break;
    }
  }
}

void AutofillChangeProcessor::RemoveSyncNode(const std::string& tag,
    sync_api::WriteTransaction* trans) {
  sync_api::WriteNode sync_node(trans);
  int64 sync_id = model_associator_->GetSyncIdFromChromeId(tag);
  if (sync_api::kInvalidId == sync_id) {
    std::string err = "Unexpected notification for: " + tag;
    error_handler()->OnUnrecoverableError(FROM_HERE, err);
    return;
  } else {
    if (!sync_node.InitByIdLookup(sync_id)) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "Autofill node lookup failed.");
      return;
    }
    model_associator_->Disassociate(sync_node.GetId());
    sync_node.Remove();
  }
}

void AutofillChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!running())
    return;
  StopObserving();

  bool autofill_profile_not_migrated = HasNotMigratedYet(trans);

  sync_api::ReadNode autofill_root(trans);
  if (!autofill_root.InitByTagLookup(kAutofillTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Autofill root node lookup failed.");
    return;
  }

  for (int i = 0; i < change_count; ++i) {
    sync_api::SyncManager::ChangeRecord::Action action(changes[i].action);
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE == action) {
      DCHECK(changes[i].specifics.HasExtension(sync_pb::autofill))
          << "Autofill specifics data not present on delete!";
      const sync_pb::AutofillSpecifics& autofill =
          changes[i].specifics.GetExtension(sync_pb::autofill);
      if (autofill.has_value() ||
        (autofill_profile_not_migrated && autofill.has_profile())) {
        autofill_changes_.push_back(AutofillChangeRecord(changes[i].action,
                                                         changes[i].id,
                                                         autofill));
      } else {
        NOTREACHED() << "Autofill specifics has no data!";
      }
      continue;
    }

    // Handle an update or add.
    sync_api::ReadNode sync_node(trans);
    if (!sync_node.InitByIdLookup(changes[i].id)) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "Autofill node lookup failed.");
      return;
    }

    // Check that the changed node is a child of the autofills folder.
    DCHECK(autofill_root.GetId() == sync_node.GetParentId());
    DCHECK(syncable::AUTOFILL == sync_node.GetModelType());

    const sync_pb::AutofillSpecifics& autofill(
        sync_node.GetAutofillSpecifics());
    int64 sync_id = sync_node.GetId();
    if (autofill.has_value() ||
      (autofill_profile_not_migrated && autofill.has_profile())) {
      autofill_changes_.push_back(AutofillChangeRecord(changes[i].action,
                                                       sync_id, autofill));
    } else {
      NOTREACHED() << "Autofill specifics has no data!";
    }
  }

  StartObserving();
}

void AutofillChangeProcessor::CommitChangesFromSyncModel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!running())
    return;
  StopObserving();

  std::vector<AutofillEntry> new_entries;
  for (unsigned int i = 0; i < autofill_changes_.size(); i++) {
    // Handle deletions.
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        autofill_changes_[i].action_) {
      if (autofill_changes_[i].autofill_.has_value()) {
        ApplySyncAutofillEntryDelete(autofill_changes_[i].autofill_);
      } else if (autofill_changes_[i].autofill_.has_profile()) {
        ApplySyncAutofillProfileDelete(autofill_changes_[i].id_);
      } else {
        NOTREACHED() << "Autofill's CommitChanges received change with no"
            " data!";
      }
      continue;
    }

    // Handle update/adds.
    if (autofill_changes_[i].autofill_.has_value()) {
      ApplySyncAutofillEntryChange(autofill_changes_[i].action_,
                                   autofill_changes_[i].autofill_, &new_entries,
                                   autofill_changes_[i].id_);
    } else if (autofill_changes_[i].autofill_.has_profile()) {
      ApplySyncAutofillProfileChange(autofill_changes_[i].action_,
                                     autofill_changes_[i].autofill_.profile(),
                                     autofill_changes_[i].id_);
    } else {
      NOTREACHED() << "Autofill's CommitChanges received change with no data!";
    }
  }
  autofill_changes_.clear();

  // Make changes
  if (!web_database_->UpdateAutofillEntries(new_entries)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
                                          "Could not update autofill entries.");
    return;
  }

  PostOptimisticRefreshTask();

  StartObserving();
}

void AutofillChangeProcessor::ApplySyncAutofillEntryDelete(
      const sync_pb::AutofillSpecifics& autofill) {
  if (!web_database_->RemoveFormElement(
      UTF8ToUTF16(autofill.name()), UTF8ToUTF16(autofill.value()))) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Could not remove autofill node.");
    return;
  }
}

void AutofillChangeProcessor::ApplySyncAutofillEntryChange(
      sync_api::SyncManager::ChangeRecord::Action action,
      const sync_pb::AutofillSpecifics& autofill,
      std::vector<AutofillEntry>* new_entries,
      int64 sync_id) {
  DCHECK_NE(sync_api::SyncManager::ChangeRecord::ACTION_DELETE, action);

  std::vector<base::Time> timestamps;
  size_t timestamps_size = autofill.usage_timestamp_size();
  for (size_t c = 0; c < timestamps_size; ++c) {
    timestamps.push_back(
        base::Time::FromInternalValue(autofill.usage_timestamp(c)));
  }
  AutofillKey k(UTF8ToUTF16(autofill.name()), UTF8ToUTF16(autofill.value()));
  AutofillEntry new_entry(k, timestamps);

  new_entries->push_back(new_entry);
  std::string tag(AutofillModelAssociator::KeyToTag(k.name(), k.value()));
  if (action == sync_api::SyncManager::ChangeRecord::ACTION_ADD)
    model_associator_->Associate(&tag, sync_id);
}

void AutofillChangeProcessor::ApplySyncAutofillProfileChange(
    sync_api::SyncManager::ChangeRecord::Action action,
    const sync_pb::AutofillProfileSpecifics& profile,
    int64 sync_id) {
  DCHECK_NE(sync_api::SyncManager::ChangeRecord::ACTION_DELETE, action);

  switch (action) {
    case sync_api::SyncManager::ChangeRecord::ACTION_ADD: {
      std::string guid(guid::GenerateGUID());
      scoped_ptr<AutoFillProfile> p(new AutoFillProfile);
      p->set_guid(guid);
      AutofillModelAssociator::FillProfileWithServerData(p.get(),
                                                              profile);
      if (!web_database_->AddAutoFillProfile(*p.get())) {
        NOTREACHED() << "Couldn't add autofill profile: " << guid;
        return;
      }
      model_associator_->Associate(&guid, sync_id);
      break;
    }
    case sync_api::SyncManager::ChangeRecord::ACTION_UPDATE: {
      const std::string* guid = model_associator_->GetChromeNodeFromSyncId(
          sync_id);
      if (guid == NULL) {
        LOG(ERROR) << " Model association has not happened for " << sync_id;
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "model association has not happened");
        return;
      }
      AutoFillProfile *temp_ptr;
      if (!web_database_->GetAutoFillProfile(*guid, &temp_ptr)) {
        LOG(ERROR) << "Autofill profile not found for " << *guid;
        return;
      }

      scoped_ptr<AutoFillProfile> p(temp_ptr);

      AutofillModelAssociator::FillProfileWithServerData(p.get(), profile);
      if (!web_database_->UpdateAutoFillProfile(*(p.get()))) {
        LOG(ERROR) << "Couldn't update autofill profile: " << guid;
        return;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void AutofillChangeProcessor::ApplySyncAutofillProfileDelete(
    int64 sync_id) {

  const std::string *guid = model_associator_->GetChromeNodeFromSyncId(sync_id);
  if (guid == NULL) {
    LOG(ERROR)<< "The profile is not associated";
    return;
  }

  if (!web_database_->RemoveAutoFillProfile(*guid)) {
    LOG(ERROR) << "Could not remove the profile";
    return;
  }

  model_associator_->Disassociate(sync_id);
}

void AutofillChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  observing_ = true;
}

void AutofillChangeProcessor::StopImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observing_ = false;
}


void AutofillChangeProcessor::StartObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  notification_registrar_.Add(this, NotificationType::AUTOFILL_ENTRIES_CHANGED,
                              NotificationService::AllSources());
}

void AutofillChangeProcessor::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  notification_registrar_.RemoveAll();
}

// static
void AutofillChangeProcessor::WriteAutofillEntry(
    const AutofillEntry& entry,
    sync_api::WriteNode* node) {
  sync_pb::AutofillSpecifics autofill;
  autofill.set_name(UTF16ToUTF8(entry.key().name()));
  autofill.set_value(UTF16ToUTF8(entry.key().value()));
  const std::vector<base::Time>& ts(entry.timestamps());
  for (std::vector<base::Time>::const_iterator timestamp = ts.begin();
       timestamp != ts.end(); ++timestamp) {
    autofill.add_usage_timestamp(timestamp->ToInternalValue());
  }
  node->SetAutofillSpecifics(autofill);
}

bool AutofillChangeProcessor::HasNotMigratedYet(
    const sync_api::BaseTransaction* trans) {
  return model_associator_->HasNotMigratedYet(trans);
}

}  // namespace browser_sync
