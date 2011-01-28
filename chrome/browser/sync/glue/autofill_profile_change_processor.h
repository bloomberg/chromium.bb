// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_CHANGE_PROCESSOR_H_
#pragma once
#include <string>
#include <vector>

#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_profile_model_associator.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace browser_sync {

class AutofillProfileChangeProcessor : public ChangeProcessor,
    public NotificationObserver {
 public:
  AutofillProfileChangeProcessor(
      AutofillProfileModelAssociator *model_associator,
      WebDatabase* web_database,
      PersonalDataManager* personal_data_manager,
      UnrecoverableErrorHandler* error_handler);

  virtual ~AutofillProfileChangeProcessor();

  // Virtual methods from ChangeProcessor class.
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction *write_trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count);

  virtual void CommitChangesFromSyncModel();

  // Virtual method implemented for the observer class.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);


  static void WriteAutofillProfile(const AutoFillProfile& profile,
                                   sync_api::WriteNode* node);

 protected:
  // Protected methods from ChangeProcessor.
  virtual void StartImpl(Profile* profile) {}
  virtual void StopImpl() {}

  struct AutofillProfileChangeRecord {
    sync_api::SyncManager::ChangeRecord::Action action_;
    int64 id_;
    sync_pb::AutofillProfileSpecifics profile_specifics_;
    AutofillProfileChangeRecord(
        sync_api::SyncManager::ChangeRecord::Action action,
        int64 id,
        const sync_pb::AutofillProfileSpecifics profile_specifics)
        : action_(action),
          id_(id),
          profile_specifics_(profile_specifics) {}
  };

  std::vector<AutofillProfileChangeRecord> autofill_changes_;

  virtual void AddAutofillProfileSyncNode(sync_api::WriteTransaction* trans,
      sync_api::BaseNode& autofill_profile_root,
      const AutoFillProfile& profile);

  void ActOnChange(AutofillProfileChangeGUID* change,
      sync_api::WriteTransaction* trans,
      sync_api::ReadNode& autofill_root);

 private:

  // This ensures that startobsrving gets called after stopobserving even
  // if there is an early return in the function.
  // TODO(lipalani) - generalize this and add it to other change processors.
  class ScopedStopObserving {
   public:
    explicit ScopedStopObserving(AutofillProfileChangeProcessor* processor);
    ~ScopedStopObserving();

   private:
    ScopedStopObserving() {}
    AutofillProfileChangeProcessor* processor_;
  };

  void StartObserving();
  void StopObserving();

  void PostOptimisticRefreshTask();

  void ApplyAutofillProfileChange(
    sync_api::SyncManager::ChangeRecord::Action action,
    const sync_pb::AutofillProfileSpecifics& profile,
    int64 sync_id);

  void RemoveSyncNode(
      const std::string& guid, sync_api::WriteTransaction *trans);

  AutofillProfileModelAssociator* model_associator_;
  bool observing_;

  WebDatabase* web_database_;
  PersonalDataManager* personal_data_;
  NotificationRegistrar notification_registrar_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_CHANGE_PROCESSOR_H_

