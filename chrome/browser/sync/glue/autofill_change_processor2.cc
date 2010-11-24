// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_change_processor2.h"

#include <string>
#include <vector>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/glue/autofill_model_associator2.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/notification_service.h"

namespace browser_sync {

class AutofillModelAssociator2;
struct AutofillChangeProcessor2::AutofillChangeRecord {
  sync_api::SyncManager::ChangeRecord::Action action_;
  int64 id_;
  sync_pb::AutofillSpecifics autofill_;
  AutofillChangeRecord(sync_api::SyncManager::ChangeRecord::Action action,
                       int64 id, const sync_pb::AutofillSpecifics& autofill)
      : action_(action),
        id_(id),
        autofill_(autofill) { }
};

AutofillChangeProcessor2::AutofillChangeProcessor2(
    AutofillModelAssociator2* model_associator,
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

AutofillChangeProcessor2::~AutofillChangeProcessor2() {}

void AutofillChangeProcessor2::Observe(NotificationType type,
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

  switch (type.value) {
    case NotificationType::AUTOFILL_ENTRIES_CHANGED: {
      AutofillChangeList* changes = Details<AutofillChangeList>(details).ptr();
      ObserveAutofillEntriesChanged(changes, &trans, autofill_root);
      break;
    }
    case NotificationType::AUTOFILL_PROFILE_CHANGED: {
      AutofillProfileChange* change =
          Details<AutofillProfileChange>(details).ptr();
      ObserveAutofillProfileChanged(change, &trans, autofill_root);
      break;
    }
    default:
      NOTREACHED()  << "Invalid NotificationType.";
  }
}

void AutofillChangeProcessor2::ChangeProfileLabelIfAlreadyTaken(
    sync_api::BaseTransaction* trans,
    const string16& pre_update_label,
    AutoFillProfile* profile,
    std::string* tag) {
  DCHECK_EQ(AutofillModelAssociator2::ProfileLabelToTag(profile->Label()),
                                                       *tag);
  sync_api::ReadNode read_node(trans);
  if (!pre_update_label.empty() && pre_update_label == profile->Label())
    return;
  if (read_node.InitByClientTagLookup(syncable::AUTOFILL, *tag)) {
    // Handle the edge case of duplicate labels.
    string16 new_label(AutofillModelAssociator2::MakeUniqueLabel(
        profile->Label(), pre_update_label, trans));
    if (new_label.empty()) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "No unique label; can't move aside");
      return;
    }
    OverrideProfileLabel(new_label, profile, tag);
  }
}

void AutofillChangeProcessor2::OverrideProfileLabel(
    const string16& new_label,
    AutoFillProfile* profile_to_update,
    std::string* tag_to_update) {
  tag_to_update->assign(AutofillModelAssociator2::ProfileLabelToTag(new_label));

  profile_to_update->set_label(new_label);
  if (!web_database_->UpdateAutoFillProfile(*profile_to_update)) {
    std::string err = "Failed to overwrite label for node ";
    err += UTF16ToUTF8(new_label);
    error_handler()->OnUnrecoverableError(FROM_HERE, err);
    return;
  }

  // Notify the PersonalDataManager that it's out of date.
  PostOptimisticRefreshTask();
}

void AutofillChangeProcessor2::PostOptimisticRefreshTask() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      new AutofillModelAssociator2::DoOptimisticRefreshTask(
           personal_data_));
}

