// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FRONTEND_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_FRONTEND_DATA_TYPE_CONTROLLER_H__

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/sync_driver/data_type_error_handler.h"
#include "components/sync_driver/directory_data_type_controller.h"

namespace base {
class SingleThreadTaskRunner;
class TimeDelta;
}

namespace syncer {
class SyncError;
}

namespace sync_driver {
class AssociatorInterface;
class ChangeProcessor;
class SyncClient;
}

namespace browser_sync {

// Implementation for datatypes that reside on the frontend thread
// (UI thread). This is the same thread we perform initialization on, so we
// don't have to worry about thread safety. The main start/stop funtionality is
// implemented by default.
// Derived classes must implement (at least):
//    syncer::ModelType type() const
//    void CreateSyncComponents();
// NOTE: This class is deprecated! New sync datatypes should be using the
// syncer::SyncableService API and the UIDataTypeController instead.
// TODO(zea): Delete this once all types are on the new API.
class FrontendDataTypeController
    : public sync_driver::DirectoryDataTypeController {
 public:
  FrontendDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      sync_driver::SyncClient* sync_client);

  // DataTypeController interface.
  void LoadModels(const ModelLoadCallback& model_load_callback) override;
  void StartAssociating(const StartCallback& start_callback) override;
  void Stop() override;
  syncer::ModelType type() const override = 0;
  syncer::ModelSafeGroup model_safe_group() const override;
  std::string name() const override;
  State state() const override;

  // DataTypeErrorHandler interface.
  void OnSingleDataTypeUnrecoverableError(
      const syncer::SyncError& error) override;

 protected:
  friend class FrontendDataTypeControllerMock;

  // For testing only.
  FrontendDataTypeController();
  ~FrontendDataTypeController() override;

  // Kick off any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  // Return value:
  //   True - if models are ready and association can proceed.
  //   False - if models are not ready. Associate() should be called when the
  //           models are ready. Refer to Start(_) implementation.
  virtual bool StartModels();

  // Datatype specific creation of sync components.
  virtual void CreateSyncComponents() = 0;

  // Perform any DataType controller specific state cleanup before stopping
  // the datatype controller. The default implementation is a no-op.
  virtual void CleanUpState();

  // Helper method for cleaning up state and running the start callback.
  virtual void StartDone(
      ConfigureResult start_result,
      const syncer::SyncMergeResult& local_merge_result,
      const syncer::SyncMergeResult& syncer_merge_result);

  // Record association time.
  virtual void RecordAssociationTime(base::TimeDelta time);
  // Record causes of start failure.
  virtual void RecordStartFailure(ConfigureResult result);

  virtual sync_driver::AssociatorInterface* model_associator() const;
  virtual void set_model_associator(
      sync_driver::AssociatorInterface* associator);
  sync_driver::ChangeProcessor* GetChangeProcessor() const override;
  virtual void set_change_processor(sync_driver::ChangeProcessor* processor);

  // Handles the reporting of unrecoverable error. It records stuff in
  // UMA and reports to breakpad.
  // Virtual for testing purpose.
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message);

  // If the DTC is waiting for models to load, once the models are
  // loaded the datatype service will call this function on DTC to let
  // us know that it is safe to start associating.
  void OnModelLoaded();

  sync_driver::SyncClient* const sync_client_;

  State state_;

  StartCallback start_callback_;
  ModelLoadCallback model_load_callback_;

  // TODO(sync): transition all datatypes to SyncableService and deprecate
  // AssociatorInterface.
  std::unique_ptr<sync_driver::AssociatorInterface> model_associator_;
  std::unique_ptr<sync_driver::ChangeProcessor> change_processor_;

 private:
  // Build sync components and associate models.
  virtual void Associate();

  void AbortModelLoad();

  // Clean up our state and state variables. Called in response
  // to a failure or abort or stop.
  void CleanUp();

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(FrontendDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_DRIVER_FRONTEND_DATA_TYPE_CONTROLLER_H__
