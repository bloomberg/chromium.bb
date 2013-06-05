// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_ENCRYPTION_HANDLER_H_
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_ENCRYPTION_HANDLER_H_

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type.h"

namespace browser_sync {

// The DataTypeEncryptionHandler provides the status of datatype encryption.
class DataTypeEncryptionHandler {
 public:
  DataTypeEncryptionHandler();
  virtual ~DataTypeEncryptionHandler();

  // Returns true if a passphrase is required for encryption to proceed, false
  // otherwise.
  virtual bool IsPassphraseRequired() const = 0;

  // Returns the current set of encrypted data types.
  virtual syncer::ModelTypeSet GetEncryptedDataTypes() const = 0;
};

} // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_ENCRYPTION_HANDLER_H_
