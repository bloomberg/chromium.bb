// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PREFERENCE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_PREFERENCE_CHANGE_PROCESSOR_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/pref_service.h"

namespace browser_sync {

class UnrecoverableErrorHandler;

// This class is responsible for taking changes from the PrefService and
// applying them to the sync_api 'syncable' model, and vice versa. All
// operations and use of this class are from the UI thread.
class PreferenceChangeProcessor : public ChangeProcessor,
                                  public NotificationObserver {
 public:
  explicit PreferenceChangeProcessor(UnrecoverableErrorHandler* error_handler);
  virtual ~PreferenceChangeProcessor() {}

  void set_model_associator(PreferenceModelAssociator* model_associator) {
    model_associator_ = model_associator;
  }

  // NotificationObserver implementation.
  // PrefService -> sync_api model change application.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // sync_api model -> PrefService change application.
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count);

 protected:
  virtual void StartImpl(Profile* profile);
  virtual void StopImpl();

 private:
  bool WritePreference(sync_api::WriteNode* node,
                       const std::string& name,
                       const Value* value);
  Value* ReadPreference(sync_api::ReadNode* node, std::string* name);

  void StartObserving();
  void StopObserving();

  // The model we are processing changes from. Non-NULL when |running_| is true.
  PrefService* pref_service_;

  // The two models should be associated according to this ModelAssociator.
  PreferenceModelAssociator* model_associator_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PREFERENCE_CHANGE_PROCESSOR_H_
