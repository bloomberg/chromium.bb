// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SHARED_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_DRIVER_SHARED_CHANGE_PROCESSOR_H_

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "components/sync_driver/data_type_error_handler.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"

namespace syncer {
class SyncableService;
struct UserShare;
}  // namespace syncer

namespace sync_driver {

class ChangeProcessor;
class GenericChangeProcessor;
class GenericChangeProcessorFactory;
class DataTypeErrorHandler;
class SyncApiComponentFactory;

// A ref-counted wrapper around a GenericChangeProcessor for use with datatypes
// that don't live on the UI thread.
//
// We need to make it refcounted as the ownership transfer from the
// DataTypeController is dependent on threading, and hence racy. The
// SharedChangeProcessor should be created on the UI thread, but should only be
// connected and used on the same thread as the datatype it interacts with.
//
// The only thread-safe method is Disconnect, which will disconnect from the
// generic change processor, letting us shut down the syncer/datatype without
// waiting for non-UI threads.
//
// Note: since we control the work being done while holding the lock, we ensure
// no I/O or other intensive work is done while blocking the UI thread (all
// the work is in-memory sync interactions).
//
// We use virtual methods so that we can use mock's in testing.
class SharedChangeProcessor
    : public base::RefCountedThreadSafe<SharedChangeProcessor> {
 public:
  // Create an uninitialized SharedChangeProcessor.
  SharedChangeProcessor();

  // Connect to the Syncer and prepare to handle changes for |type|. Will
  // create and store a new GenericChangeProcessor and return a weak pointer to
  // the syncer::SyncableService associated with |type|.
  // Note: If this SharedChangeProcessor has been disconnected, or the
  // syncer::SyncableService was not alive, will return a null weak pointer.
  virtual base::WeakPtr<syncer::SyncableService> Connect(
      SyncApiComponentFactory* sync_factory,
      GenericChangeProcessorFactory* processor_factory,
      syncer::UserShare* user_share,
      DataTypeErrorHandler* error_handler,
      syncer::ModelType type,
      const base::WeakPtr<syncer::SyncMergeResult>& merge_result);

  // Disconnects from the generic change processor. May be called from any
  // thread. After this, all attempts to interact with the change processor by
  // |local_service_| are dropped and return errors. The syncer will be safe to
  // shut down from the point of view of this datatype.
  // Note: Once disconnected, you cannot reconnect without creating a new
  // SharedChangeProcessor.
  // Returns: true if we were previously succesfully connected, false if we were
  // already disconnected.
  virtual bool Disconnect();

  // GenericChangeProcessor stubs (with disconnect support).
  // Should only be called on the same thread the datatype resides.
  virtual int GetSyncCount();
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list);
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const;
  virtual syncer::SyncError GetAllSyncDataReturnError(
      syncer::ModelType type,
      syncer::SyncDataList* data) const;
  virtual syncer::SyncError UpdateDataTypeContext(
      syncer::ModelType type,
      syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
      const std::string& context);
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);
  virtual bool CryptoReadyIfNecessary();

  // If a datatype context associated with the current type exists, fills
  // |context| and returns true. Otheriwse, if there has not been a context
  // set, returns false.
  virtual bool GetDataTypeContext(std::string* context) const;

  virtual syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message);

  ChangeProcessor* generic_change_processor();

 protected:
  friend class base::RefCountedThreadSafe<SharedChangeProcessor>;
  virtual ~SharedChangeProcessor();

 private:
  // Monitor lock for this object. All methods that interact with the change
  // processor must aquire this lock and check whether we're disconnected or
  // not. Once disconnected, all attempted changes to or loads from the change
  // processor return errors. This enables us to shut down the syncer without
  // having to wait for possibly non-UI thread datatypes to complete work.
  mutable base::Lock monitor_lock_;
  bool disconnected_;

  // The sync datatype we were last connected to.
  syncer::ModelType type_;

  // The frontend / UI MessageLoop this object is constructed on. May also be
  // destructed and/or disconnected on this loop, see ~SharedChangeProcessor.
  const scoped_refptr<const base::MessageLoopProxy> frontend_loop_;

  // The loop that all methods except the constructor, destructor, and
  // Disconnect() should be called on.  Set in Connect().
  scoped_refptr<base::MessageLoopProxy> backend_loop_;

  // Used only on |backend_loop_|.
  GenericChangeProcessor* generic_change_processor_;

  DataTypeErrorHandler* error_handler_;

  DISALLOW_COPY_AND_ASSIGN(SharedChangeProcessor);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_SHARED_CHANGE_PROCESSOR_H_
