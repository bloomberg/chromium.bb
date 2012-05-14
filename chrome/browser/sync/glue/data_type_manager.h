// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_H__
#pragma once

#include <list>
#include <set>
#include <string>

#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "sync/internal_api/configure_reason.h"
#include "sync/syncable/model_type.h"

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

  // Update NotifyDone() in data_type_manager_impl.cc if you update
  // this.
  enum ConfigureStatus {
    UNKNOWN = -1,
    OK,                  // Configuration finished without error.
    PARTIAL_SUCCESS,     // Some data types had an error while starting up.
    ABORTED,             // Start was aborted by calling Stop() before
                         // all types were started.
    RETRY,               // Download failed due to a transient error and it
                         // is being retried.
    UNRECOVERABLE_ERROR  // We got an unrecoverable error during startup.
  };

  typedef syncable::ModelTypeSet TypeSet;

  // Note: |errors| is only filled when status is not OK.
  struct ConfigureResult {
    ConfigureResult();
    ConfigureResult(ConfigureStatus status,
                    TypeSet requested_types);
    ConfigureResult(ConfigureStatus status,
                    TypeSet requested_types,
                    const std::list<SyncError>& errors);
    ~ConfigureResult();
    ConfigureStatus status;
    TypeSet requested_types;
    std::list<SyncError> errors;
  };

  virtual ~DataTypeManager() {}

  // Convert a ConfigureStatus to string for debug purposes.
  static std::string ConfigureStatusToString(ConfigureStatus status);

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
  virtual void Configure(TypeSet desired_types,
                         sync_api::ConfigureReason reason) = 0;

  virtual void ConfigureWithoutNigori(TypeSet desired_types,
                                      sync_api::ConfigureReason reason) = 0;

  // Synchronously stops all registered data types.  If called after
  // Configure() is called but before it finishes, it will abort the
  // configure and any data types that have been started will be
  // stopped.
  virtual void Stop() = 0;

  // The current state of the data type manager.
  virtual State state() const = 0;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_H__
