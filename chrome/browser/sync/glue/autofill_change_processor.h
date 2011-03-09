// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_CHANGE_PROCESSOR_H_
#pragma once

#include <vector>

#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class AutofillCreditCardChange;
class AutofillEntry;
class AutofillProfileChange;
class PersonalDataManager;
class WebDatabase;

namespace browser_sync {

class AutofillModelAssociator;
class UnrecoverableErrorHandler;

// This class is responsible for taking changes from the web data service and
// applying them to the sync_api 'syncable' model, and vice versa. All
// operations and use of this class are from the DB thread.
class AutofillChangeProcessor : public ChangeProcessor,
                                public NotificationObserver {
 public:
  AutofillChangeProcessor(AutofillModelAssociator* model_associator,
                          WebDatabase* web_database,
                          PersonalDataManager* personal_data,
                          UnrecoverableErrorHandler* error_handler);
  virtual ~AutofillChangeProcessor();

  // NotificationObserver implementation.
  // WebDataService -> sync_api model change application.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // sync_api model -> WebDataService change application.
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count);

  // Commit any changes from ApplyChangesFromSyncModel buffered in
  // autofill_changes_.
  virtual void CommitChangesFromSyncModel();

  // Copy the properties of the given Autofill entry into the sync
  // node.
  static void WriteAutofillEntry(const AutofillEntry& entry,
                                 sync_api::WriteNode* node);
  // TODO(georgey) : add the same processing for CC info (already in protocol
  // buffers).

 protected:
  virtual void StartImpl(Profile* profile);
  virtual void StopImpl();

 private:
  void StartObserving();
  void StopObserving();

  // A function to remove the sync node for an autofill entry or profile.
  void RemoveSyncNode(const std::string& tag,
                      sync_api::WriteTransaction* trans);

  // These two methods are dispatched to by Observe depending on the type.
  void ObserveAutofillEntriesChanged(AutofillChangeList* changes,
      sync_api::WriteTransaction* trans,
      const sync_api::ReadNode& autofill_root);
  void ObserveAutofillProfileChanged(AutofillProfileChange* change,
      sync_api::WriteTransaction* trans,
      const sync_api::ReadNode& autofill_root);

  // The following methods are the implementation of ApplyChangeFromSyncModel
  // for the respective autofill subtypes.
  void ApplySyncAutofillEntryChange(
      sync_api::SyncManager::ChangeRecord::Action action,
      const sync_pb::AutofillSpecifics& autofill,
      std::vector<AutofillEntry>* new_entries,
      int64 sync_id);
  void ApplySyncAutofillProfileChange(
      sync_api::SyncManager::ChangeRecord::Action action,
      const sync_pb::AutofillProfileSpecifics& profile,
      int64 sync_id);

  // Delete is a special case of change application.
  void ApplySyncAutofillEntryDelete(
      const sync_pb::AutofillSpecifics& autofill);
  void ApplySyncAutofillProfileDelete(
      int64 sync_id);

  // Helper to post a task to the UI loop to inform the PersonalDataManager
  // it needs to refresh itself.
  void PostOptimisticRefreshTask();

  // Called to see if we need to upgrade to the new autofill2 profile.
  bool HasNotMigratedYet(const sync_api::BaseTransaction* trans);

  // The two models should be associated according to this ModelAssociator.
  AutofillModelAssociator* model_associator_;

  // The model we are processing changes from.  This is owned by the
  // WebDataService which is kept alive by our data type controller
  // holding a reference.
  WebDatabase* web_database_;

  // We periodically tell the PersonalDataManager to refresh as we make
  // changes to the autofill data in the WebDatabase.
  PersonalDataManager* personal_data_;

  NotificationRegistrar notification_registrar_;

  bool observing_;

  // Record of changes from ApplyChangesFromSyncModel. These are then processed
  // in CommitChangesFromSyncModel.
  struct AutofillChangeRecord;
  std::vector<AutofillChangeRecord> autofill_changes_;

  DISALLOW_COPY_AND_ASSIGN(AutofillChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_CHANGE_PROCESSOR_H_
