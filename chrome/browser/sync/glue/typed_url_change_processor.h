// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TYPED_URL_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_TYPED_URL_CHANGE_PROCESSOR_H_
#pragma once

#include "chrome/browser/sync/glue/change_processor.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_type.h"

class MessageLoop;
class NotificationService;

namespace history {
class HistoryBackend;
struct URLsDeletedDetails;
struct URLsModifiedDetails;
struct URLVisitedDetails;
class URLRow;
};

namespace browser_sync {

class TypedUrlModelAssociator;
class UnrecoverableErrorHandler;

// This class is responsible for taking changes from the history backend and
// applying them to the sync_api 'syncable' model, and vice versa. All
// operations and use of this class are from the UI thread.
class TypedUrlChangeProcessor : public ChangeProcessor,
                                public NotificationObserver {
 public:
  TypedUrlChangeProcessor(TypedUrlModelAssociator* model_associator,
                          history::HistoryBackend* history_backend,
                          UnrecoverableErrorHandler* error_handler);
  virtual ~TypedUrlChangeProcessor();

  // NotificationObserver implementation.
  // History -> sync_api model change application.
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
  void StartObserving();
  void StopObserving();

  void HandleURLsModified(history::URLsModifiedDetails* details);
  void HandleURLsDeleted(history::URLsDeletedDetails* details);
  void HandleURLsVisited(history::URLVisitedDetails* details);

  // The two models should be associated according to this ModelAssociator.
  TypedUrlModelAssociator* model_associator_;

  // The model we are processing changes from.  This is owned by the
  // WebDataService which is kept alive by our data type controller
  // holding a reference.
  history::HistoryBackend* history_backend_;

  NotificationRegistrar notification_registrar_;

  bool observing_;

  MessageLoop* expected_loop_;

  scoped_ptr<NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TYPED_URL_CHANGE_PROCESSOR_H_
