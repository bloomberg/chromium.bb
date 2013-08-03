// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/recovery_private/write_from_file_operation.h"

namespace extensions {
namespace recovery {

WriteFromFileOperation::WriteFromFileOperation(
    RecoveryOperationManager* manager,
    const ExtensionId& extension_id,
    const std::string& path,
    const std::string& storage_unit_id)
  : RecoveryOperation(manager, extension_id),
    path_(path),
    storage_unit_id_(storage_unit_id) {
}

WriteFromFileOperation::~WriteFromFileOperation() {
}

} // namespace recovery
} // namespace extensions
