// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TYPED_URL_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_TYPED_URL_CHANGE_PROCESSOR_H_

#include "components/sync_driver/change_processor.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "components/sync_driver/data_type_error_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"

class Profile;

namespace base {
class MessageLoop;
}

namespace content {
class NotificationService;
}

namespace history {
class HistoryBackend;
struct URLsDeletedDetails;
struct URLsModifiedDetails;
struct URLVisitedDetails;
class URLRow;
};

namespace browser_sync {

class DataTypeErrorHandler;

// This class is responsible for taking changes from the history backend and
// applying them to the sync API 'syncable' model, and vice versa. All
// operations and use of this class are from the UI thread.
class TypedUrlChangeProcessor : public sync_driver::ChangeProcessor,
                                public content::NotificationObserver {
 public:
  TypedUrlChangeProcessor(Profile* profile,
                          TypedUrlModelAssociator* model_associator,
                          history::HistoryBackend* history_backend,
                          sync_driver::DataTypeErrorHandler* error_handler);
  virtual ~TypedUrlChangeProcessor();

  // content::NotificationObserver implementation.
  // History -> sync API change application.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // sync API model -> WebDataService change application.
  virtual void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 model_version,
      const syncer::ImmutableChangeRecordList& changes) OVERRIDE;

  // Commit changes here, after we've released the transaction lock to avoid
  // jank.
  virtual void CommitChangesFromSyncModel() OVERRIDE;

  // Stop processing changes and wait for being destroyed.
  void Disconnect();

 protected:
  virtual void StartImpl() OVERRIDE;

 private:
  friend class ScopedStopObserving<TypedUrlChangeProcessor>;
  void StartObserving();
  void StopObserving();

  void HandleURLsModified(history::URLsModifiedDetails* details);
  void HandleURLsDeleted(history::URLsDeletedDetails* details);
  void HandleURLsVisited(history::URLVisitedDetails* details);

  // Returns true if the caller should sync as a result of the passed visit
  // notification. We use this to throttle the number of sync changes we send
  // to the server so we don't hit the server for every
  // single typed URL visit.
  bool ShouldSyncVisit(history::URLVisitedDetails* details);

  // Utility routine that either updates an existing sync node or creates a
  // new one for the passed |typed_url| if one does not already exist. Returns
  // false and sets an unrecoverable error if the operation failed.
  bool CreateOrUpdateSyncNode(history::URLRow typed_url,
                              syncer::WriteTransaction* transaction);

  // The profile with which we are associated.
  Profile* profile_;

  // The two models should be associated according to this ModelAssociator.
  TypedUrlModelAssociator* model_associator_;

  // The model we are processing changes from.  This is owned by the
  // WebDataService which is kept alive by our data type controller
  // holding a reference.
  history::HistoryBackend* history_backend_;
  base::MessageLoop* backend_loop_;

  content::NotificationRegistrar notification_registrar_;

  scoped_ptr<content::NotificationService> notification_service_;

  // The set of pending changes that will be written out on the next
  // CommitChangesFromSyncModel() call.
  history::URLRows pending_new_urls_;
  history::URLRows pending_updated_urls_;
  std::vector<GURL> pending_deleted_urls_;
  TypedUrlModelAssociator::TypedUrlVisitVector pending_new_visits_;
  history::VisitVector pending_deleted_visits_;

  bool disconnected_;
  base::Lock disconnect_lock_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TYPED_URL_CHANGE_PROCESSOR_H_
