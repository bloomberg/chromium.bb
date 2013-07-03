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
  typedef std::map<syncer::ModelType, syncer::SyncError> TypeErrorMap;

  explicit FailedDataTypesHandler();
  ~FailedDataTypesHandler();

  // Update the failed datatypes. Types will be added to their corresponding
  // error map based on their |error_type()|.
  bool UpdateFailedDataTypes(const TypeErrorMap& errors);

  // Resets the current set of data type errors.
  void Reset();

  // Resets the set of types with cryptographer errors.
  void ResetCryptoErrors();

  // Resets those persistence errors that intersect with |purged_types|.
  void ResetPersistenceErrorsFrom(syncer::ModelTypeSet purged_types);

  // Returns a list of all the errors this class has recorded.
  TypeErrorMap GetAllErrors() const;

  // Returns all types with errors.
  syncer::ModelTypeSet GetFailedTypes() const;

  // Returns the types that are failing due to startup or runtime errors.
  syncer::ModelTypeSet GetFatalErrorTypes() const;

  // Returns the types that are failing due to cryptographer errors.
  syncer::ModelTypeSet GetCryptoErrorTypes() const;

  // Returns the types that are failing due to persistence errors.
  syncer::ModelTypeSet GetPersistenceErrorTypes() const;

 private:
  // Returns true if there are any types with errors.
  bool AnyFailedDataType() const;

  // List of data types that failed at startup due to association errors or
  // runtime due to data type errors.
  TypeErrorMap fatal_errors_;

  // List of data types that failed due to the cryptographer not being ready.
  TypeErrorMap crypto_errors_;

  // List of data type that failed because sync did not persist the newest
  // version of their data.
  TypeErrorMap persistence_errors_;

  DISALLOW_COPY_AND_ASSIGN(FailedDataTypesHandler);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FAILED_DATA_TYPES_HANDLER_H_
