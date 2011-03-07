// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <map>

#include "base/callback.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

// Data type controllers need to be refcounted threadsafe, as they may
// need to run model associator or change processor on other threads.
class DataTypeController
    : public base::RefCountedThreadSafe<DataTypeController,
                                        BrowserThread::DeleteOnUIThread>,
      public UnrecoverableErrorHandler {
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
    STOPPING        // The controller is in the process of stopping
                    // and is waiting for dependent services to stop.
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

  typedef Callback1<StartResult>::Type StartCallback;

  typedef std::map<syncable::ModelType,
                   scoped_refptr<DataTypeController> > TypeMap;
  typedef std::map<syncable::ModelType, DataTypeController::State> StateMap;

  // Begins asynchronous start up of this data type.  Start up will
  // wait for all other dependent services to be available, then
  // proceed with model association and then change processor
  // activation.  Upon completion, the start_callback will be invoked
  // on the UI thread.  See the StartResult enum above for details on the
  // possible start results.
  virtual void Start(StartCallback* start_callback) = 0;

  // Synchronously stops the data type.  If called after Start() is
  // called but before the start callback is called, the start is
  // aborted and the start callback is invoked with the ABORTED start
  // result.
  virtual void Stop() = 0;

  // Returns true if the user has indicated that they want this data
  // type to be enabled.
  virtual bool enabled() = 0;

  // Unique model type for this data type controller.
  virtual syncable::ModelType type() = 0;

  // Name of this data type.  For logging purposes only.
  virtual const char* name() const = 0;

  // The model safe group of this data type.  This should reflect the
  // thread that should be used to modify the data type's native
  // model.
  virtual browser_sync::ModelSafeGroup model_safe_group() = 0;

  // Current state of the data type controller.
  virtual State state() = 0;

 protected:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<DataTypeController>;
  friend class ShutdownTask;

  virtual ~DataTypeController() {}
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_CONTROLLER_H__
