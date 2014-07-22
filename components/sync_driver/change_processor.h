// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_DRIVER_CHANGE_PROCESSOR_H_

#include "components/sync_driver/data_type_error_handler.h"
#include "sync/internal_api/public/base_transaction.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/user_share.h"

namespace syncer {
class UnrecoverableErrorHandler;
}  // namespace syncer

namespace sync_driver {

class ModelAssociator;

// An interface used to apply changes from the sync model to the browser's
// native model.  This does not currently distinguish between model data types.
class ChangeProcessor {
 public:
  explicit ChangeProcessor(DataTypeErrorHandler* error_handler);
  virtual ~ChangeProcessor();

  // Call when the processor should accept changes from either provided model
  // and apply them to the other.  Both the native model and sync_api are
  // expected to be initialized and loaded.  You must have set a valid
  // ModelAssociator and UnrecoverableErrorHandler before using this method, and
  // the two models should be associated w.r.t the ModelAssociator provided.
  void Start(syncer::UserShare* share_handle);

  // Changes have been applied to the backend model and are ready to be
  // applied to the frontend model.
  virtual void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 model_version,
      const syncer::ImmutableChangeRecordList& changes) = 0;

  // The changes found in ApplyChangesFromSyncModel may be too slow to be
  // performed while holding a [Read/Write]Transaction lock or may interact
  // with another thread, which might itself be waiting on the transaction lock,
  // putting us at risk of deadlock.
  // This function is called once the transactional lock is released and it is
  // safe to perform inter-thread or slow I/O operations. Note that not all
  // datatypes need this, so we provide an empty default version.
  virtual void CommitChangesFromSyncModel();

  // This ensures that startobserving gets called after stopobserving even
  // if there is an early return in the function.
  template <class T>
  class ScopedStopObserving {
   public:
    explicit ScopedStopObserving(T* processor)
        : processor_(processor) {
      processor_->StopObserving();
    }
    ~ScopedStopObserving() {
      processor_->StartObserving();
    }

   private:
    ScopedStopObserving() {}
    T* processor_;
  };

 protected:
  // These methods are invoked by Start() and Stop() to do
  // implementation-specific work.
  virtual void StartImpl() = 0;

  DataTypeErrorHandler* error_handler() const;
  virtual syncer::UserShare* share_handle() const;

 private:
  DataTypeErrorHandler* error_handler_;  // Guaranteed to outlive us.

  // The sync model we are processing changes from.
  syncer::UserShare* share_handle_;

  DISALLOW_COPY_AND_ASSIGN(ChangeProcessor);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_CHANGE_PROCESSOR_H_
