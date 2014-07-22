// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/shared_change_processor.h"

namespace syncer {
class SyncableService;
}

namespace sync_driver {

class SyncApiComponentFactory;

class NonUIDataTypeController : public DataTypeController {
 public:
  NonUIDataTypeController(
      scoped_refptr<base::MessageLoopProxy> ui_thread,
      const base::Closure& error_callback,
      const DisableTypeCallback& disable_callback,
      SyncApiComponentFactory* sync_factory);

  // DataTypeController interface.
  virtual void LoadModels(
      const ModelLoadCallback& model_load_callback) OVERRIDE;
  virtual void StartAssociating(const StartCallback& start_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual syncer::ModelType type() const = 0;
  virtual syncer::ModelSafeGroup model_safe_group() const = 0;
  virtual ChangeProcessor* GetChangeProcessor() const OVERRIDE;
  virtual std::string name() const OVERRIDE;
  virtual State state() const OVERRIDE;
  virtual void OnSingleDatatypeUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

 protected:
  // For testing only.
  NonUIDataTypeController();
  // DataTypeController is RefCounted.
  virtual ~NonUIDataTypeController();

  // DataTypeController interface.
  virtual void OnModelLoaded() OVERRIDE;

  // Start any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  // Return value:
  //   True - if models are ready and association can proceed.
  //   False - if models are not ready. StartAssociationAsync should be called
  //           when the models are ready.
  // Note: this is performed on the UI thread.
  virtual bool StartModels();

  // Perform any DataType controller specific state cleanup before stopping
  // the datatype controller. The default implementation is a no-op.
  // Note: this is performed on the UI thread.
  virtual void StopModels();

  // Posts the given task to the backend thread, i.e. the thread the
  // datatype lives on.  Return value: True if task posted successfully,
  // false otherwise.
  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) = 0;

  // Start up complete, update the state and invoke the callback.
  // Note: this is performed on the datatype's thread.
  virtual void StartDone(
      DataTypeController::StartResult start_result,
      const syncer::SyncMergeResult& local_merge_result,
      const syncer::SyncMergeResult& syncer_merge_result);

  // UI thread implementation of StartDone.
  virtual void StartDoneImpl(
      DataTypeController::StartResult start_result,
      DataTypeController::State new_state,
      const syncer::SyncMergeResult& local_merge_result,
      const syncer::SyncMergeResult& syncer_merge_result);

  // Kick off the association process.
  virtual bool StartAssociationAsync();

  // Record association time.
  virtual void RecordAssociationTime(base::TimeDelta time);

  // Record causes of start failure.
  virtual void RecordStartFailure(StartResult result);

  // To allow unit tests to control thread interaction during non-ui startup
  // and shutdown, use a factory method to create the SharedChangeProcessor.
  virtual SharedChangeProcessor* CreateSharedChangeProcessor();

 private:

  // Posted on the backend thread by StartAssociationAsync().
  void StartAssociationWithSharedChangeProcessor(
      const scoped_refptr<SharedChangeProcessor>& shared_change_processor);

  // Calls Disconnect() on |shared_change_processor_|, then sets it to
  // NULL.  Must be called only by StartDoneImpl() or Stop() (on the
  // UI thread) and only after a call to Start() (i.e.,
  // |shared_change_processor_| must be non-NULL).
  void ClearSharedChangeProcessor();

  // Posts StopLocalService() to the datatype's thread.
  void StopLocalServiceAsync();

  // Calls local_service_->StopSyncing() and releases our references to it.
  void StopLocalService();

  // Abort model loading and trigger the model load callback.
  void AbortModelLoad();

  // Disable this type with the sync service. Should only be invoked in case of
  // an unrecoverable error.
  // Note: this is performed on the UI thread.
  void DisableImpl(const tracked_objects::Location& from_here,
                   const std::string& message);

  SyncApiComponentFactory* const sync_factory_;

  // State of this datatype controller.
  State state_;

  // Callbacks for use when starting the datatype.
  StartCallback start_callback_;
  ModelLoadCallback model_load_callback_;

  // The shared change processor is the thread-safe interface to the
  // datatype.  We hold a reference to it from the UI thread so that
  // we can call Disconnect() on it from Stop()/StartDoneImpl().  Most
  // of the work is done on the backend thread, and in
  // StartAssociationWithSharedChangeProcessor() for this class in
  // particular.
  //
  // Lifetime: The SharedChangeProcessor object is created on the UI
  // thread and passed on to the backend thread.  This reference is
  // released on the UI thread in Stop()/StartDoneImpl(), but the
  // backend thread may still have references to it (which is okay,
  // since we call Disconnect() before releasing the UI thread
  // reference).
  scoped_refptr<SharedChangeProcessor> shared_change_processor_;

  // A weak pointer to the actual local syncable service, which performs all the
  // real work. We do not own the object, and it is only safe to access on the
  // DataType's thread.
  // Lifetime: it gets set in StartAssociationWithSharedChangeProcessor(...)
  // and released in StopLocalService().
  base::WeakPtr<syncer::SyncableService> local_service_;

  scoped_refptr<base::MessageLoopProxy> ui_thread_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_H_
