// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_WRITE_FROM_FILE_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_WRITE_FROM_FILE_OPERATION_H_

#include "chrome/browser/extensions/api/recovery_private/recovery_operation.h"

namespace extensions {
namespace recovery {

// Encapsulates a write of an image from a local file.
class WriteFromFileOperation : public RecoveryOperation {
 public:
  WriteFromFileOperation(RecoveryOperationManager* manager,
                         const ExtensionId& extension_id,
                         const base::FilePath& path,
                         const std::string& storage_unit_id);
  virtual void Start() OVERRIDE;

 private:
  virtual ~WriteFromFileOperation();
  const base::FilePath path_;
};

} // namespace recovery
} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_WRITE_FROM_FILE_OPERATION_H_
