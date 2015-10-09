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
#include "components/sync_driver/directory_data_type_controller.h"
#include "components/sync_driver/shared_change_processor.h"

namespace syncer {
class SyncableService;
struct UserShare;
}

namespace sync_driver {

class SyncClient;

class NonUIDataTypeController : public DirectoryDataTypeController {
 public:
  NonUIDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      SyncClient* sync_client);

  // DataTypeController interface.
  void LoadModels(const ModelLoadCallback& model_load_callback) override;
  void StartAssociating(const StartCallback& start_callback) override;
  void Stop() override;
  syncer::ModelType type() const override = 0;
  syncer::ModelSafeGroup model_safe_group() const override = 0;
  ChangeProcessor* GetChangeProcessor() const override;
  std::string name() const override;
  State state() const override;
  void OnSingleDataTypeUnrecoverableError(
      const syncer::SyncError& error) override;

 protected:
  // For testing only.
  NonUIDataTypeController();
  // DataTypeController is RefCounted.
  ~NonUIDataTypeController() override;

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
      DataTypeController::ConfigureResult start_result,
      const syncer::SyncMergeResult& local_merge_result,
      const syncer::SyncMergeResult& syncer_merge_result);

  // UI thread implementation of StartDone.
  virtual void StartDoneImpl(
      DataTypeController::ConfigureResult start_result,
      DataTypeController::State new_state,
      const syncer::SyncMergeResult& local_merge_result,
      const syncer::SyncMergeResult& syncer_merge_result);

  // Kick off the association process.
  virtual bool StartAssociationAsync();

  // Record association time.
  virtual void RecordAssociationTime(base::TimeDelta time);

  // Record causes of start failure.
  virtual void RecordStartFailure(ConfigureResult result);

  // To allow unit tests to control thread interaction during non-ui startup
  // and shutdown, use a factory method to create the SharedChangeProcessor.
  virtual SharedChangeProcessor* CreateSharedChangeProcessor();

  // If the DTC is waiting for models to load, once the models are
  // loaded the datatype service will call this function on DTC to let
  // us know that it is safe to start associating.
  void OnModelLoaded();

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
  void DisableImpl(const syncer::SyncError& error);

  SyncClient* const sync_client_;

  // UserShare is stored in StartAssociating while on UI thread and
  // passed to SharedChangeProcessor::Connect on the model thread.
  syncer::UserShare* user_share_;

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

  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_H_
