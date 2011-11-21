// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TYPED_URL_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_TYPED_URL_CHANGE_PROCESSOR_H_
#pragma once

#include "chrome/browser/sync/glue/change_processor.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"

class MessageLoop;
class Profile;

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

class UnrecoverableErrorHandler;

// This class is responsible for taking changes from the history backend and
// applying them to the sync_api 'syncable' model, and vice versa. All
// operations and use of this class are from the UI thread.
class TypedUrlChangeProcessor : public ChangeProcessor,
                                public content::NotificationObserver {
 public:
  TypedUrlChangeProcessor(Profile* profile,
                          TypedUrlModelAssociator* model_associator,
                          history::HistoryBackend* history_backend,
                          UnrecoverableErrorHandler* error_handler);
  virtual ~TypedUrlChangeProcessor();

  // content::NotificationObserver implementation.
  // History -> sync_api model change application.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // sync_api model -> WebDataService change application.
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction* trans,
      const sync_api::ImmutableChangeRecordList& changes) OVERRIDE;

  // Commit changes here, after we've released the transaction lock to avoid
  // jank.
  virtual void CommitChangesFromSyncModel() OVERRIDE;

 protected:
  virtual void StartImpl(Profile* profile) OVERRIDE;
  virtual void StopImpl() OVERRIDE;

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
                              sync_api::WriteTransaction* transaction);

  // The profile with which we are associated.
  Profile* profile_;

  // The two models should be associated according to this ModelAssociator.
  TypedUrlModelAssociator* model_associator_;

  // The model we are processing changes from.  This is owned by the
  // WebDataService which is kept alive by our data type controller
  // holding a reference.
  history::HistoryBackend* history_backend_;

  content::NotificationRegistrar notification_registrar_;

  bool observing_;  // True when we should observe notifications.

  MessageLoop* expected_loop_;

  scoped_ptr<content::NotificationService> notification_service_;

  // The set of pending changes that will be written out on the next
  // CommitChangesFromSyncModel() call.
  TypedUrlModelAssociator::TypedUrlVector pending_new_urls_;
  TypedUrlModelAssociator::TypedUrlUpdateVector pending_updated_urls_;
  std::vector<GURL> pending_deleted_urls_;
  TypedUrlModelAssociator::TypedUrlVisitVector pending_new_visits_;
  history::VisitVector pending_deleted_visits_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TYPED_URL_CHANGE_PROCESSOR_H_
