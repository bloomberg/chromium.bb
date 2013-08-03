// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/recovery_private/write_from_url_operation.h"

namespace extensions {
namespace recovery {

WriteFromUrlOperation::WriteFromUrlOperation(RecoveryOperationManager* manager,
                                             const ExtensionId& extension_id,
                                             const GURL& url,
                                             scoped_ptr<std::string> hash,
                                             bool saveImageAsDownload,
                                             const std::string& storage_unit_id)
  : RecoveryOperation(manager, extension_id),
    url_(url),
    hash_(hash.Pass()),
    saveImageAsDownload_(saveImageAsDownload),
    storage_unit_id_(storage_unit_id) {
  // "use" this field so that clang does not complain at this time.
  (void)saveImageAsDownload_;
}

WriteFromUrlOperation::~WriteFromUrlOperation() {
}

} // namespace recovery
} // namespace extensions
