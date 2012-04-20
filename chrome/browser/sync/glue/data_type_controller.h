// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <map>
#include <string>

#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop_helpers.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "content/public/browser/browser_thread.h"
#include "sync/engine/model_safe_worker.h"
#include "sync/syncable/model_type.h"
#include "sync/util/unrecoverable_error_handler.h"

class SyncError;

namespace browser_sync {

// Data type controllers need to be refcounted threadsafe, as they may
// need to run model associator or change processor on other threads.
class DataTypeController
    : public base::RefCountedThreadSafe<
          DataTypeController, content::BrowserThread::DeleteOnUIThread>,
      public DataTypeErrorHandler {
 public:
  enum State {
    NOT_RUNNING,    // The controller has never been started or has
                    // previously been stopped.  Must be in this state to start.
    MODEL_STARTING, // The controller is waiting on dependent services
                    // that need to be available before model
                    // association.
    ASSOCIATING,    // Model association is in progress.
    RUNNING,        // The controller is running and the data type is
                    // in sync with the cloud.
    STOPPING,       // The controller is in the process of stopping
                    // and is waiting for dependent services to stop.
    DISABLED        // The controller was started but encountered an error
                    // so it is disabled waiting for it to be stopped.
  };

  enum StartResult {
    OK,                   // The data type has started normally.
    OK_FIRST_RUN,         // Same as OK, but sent on first successful
                          // start for this type for this user as
                          // determined by cloud state.
    BUSY,                 // Start() was called while already in progress.
    NOT_ENABLED,          // This data type is not enabled for the current user.
    ASSOCIATION_FAILED,   // An error occurred during model association.
    ABORTED,              // Start was aborted by calling Stop().
    UNRECOVERABLE_ERROR,  // An unrecoverable error occured.
    NEEDS_CRYPTO,         // The data type cannot be started yet because it
                          // depends on the cryptographer.
    MAX_START_RESULT
  };

  typedef base::Callback<void(StartResult, const SyncError&)> StartCallback;

  typedef std::map<syncable::ModelType,
                   scoped_refptr<DataTypeController> > TypeMap;
  typedef std::map<syncable::ModelType, DataTypeController::State> StateMap;

  // Returns true if the start result should trigger an unrecoverable error.
  // Public so unit tests can use this function as well.
  static bool IsUnrecoverableResult(StartResult result);

  // Begins asynchronous start up of this data type.  Start up will
  // wait for all other dependent services to be available, then
  // proceed with model association and then change processor
  // activation.  Upon completion, the start_callback will be invoked
  // on the UI thread.  See the StartResult enum above for details on the
  // possible start results.
  virtual void Start(const StartCallback& start_callback) = 0;

  // Synchronously stops the data type.  If called after Start() is
  // called but before the start callback is called, the start is
  // aborted and the start callback is invoked with the ABORTED start
  // result.
  virtual void Stop() = 0;

  // Unique model type for this data type controller.
  virtual syncable::ModelType type() const = 0;

  // Name of this data type.  For logging purposes only.
  virtual std::string name() const = 0;

  // The model safe group of this data type.  This should reflect the
  // thread that should be used to modify the data type's native
  // model.
  virtual browser_sync::ModelSafeGroup model_safe_group() const = 0;

  // Current state of the data type controller.
  virtual State state() const = 0;

  // Partial implementation of DataTypeErrorHandler.
  // This is thread safe.
  virtual SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message,
      syncable::ModelType type) OVERRIDE;

 protected:
  // Handles the reporting of unrecoverable error. It records stuff in
  // UMA and reports to breakpad.
  // Virtual for testing purpose.
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message);

  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<DataTypeController>;

  virtual ~DataTypeController() {}
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_CONTROLLER_H__