void AutofillChangeProcessor2::AddAutofillProfileSyncNode(
    sync_api::WriteTransaction* trans, const sync_api::BaseNode& autofill,
    const std::string& tag, const AutoFillProfile* profile) {
  sync_api::WriteNode sync_node(trans);
  if (!sync_node.InitUniqueByCreation(syncable::AUTOFILL, autofill, tag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Failed to create autofill sync node.");
    return;
  }
  sync_node.SetTitle(UTF8ToWide(tag));

  WriteAutofillProfile(*profile, &sync_node);
  model_associator_->Associate(&tag, sync_node.GetId());
}

void AutofillChangeProcessor2::ObserveAutofillProfileChanged(
    AutofillProfileChange* change, sync_api::WriteTransaction* trans,
    const sync_api::ReadNode& autofill_root) {
  std::string tag(AutofillModelAssociator2::ProfileLabelToTag(change->key()));
  switch (change->type()) {
    case AutofillProfileChange::ADD: {
      scoped_ptr<AutoFillProfile> clone(
          static_cast<AutoFillProfile*>(change->profile()->Clone()));
      DCHECK_EQ(clone->Label(), change->key());
      ChangeProfileLabelIfAlreadyTaken(trans, string16(), clone.get(), &tag);
      AddAutofillProfileSyncNode(trans, autofill_root, tag, clone.get());
      break;
    }
    case AutofillProfileChange::UPDATE: {
      scoped_ptr<AutoFillProfile> clone(
          static_cast<AutoFillProfile*>(change->profile()->Clone()));
      std::string pre_update_tag = AutofillModelAssociator2::ProfileLabelToTag(
          change->pre_update_label());
      DCHECK_EQ(clone->Label(), change->key());
      sync_api::WriteNode sync_node(trans);
      ChangeProfileLabelIfAlreadyTaken(trans, change->pre_update_label(),
                                       clone.get(), &tag);
      if (pre_update_tag != tag) {
        // If the label changes, replace the node instead of updating it.
        RemoveSyncNode(pre_update_tag, trans);
        AddAutofillProfileSyncNode(trans, autofill_root, tag, clone.get());
        return;
      }
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
        WriteAutofillProfile(*clone.get(), &sync_node);
      }
      break;
    }
    case AutofillProfileChange::REMOVE: {
      RemoveSyncNode(tag, trans);
      break;
    }
  }
}

void AutofillChangeProcessor2::ObserveAutofillEntriesChanged(
    AutofillChangeList* changes, sync_api::WriteTransaction* trans,
    const sync_api::ReadNode& autofill_root) {
  for (AutofillChangeList::iterator change = changes->begin();
       change != changes->end(); ++change) {
    switch (change->type()) {
      case AutofillChange::ADD:
        {
          sync_api::WriteNode sync_node(trans);
          std::string tag =
              AutofillModelAssociator2::KeyToTag(change->key().name(),
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
          std::string tag = AutofillModelAssociator2::KeyToTag(
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
        std::string tag = AutofillModelAssociator2::KeyToTag(
            change->key().name(), change->key().value());
        RemoveSyncNode(tag, trans);
        }
        break;
    }
  }
}

void AutofillChangeProcessor2::RemoveSyncNode(const std::string& tag,
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

void AutofillChangeProcessor2::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!running())
    return;
  StopObserving();

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
      if (autofill.has_value() || autofill.has_profile()) {
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
    if (autofill.has_value() || autofill.has_profile()) {
      autofill_changes_.push_back(AutofillChangeRecord(changes[i].action,
                                                       sync_id, autofill));
    } else {
      NOTREACHED() << "Autofill specifics has no data!";
    }
  }

  StartObserving();
}

void AutofillChangeProcessor2::CommitChangesFromSyncModel() {
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
        ApplySyncAutofillProfileDelete(autofill_changes_[i].autofill_.profile(),
                                       autofill_changes_[i].id_);
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

void AutofillChangeProcessor2::ApplySyncAutofillEntryDelete(
      const sync_pb::AutofillSpecifics& autofill) {
  if (!web_database_->RemoveFormElement(
      UTF8ToUTF16(autofill.name()), UTF8ToUTF16(autofill.value()))) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Could not remove autofill node.");
    return;
  }
}

void AutofillChangeProcessor2::ApplySyncAutofillEntryChange(
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
  std::string tag(AutofillModelAssociator2::KeyToTag(k.name(), k.value()));
  if (action == sync_api::SyncManager::ChangeRecord::ACTION_ADD)
    model_associator_->Associate(&tag, sync_id);
}

void AutofillChangeProcessor2::ApplySyncAutofillProfileChange(
    sync_api::SyncManager::ChangeRecord::Action action,
    const sync_pb::AutofillProfileSpecifics& profile,
    int64 sync_id) {
  DCHECK_NE(sync_api::SyncManager::ChangeRecord::ACTION_DELETE, action);

  std::string tag(AutofillModelAssociator2::ProfileLabelToTag(
      UTF8ToUTF16(profile.label())));
  switch (action) {
    case sync_api::SyncManager::ChangeRecord::ACTION_ADD: {
      PersonalDataManager* pdm = model_associator_->sync_service()->
          profile()->GetPersonalDataManager();
      scoped_ptr<AutoFillProfile> p(
          pdm->CreateNewEmptyAutoFillProfileForDBThread(
              UTF8ToUTF16(profile.label())));
      AutofillModelAssociator2::OverwriteProfileWithServerData(p.get(),
                                                              profile);

      model_associator_->Associate(&tag, sync_id);
      if (!web_database_->AddAutoFillProfile(*p.get())) {
        NOTREACHED() << "Couldn't add autofill profile: " << profile.label();
        return;
      }
      break;
    }
    case sync_api::SyncManager::ChangeRecord::ACTION_UPDATE: {
      AutoFillProfile* p = NULL;
      string16 label = UTF8ToUTF16(profile.label());
      if (!web_database_->GetAutoFillProfileForLabel(label, &p)) {
        NOTREACHED() << "Couldn't retrieve autofill profile: " << label;
        return;
      }
      AutofillModelAssociator2::OverwriteProfileWithServerData(p, profile);
      if (!web_database_->UpdateAutoFillProfile(*p)) {
        NOTREACHED() << "Couldn't update autofill profile: " << label;
        return;
      }
      delete p;
      break;
    }
    default:
      NOTREACHED();
  }
}

void AutofillChangeProcessor2::ApplySyncAutofillProfileDelete(
    const sync_pb::AutofillProfileSpecifics& profile,
    int64 sync_id) {
  string16 label(UTF8ToUTF16(profile.label()));
  AutoFillProfile* ptr = NULL;
  bool get_success = web_database_->GetAutoFillProfileForLabel(label, &ptr);
  scoped_ptr<AutoFillProfile> p(ptr);
  if (!get_success) {
    NOTREACHED() << "Couldn't retrieve autofill profile: " << label;
    return;
  }
  if (!web_database_->RemoveAutoFillProfile(p->guid())) {
    NOTREACHED() << "Couldn't remove autofill profile: " << label;
    return;
  }
  model_associator_->Disassociate(sync_id);
}

void AutofillChangeProcessor2::StartImpl(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  observing_ = true;
}

void AutofillChangeProcessor2::StopImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observing_ = false;
}


