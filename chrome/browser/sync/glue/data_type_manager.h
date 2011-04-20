// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_H__
#pragma once

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {

// This interface is for managing the start up and shut down life cycle
// of many different syncable data types.
class DataTypeManager {
 public:
  enum State {
    STOPPED,           // No data types are currently running.
    DOWNLOAD_PENDING,  // Not implemented yet: Waiting for the syncer to
                       // complete the initial download of new data
                       // types.

    CONFIGURING,       // Data types are being started.
    BLOCKED,           // We can't move forward with configuration because some
                       // external action must take place (i.e. passphrase).

    CONFIGURED,        // All enabled data types are running.
    STOPPING           // Data types are being stopped.
  };

  enum ConfigureResult {
    OK,                  // Configuration finished without error.
    ASSOCIATION_FAILED,  // An error occurred during model association.
    ABORTED,             // Start was aborted by calling Stop() before
                         // all types were started.
    UNRECOVERABLE_ERROR  // A data type experienced an unrecoverable error
                         // during startup.
  };

  typedef std::set<syncable::ModelType> TypeSet;

  // In case of an error the location is filled with the location the
  // error originated from. In case of a success the error location value
  // is to be not used.
  // TODO(tim): We should rename this / ConfigureResult to something more
  // flexible like SyncConfigureDoneDetails.
  struct ConfigureResultWithErrorLocation {
    ConfigureResult result;
    TypeSet requested_types;
    scoped_ptr<tracked_objects::Location> location;

    ConfigureResultWithErrorLocation();
    ConfigureResultWithErrorLocation(const ConfigureResult& result,
        const tracked_objects::Location& location,
        const TypeSet& requested_types)
        : result(result),
          requested_types(requested_types) {
      this->location.reset(new tracked_objects::Location(
          location.function_name(),
          location.file_name(),
          location.line_number()));
    }

      ~ConfigureResultWithErrorLocation();
  };


  virtual ~DataTypeManager() {}

  // Begins asynchronous configuration of data types.  Any currently
  // running data types that are not in the desired_types set will be
  // stopped.  Any stopped data types that are in the desired_types
  // set will be started.  All other data types are left in their
  // current state.  A SYNC_CONFIGURE_START notification will be sent
  // to the UI thread when configuration is started and a
  // SYNC_CONFIGURE_DONE notification will be sent (with a
  // ConfigureResult detail) when configuration is complete.
  //
  // Note that you may call Configure() while configuration is in
  // progress.  Configuration will be complete only when the
  // desired_types supplied in the last call to Configure is achieved.
  virtual void Configure(const TypeSet& desired_types) = 0;

  // Synchronously stops all registered data types.  If called after
  // Configure() is called but before it finishes, it will abort the
  // configure and any data types that have been started will be
  // stopped.
  virtual void Stop() = 0;

  // Reference to map of data type controllers.
  virtual const DataTypeController::TypeMap& controllers() = 0;

  // The current state of the data type manager.
  virtual State state() = 0;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_H__
