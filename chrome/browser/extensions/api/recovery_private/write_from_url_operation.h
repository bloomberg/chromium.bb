// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_WRITE_FROM_URL_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_WRITE_FROM_URL_OPERATION_H_

#include "chrome/browser/extensions/api/recovery_private/recovery_operation.h"
#include "url/gurl.h"

namespace extensions {
namespace recovery {

// Encapsulates a write of an image accessed via URL.
class WriteFromUrlOperation : public RecoveryOperation {
 public:
  WriteFromUrlOperation(RecoveryOperationManager* manager,
                        const ExtensionId& extension_id,
                        const GURL& url,
                        scoped_ptr<std::string> hash,
                        bool saveImageAsDownload,
                        const std::string& storage_unit_id);
  virtual ~WriteFromUrlOperation();
 private:
  const GURL url_;
  scoped_ptr<std::string> hash_;
  const bool saveImageAsDownload_;
  const std::string storage_unit_id_;
};


} // namespace recovery
} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_WRITE_FROM_URL_OPERATION_H_
