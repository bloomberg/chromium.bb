// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_FAILED_DATA_TYPES_HANDLER_H_
#define CHROME_BROWSER_SYNC_GLUE_FAILED_DATA_TYPES_HANDLER_H_

#include <string>

#include "chrome/browser/sync/glue/data_type_manager.h"

namespace browser_sync {

// Class to keep track of data types that have encountered an error during sync.
class FailedDataTypesHandler {
 public:
  enum FailureType {
    // The dataype failed at startup.
    STARTUP,

    // The datatype encountered a runtime error.
    RUNTIME,

    // The datatype encountered a cryptographer related error.
    CRYPTO
  };
  typedef std::map<syncer::ModelType, syncer::SyncError> TypeErrorMap;

  explicit FailedDataTypesHandler();
  ~FailedDataTypesHandler();

  // Called with the result of sync configuration. The types with errors
  // are obtained from the |result|.
  bool UpdateFailedDataTypes(const TypeErrorMap& errors,
                             FailureType failure_type);

  // Resets the current set of data type errors.
  void Reset();

  // Resets the set of types with cryptographer errors.
  void ResetCryptoErrors();

  // Returns a list of all the errors this class has recorded.
  TypeErrorMap GetAllErrors() const;

  // Returns all types with errors.
  syncer::ModelTypeSet GetFailedTypes() const;

  // Returns the types that are failing due to startup or runtime errors.
  syncer::ModelTypeSet GetFatalErrorTypes() const;

  // Returns the types that are failing due to cryptographer errors.
  syncer::ModelTypeSet GetCryptoErrorTypes() const;

 private:
  // Returns true if there are any types with errors.
  bool AnyFailedDataType() const;

  // List of data types that failed at startup due to association errors.
  TypeErrorMap startup_errors_;

  // List of data types that failed at runtime.
  TypeErrorMap runtime_errors_;

  // List of data types that failed due to the cryptographer not being ready.
  TypeErrorMap crypto_errors_;

  DISALLOW_COPY_AND_ASSIGN(FailedDataTypesHandler);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FAILED_DATA_TYPES_HANDLER_H_
