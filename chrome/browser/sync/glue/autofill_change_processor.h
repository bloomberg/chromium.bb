// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_CHANGE_PROCESSOR_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class WebDataService;

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

 protected:
  virtual void StartImpl(Profile* profile);
  virtual void StopImpl();

 private:
  bool WriteAutofill(sync_api::WriteNode* node,
                     const string16& name,
                     const string16& value,
                     std::vector<base::Time>& timestamps);

  void StartObserving();
  void StopObserving();

  // The model we are processing changes from. Non-NULL when |running_| is true.
  // We keep a reference to web data service instead of the database because
  // WebDatabase is not refcounted.
  scoped_refptr<WebDataService> web_data_service_;

  // The two models should be associated according to this ModelAssociator.
  AutofillModelAssociator* model_associator_;

  NotificationRegistrar notification_registrar_;

  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(AutofillChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_CHANGE_PROCESSOR_H_
