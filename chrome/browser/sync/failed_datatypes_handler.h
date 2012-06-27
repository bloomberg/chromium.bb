// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FAILED_DATATYPES_HANDLER_H_
#define CHROME_BROWSER_SYNC_FAILED_DATATYPES_HANDLER_H_
#pragma once

#include <string>

#include "chrome/browser/sync/glue/data_type_manager.h"

class ProfileSyncService;

// Class to keep track of data types that have encountered an error during sync.
class FailedDatatypesHandler {
 public:
  enum FailureType {
    // The dataype failed at startup.
    STARTUP,

    // The datatype encountered a runtime error.
    RUNTIME
  };

  explicit FailedDatatypesHandler(ProfileSyncService* service);
  ~FailedDatatypesHandler();

  // Called with the result of sync configuration. The types with errors
  // are obtained from the |result|.
  bool UpdateFailedDatatypes(
      const std::list<csync::SyncError>& errors,
      FailureType failure_type);

  // Called when the user has chosen a new set of datatypes to sync. We clear
  // the current list of failed types and retry them once more.
  void OnUserChoseDatatypes();

  // Returns the types that are failing.
  syncable::ModelTypeSet GetFailedTypes() const;

  // Returns if there are any failed types.
  bool AnyFailedDatatype() const;

  // Gets the error string giving more info about each type that is failing.
  std::string GetErrorString() const;

 private:
  // List of dataypes that failed at startup.
  std::list<csync::SyncError> startup_errors_;

  // List of datatypes that failed at runtime.
  std::list<csync::SyncError> runtime_errors_;

  ProfileSyncService* service_;

  DISALLOW_COPY_AND_ASSIGN(FailedDatatypesHandler);
};
#endif  // CHROME_BROWSER_SYNC_FAILED_DATATYPES_HANDLER_H_
