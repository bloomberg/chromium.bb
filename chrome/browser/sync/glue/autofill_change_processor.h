// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_CHANGE_PROCESSOR_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class AutofillCreditCardChange;
class AutofillEntry;
class AutofillProfileChange;
class WebDatabase;

namespace browser_sync {

class AutofillModelAssociator;
class UnrecoverableErrorHandler;

// This class is responsible for taking changes from the web data service and
// applying them to the sync_api 'syncable' model, and vice versa. All
// operations and use of this class are from the UI thread.
class AutofillChangeProcessor : public ChangeProcessor,
                                public NotificationObserver {
 public:
  AutofillChangeProcessor(AutofillModelAssociator* model_associator,
                          WebDatabase* web_database,
                          UnrecoverableErrorHandler* error_handler);
  virtual ~AutofillChangeProcessor() {}

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

  // Copy the properties of the given Autofill entry into the sync
  // node.
  static void WriteAutofill(sync_api::WriteNode* node,
                            const AutofillEntry& entry);

 protected:
  virtual void StartImpl(Profile* profile);
  virtual void StopImpl();

 private:
  void StartObserving();
  void StopObserving();

  void ObserveAutofillEntriesChanged(AutofillChangeList* changes);

  template <class AutofillChangeType, class AssociatorType>
  void RemoveSyncNode(AutofillChangeType* change,
                      AssociatorType* associator,
                      sync_api::WriteTransaction* trans);

  // The two models should be associated according to this ModelAssociator.
  AutofillModelAssociator* model_associator_;

  // The model we are processing changes from.  This is owned by the
  // WebDataService which is kept alive by our data type controller
  // holding a reference.
  WebDatabase* web_database_;

  NotificationRegistrar notification_registrar_;

  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(AutofillChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_CHANGE_PROCESSOR_H_
