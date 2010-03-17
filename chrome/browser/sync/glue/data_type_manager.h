// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_H__

#include "base/task.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {

// This interface is for managing the start up and shut down life cycle
// of many different syncable data types.
class DataTypeManager {
 public:
  enum State {
    STOPPED,         // No data types are currently running.
    PAUSE_PENDING,   // Waiting for the sync backend to pause.
    STARTING,        // Data types are being started.
    RESUME_PENDING,  // Waiting for the sync backend to resume.
    STARTED,         // All enabled data types are running.
    STOPPING         // Data types are being stopped.
  };

  enum StartResult {
    OK,                 // All enabled data types were started normally.
    BUSY,               // Start() was called while start is already
                        // in progress.
    ASSOCIATION_FAILED, // An error occurred during model association.
    ABORTED,            // Start was aborted by calling Stop() before
                        // all types were started.
    UNRECOVERABLE_ERROR // A data type experienced an unrecoverable error
                        // during startup.
  };

  typedef Callback1<StartResult>::Type StartCallback;

  virtual ~DataTypeManager() {}

  // Begins asynchronous start up of all registered data types.  Upon
  // successful completion or failure, the start_callback will be
  // invoked with the corresponding StartResult.  This is an all or
  // nothing process -- if any of the registered data types fail to
  // start, the startup process will halt and any data types that have
  // been started will be stopped.
  virtual void Start(StartCallback* start_callback) = 0;

  // Synchronously stops all registered data types.  If called after
  // Start() is called but before it finishes, it will abort the start
  // and any data types that have been started will be stopped.
  virtual void Stop() = 0;

  // Returns true if the given data type has been registered.
  virtual bool IsRegistered(syncable::ModelType type) = 0;

  // Returns true if the given data type has been registered and is
  // enabled.
  virtual bool IsEnabled(syncable::ModelType type) = 0;

  // Reference to map of data type controllers.
  virtual const DataTypeController::TypeMap& controllers() = 0;

  // The current state of the data type manager.
  virtual State state() = 0;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_H__
