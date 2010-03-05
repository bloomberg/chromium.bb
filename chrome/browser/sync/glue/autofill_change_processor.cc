// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_change_processor.h"

#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/notification_service.h"

namespace browser_sync {

AutofillChangeProcessor::AutofillChangeProcessor(
    AutofillModelAssociator* model_associator,
    WebDatabase* web_database,
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      model_associator_(model_associator),
      web_database_(web_database),
      observing_(false) {
  DCHECK(model_associator);
  DCHECK(web_database);
  DCHECK(error_handler);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  StartObserving();
}

void AutofillChangeProcessor::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  LOG(INFO) << "Observed autofill change.";
  DCHECK(running());
  DCHECK(NotificationType::AUTOFILL_ENTRIES_CHANGED == type);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  if (!observing_) {
    return;
  }

  AutofillChangeList* changes = Details<AutofillChangeList>(details).ptr();

  sync_api::WriteTransaction trans(share_handle());

  for (AutofillChangeList::iterator change = changes->begin();
       change != changes->end(); ++change) {
    switch (change->type()) {
      case AutofillChange::ADD:
        {
          sync_api::ReadNode autofill_root(&trans);
          if (!autofill_root.InitByTagLookup(kAutofillTag)) {
            error_handler()->OnUnrecoverableError();
            LOG(ERROR) << "Server did not create the top-level autofill node. "
                       << "We might be running against an out-of-date server.";
            return;
          }

          sync_api::WriteNode sync_node(&trans);
          std::string tag =
              AutofillModelAssociator::KeyToTag(change->key().name(),
                                                change->key().value());
          if (!sync_node.InitUniqueByCreation(syncable::AUTOFILL,
                                              autofill_root, tag)) {
            LOG(ERROR) << "Failed to create autofill sync node.";
            error_handler()->OnUnrecoverableError();
            return;
          }

          std::vector<base::Time> timestamps;
          if (!web_database_->GetAutofillTimestamps(
                  change->key().name(),
                  change->key().value(),
                  &timestamps)) {
            LOG(ERROR) << "Failed to get timestamps.";
            error_handler()->OnUnrecoverableError();
            return;
          }

          sync_node.SetTitle(UTF16ToWide(change->key().name() +
                                         change->key().value()));

          WriteAutofill(&sync_node, change->key().name(),
                        change->key().value(), timestamps);

          model_associator_->Associate(&(change->key()), sync_node.GetId());
        }
        break;

      case AutofillChange::UPDATE:
        {
          sync_api::WriteNode sync_node(&trans);
          int64 sync_id =
              model_associator_->GetSyncIdFromChromeId(change->key());
          if (sync_api::kInvalidId == sync_id) {
            LOG(ERROR) << "Unexpected notification for: " <<
                       change->key().name();
            error_handler()->OnUnrecoverableError();
            return;
          } else {
            if (!sync_node.InitByIdLookup(sync_id)) {
              LOG(ERROR) << "Autofill node lookup failed.";
              error_handler()->OnUnrecoverableError();
              return;
            }
          }

          std::vector<base::Time> timestamps;
          if (!web_database_->GetAutofillTimestamps(
                   change->key().name(),
                   change->key().value(),
                   &timestamps)) {
            LOG(ERROR) << "Failed to get timestamps.";
            error_handler()->OnUnrecoverableError();
            return;
          }

          WriteAutofill(&sync_node, change->key().name(),
                        change->key().value(), timestamps);
        }
        break;

      case AutofillChange::REMOVE:
        {
          sync_api::WriteNode sync_node(&trans);
          int64 sync_id =
              model_associator_->GetSyncIdFromChromeId(change->key());
          if (sync_api::kInvalidId == sync_id) {
            LOG(ERROR) << "Unexpected notification for: " <<
                       change->key().name();
            error_handler()->OnUnrecoverableError();
            return;
          } else {
            model_associator_->Disassociate(sync_node.GetId());
            sync_node.Remove();
          }
        }
        break;
    }
  }
}

void AutofillChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  if (!running())
    return;
  StopObserving();

  sync_api::ReadNode autofill_root(trans);
  if (!autofill_root.InitByTagLookup(kAutofillTag)) {
    LOG(ERROR) << "Autofill root node lookup failed.";
    error_handler()->OnUnrecoverableError();
    return;
  }

  std::vector<AutofillEntry> new_entries;
  for (int i = 0; i < change_count; ++i) {

    sync_api::ReadNode sync_node(trans);
    if (!sync_node.InitByIdLookup(changes[i].id)) {
      LOG(ERROR) << "Autofill node lookup failed.";
      error_handler()->OnUnrecoverableError();
      return;
    }

    // Check that the changed node is a child of the autofills folder.
    DCHECK(autofill_root.GetId() == sync_node.GetParentId());
    DCHECK(syncable::AUTOFILL == sync_node.GetModelType());

    const sync_pb::AutofillSpecifics& autofill(
        sync_node.GetAutofillSpecifics());
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      if (!web_database_->RemoveFormElement(
              UTF8ToUTF16(autofill.name()), UTF8ToUTF16(autofill.value()))) {
        LOG(ERROR) << "Could not remove autofill node.";
        error_handler()->OnUnrecoverableError();
        return;
      }
    } else {
      std::vector<base::Time> timestamps;
      size_t timestamps_size = autofill.usage_timestamp_size();
      for (size_t c = 0; c < timestamps_size; ++c) {
        timestamps.push_back(
            base::Time::FromInternalValue(autofill.usage_timestamp(c)));
      }
      AutofillEntry new_entry(AutofillKey(UTF8ToUTF16(autofill.name()),
                                          UTF8ToUTF16(autofill.value())),
                              timestamps);

      new_entries.push_back(new_entry);
    }
  }
  if (!web_database_->UpdateAutofillEntries(new_entries)) {
    LOG(ERROR) << "Could not update autofill entries.";
    error_handler()->OnUnrecoverableError();
    return;
  }
  StartObserving();
}

void AutofillChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  observing_ = true;
}

void AutofillChangeProcessor::StopImpl() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  observing_ = false;
}


void AutofillChangeProcessor::StartObserving() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  notification_registrar_.Add(this, NotificationType::AUTOFILL_ENTRIES_CHANGED,
                              NotificationService::AllSources());
}

void AutofillChangeProcessor::StopObserving() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  notification_registrar_.Remove(this,
                                 NotificationType::AUTOFILL_ENTRIES_CHANGED,
                                 NotificationService::AllSources());
}

// static
void AutofillChangeProcessor::WriteAutofill(
    sync_api::WriteNode* node,
    const string16& name,
    const string16& value,
    const std::vector<base::Time>& timestamps) {
  sync_pb::AutofillSpecifics autofill;
  autofill.set_name(UTF16ToUTF8(name));
  autofill.set_value(UTF16ToUTF8(value));
  for (std::vector<base::Time>::const_iterator timestamp = timestamps.begin();
       timestamp != timestamps.end(); ++timestamp) {
    autofill.add_usage_timestamp(timestamp->ToInternalValue());
  }
  node->SetAutofillSpecifics(autofill);
}

}  // namespace browser_sync
