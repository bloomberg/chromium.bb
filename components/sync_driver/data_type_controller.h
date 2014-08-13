// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_H__

#include <map>
#include <string>

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/sequenced_task_runner_helpers.h"
#include "components/sync_driver/data_type_error_handler.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"

namespace syncer {
class SyncError;
struct UserShare;
}

namespace sync_driver {

class ChangeProcessor;

// Data type controllers need to be refcounted threadsafe, as they may
// need to run model associator or change processor on other threads.
class DataTypeController
    : public base::RefCountedDeleteOnMessageLoop<DataTypeController>,
      public DataTypeErrorHandler {
 public:
  enum State {
    NOT_RUNNING,    // The controller has never been started or has
                    // previously been stopped.  Must be in this state to start.
    MODEL_STARTING, // The controller is waiting on dependent services
                    // that need to be available before model
                    // association.
    MODEL_LOADED,   // The model has finished loading and can start
                    // associating now.
    ASSOCIATING,    // Model association is in progress.
    RUNNING,        // The controller is running and the data type is
                    // in sync with the cloud.
    STOPPING,       // The controller is in the process of stopping
                    // and is waiting for dependent services to stop.
    DISABLED        // The controller was started but encountered an error
                    // so it is disabled waiting for it to be stopped.
  };

  enum ConfigureResult {
    OK,                   // The data type has started normally.
    OK_FIRST_RUN,         // Same as OK, but sent on first successful
                          // start for this type for this user as
                          // determined by cloud state.
    ASSOCIATION_FAILED,   // An error occurred during model association.
    ABORTED,              // Start was aborted by calling Stop().
    UNRECOVERABLE_ERROR,  // An unrecoverable error occured.
    NEEDS_CRYPTO,         // The data type cannot be started yet because it
                          // depends on the cryptographer.
    RUNTIME_ERROR,        // After starting, a runtime error was encountered.
    MAX_START_RESULT
  };

  typedef base::Callback<void(ConfigureResult,
                              const syncer::SyncMergeResult&,
                              const syncer::SyncMergeResult&)> StartCallback;

  typedef base::Callback<void(syncer::ModelType,
                              syncer::SyncError)> ModelLoadCallback;

  typedef base::Callback<void(const tracked_objects::Location& location,
                              const std::string&)> DisableTypeCallback;

  typedef std::map<syncer::ModelType,
                   scoped_refptr<DataTypeController> > TypeMap;
  typedef std::map<syncer::ModelType, DataTypeController::State> StateMap;

  // Returns true if the start result should trigger an unrecoverable error.
  // Public so unit tests can use this function as well.
  static bool IsUnrecoverableResult(ConfigureResult result);

  // Returns true if the datatype started successfully.
  static bool IsSuccessfulResult(ConfigureResult result);

  // Begins asynchronous operation of loading the model to get it ready for
  // model association. Once the models are loaded the callback will be invoked
  // with the result. If the models are already loaded it is safe to call the
  // callback right away. Else the callback needs to be stored and called when
  // the models are ready.
  virtual void LoadModels(const ModelLoadCallback& model_load_callback) = 0;

  // Will start a potentially asynchronous operation to perform the
  // model association. Once the model association is done the callback will
  // be invoked.
  virtual void StartAssociating(const StartCallback& start_callback) = 0;

  // Synchronously stops the data type. If StartAssociating has already been
  // called but is not done yet it will be aborted. Similarly if LoadModels
  // has not completed it will also be aborted.
  // NOTE: Stop() should be called after sync backend machinery has stopped
  // routing changes to this data type. Stop() should ensure the data type
  // logic shuts down gracefully by flushing remaining changes and calling
  // StopSyncing on the SyncableService. This assumes no changes will ever
  // propagate from sync again from point where Stop() is called.
  virtual void Stop() = 0;

  // Unique model type for this data type controller.
  virtual syncer::ModelType type() const = 0;

  // Name of this data type.  For logging purposes only.
  virtual std::string name() const = 0;

  // The model safe group of this data type.  This should reflect the
  // thread that should be used to modify the data type's native
  // model.
  virtual syncer::ModelSafeGroup model_safe_group() const = 0;

  // Access to the ChangeProcessor for the type being controlled by |this|.
  // Returns NULL if the ChangeProcessor isn't created or connected.
  virtual ChangeProcessor* GetChangeProcessor() const = 0;

  // Current state of the data type controller.
  virtual State state() const = 0;

  // Partial implementation of DataTypeErrorHandler.
  // This is thread safe.
  virtual syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message,
      syncer::ModelType type) OVERRIDE;

  // Called when the sync backend has initialized. |share| is the
  // UserShare handle to associate model data with.
  void OnUserShareReady(syncer::UserShare* share);

  // Whether the DataTypeController is ready to start. This is useful if the
  // datatype itself must make the decision about whether it should be enabled
  // at all (and therefore whether the initial download of the sync data for
  // the type should be performed).
  // Returns true by default.
  virtual bool ReadyForStart() const;

 protected:
  friend class base::RefCountedDeleteOnMessageLoop<DataTypeController>;
  friend class base::DeleteHelper<DataTypeController>;

  DataTypeController(scoped_refptr<base::MessageLoopProxy> ui_thread,
                     const base::Closure& error_callback);

  // If the DTC is waiting for models to load, once the models are
  // loaded the datatype service will call this function on DTC to let
  // us know that it is safe to start associating.
  virtual void OnModelLoaded() = 0;

  virtual ~DataTypeController();

  syncer::UserShare* user_share() const;

  // The callback that will be invoked when an unrecoverable error occurs.
  // TODO(sync): protected for use by legacy controllers.
  base::Closure error_callback_;

 private:
  syncer::UserShare* user_share_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_H__
