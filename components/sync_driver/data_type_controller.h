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
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace syncer {
class SyncError;
class SyncMergeResult;
}

namespace sync_driver {
class BackendDataTypeConfigurer;

// Data type controllers need to be refcounted threadsafe, as they may
// need to run model associator or change processor on other threads.
class DataTypeController
    : public base::RefCountedDeleteOnMessageLoop<DataTypeController>,
      public DataTypeErrorHandler {
 public:
  enum State {
    NOT_RUNNING,     // The controller has never been started or has previously
                     // been stopped.  Must be in this state to start.
    MODEL_STARTING,  // The controller is waiting on dependent services
                     // that need to be available before model
                     // association.
    MODEL_LOADED,    // The model has finished loading and can start
                     // associating now.
    ASSOCIATING,     // Model association is in progress.
    RUNNING,         // The controller is running and the data type is
                     // in sync with the cloud.
    STOPPING,        // The controller is in the process of stopping
                     // and is waiting for dependent services to stop.
    DISABLED         // The controller was started but encountered an error
                     // so it is disabled waiting for it to be stopped.
  };

  // This enum is used for "Sync.*ConfigureFailre" histograms so the order
  // of is important. Any changes need to be reflected in histograms.xml.
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
    MAX_CONFIGURE_RESULT
  };

  typedef base::Callback<void(ConfigureResult,
                              const syncer::SyncMergeResult&,
                              const syncer::SyncMergeResult&)> StartCallback;

  typedef base::Callback<void(syncer::ModelType,
                              syncer::SyncError)> ModelLoadCallback;

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

  // Called by DataTypeManager to activate the controlled data type using
  // one of the implementation specific methods provided by the |configurer|.
  // This is called (on UI thread) after the data type configuration has
  // completed successfully.
  virtual void ActivateDataType(BackendDataTypeConfigurer* configurer) = 0;

  // Called by DataTypeManager to deactivate the controlled data type.
  // See comments for ModelAssociationManager::OnSingleDataTypeWillStop.
  virtual void DeactivateDataType(BackendDataTypeConfigurer* configurer) = 0;

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

  // Current state of the data type controller.
  virtual State state() const = 0;

  // Partial implementation of DataTypeErrorHandler.
  // This is thread safe.
  syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message,
      syncer::ModelType type) override;

  // Whether the DataTypeController is ready to start. This is useful if the
  // datatype itself must make the decision about whether it should be enabled
  // at all (and therefore whether the initial download of the sync data for
  // the type should be performed).
  // Returns true by default.
  virtual bool ReadyForStart() const;

 protected:
  friend class base::RefCountedDeleteOnMessageLoop<DataTypeController>;
  friend class base::DeleteHelper<DataTypeController>;

  DataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback);

  ~DataTypeController() override;

  const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread() const {
    return ui_thread_;
  }

  // The callback that will be invoked when an unrecoverable error occurs.
  // TODO(sync): protected for use by legacy controllers.
  base::Closure error_callback_;

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_H__
