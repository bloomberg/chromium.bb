// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_CHANGE_PROCESSOR_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_CHANGE_PROCESSOR_H_

#include "components/sync_driver/change_processor.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/history/core/browser/typed_url_model_associator.h"
#include "components/sync_driver/data_type_error_handler.h"

namespace base {
class MessageLoop;
}

namespace history {
class HistoryBackend;
class URLRow;
};

namespace browser_sync {

class DataTypeErrorHandler;

// This class is responsible for taking changes from the history backend and
// applying them to the sync API 'syncable' model, and vice versa. All
// operations and use of this class are from the UI thread.
class TypedUrlChangeProcessor : public sync_driver::ChangeProcessor,
                                public history::HistoryBackendObserver {
 public:
  TypedUrlChangeProcessor(
      TypedUrlModelAssociator* model_associator,
      history::HistoryBackend* history_backend,
      sync_driver::DataTypeErrorHandler* error_handler,
      const scoped_refptr<base::SingleThreadTaskRunner> ui_thread);
  ~TypedUrlChangeProcessor() override;

  // sync API model -> WebDataService change application.
  void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 model_version,
      const syncer::ImmutableChangeRecordList& changes) override;

  // Commit changes here, after we've released the transaction lock to avoid
  // jank.
  void CommitChangesFromSyncModel() override;

  // Stop processing changes and wait for being destroyed.
  void Disconnect();

 protected:
  void StartImpl() override;

 private:
  friend class ScopedStopObserving<TypedUrlChangeProcessor>;
  void StartObserving();
  void StopObserving();

  // history::HistoryBackendObserver:
  void OnURLVisited(history::HistoryBackend* history_backend,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsModified(history::HistoryBackend* history_backend,
                      const history::URLRows& changed_urls) override;
  void OnURLsDeleted(history::HistoryBackend* history_backend,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Returns true if the caller should sync as a result of the passed visit
  // notification. We use this to throttle the number of sync changes we send
  // to the server so we don't hit the server for every
  // single typed URL visit.
  bool ShouldSyncVisit(int typed_count, ui::PageTransition transition);

  // This struct is used to return result from FixupURLAndGetVisitsToSync
  // function below.
  struct VisitsToSync {
    VisitsToSync();
    ~VisitsToSync();

    history::URLRow row;
    history::VisitVector visits;
  };

  // Utility function that fixes the URL and retrieves visits to be used to
  // update sync node for the given |url|. Returns false if the data can't be
  // retrieved from History backend or if there are no typed visits to sync.
  bool FixupURLAndGetVisitsToSync(const history::URLRow& row,
                                  VisitsToSync* visits_to_sync);

  // Utility routine that either updates an existing sync node or creates a
  // new one for the passed |typed_url| if one does not already exist. Returns
  // false and sets an unrecoverable error if the operation failed.
  void CreateOrUpdateSyncNode(syncer::WriteTransaction* transaction,
                              const history::URLRow& typed_url,
                              const history::VisitVector& visit_vector);

  // The two models should be associated according to this ModelAssociator.
  TypedUrlModelAssociator* model_associator_;

  // The model we are processing changes from.  This is owned by the
  // WebDataService which is kept alive by our data type controller
  // holding a reference.
  history::HistoryBackend* history_backend_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> backend_thread_;

  // The set of pending changes that will be written out on the next
  // CommitChangesFromSyncModel() call.
  history::URLRows pending_new_urls_;
  history::URLRows pending_updated_urls_;
  std::vector<GURL> pending_deleted_urls_;
  TypedUrlModelAssociator::TypedUrlVisitVector pending_new_visits_;
  history::VisitVector pending_deleted_visits_;

  bool disconnected_;
  base::Lock disconnect_lock_;

  ScopedObserver<history::HistoryBackend, history::HistoryBackendObserver>
      history_backend_observer_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlChangeProcessor);
};

}  // namespace browser_sync

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_CHANGE_PROCESSOR_H_