void AutofillChangeProcessor2::StartObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  notification_registrar_.Add(this, NotificationType::AUTOFILL_ENTRIES_CHANGED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::AUTOFILL_PROFILE_CHANGED,
                              NotificationService::AllSources());
}

void AutofillChangeProcessor2::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  notification_registrar_.RemoveAll();
}

// static
void AutofillChangeProcessor2::WriteAutofillEntry(
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

// static
void AutofillChangeProcessor2::WriteAutofillProfile(
    const AutoFillProfile& profile, sync_api::WriteNode* node) {
  sync_pb::AutofillSpecifics autofill;
  sync_pb::AutofillProfileSpecifics* s(autofill.mutable_profile());
  s->set_label(UTF16ToUTF8(profile.Label()));
  s->set_name_first(UTF16ToUTF8(
      profile.GetFieldText(AutoFillType(NAME_FIRST))));
  s->set_name_middle(UTF16ToUTF8(
      profile.GetFieldText(AutoFillType(NAME_MIDDLE))));
  s->set_name_last(UTF16ToUTF8(profile.GetFieldText(AutoFillType(NAME_LAST))));
  s->set_address_home_line1(
      UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE1))));
  s->set_address_home_line2(
      UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE2))));
  s->set_address_home_city(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(ADDRESS_HOME_CITY))));
  s->set_address_home_state(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(ADDRESS_HOME_STATE))));
  s->set_address_home_country(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(ADDRESS_HOME_COUNTRY))));
  s->set_address_home_zip(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(ADDRESS_HOME_ZIP))));
  s->set_email_address(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(EMAIL_ADDRESS))));
  s->set_company_name(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(COMPANY_NAME))));
  s->set_phone_fax_whole_number(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(PHONE_FAX_WHOLE_NUMBER))));
  s->set_phone_home_whole_number(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(PHONE_HOME_WHOLE_NUMBER))));
  node->SetAutofillSpecifics(autofill);
}

}  // namespace browser_sync

