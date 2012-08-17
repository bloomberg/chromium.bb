// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_SYSTEM_INFO_STORAGE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_SYSTEM_INFO_STORAGE_API_H_

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

// Implementation of the systeminfo.storage.get API. It is an asynchronous
// call relative to browser UI thread.
class SystemInfoStorageGetFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.systemInfo.storage.get");

  SystemInfoStorageGetFunction();

 private:
  virtual ~SystemInfoStorageGetFunction();

  // AsyncExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

  // Called on FILE thread to get storage information
  void WorkOnFileThread();

  // Responds the result back to UI thread
  void RespondOnUIThread(bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_SYSTEM_INFO_STORAGE_API_H_
