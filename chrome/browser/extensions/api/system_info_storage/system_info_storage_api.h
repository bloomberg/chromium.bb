// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_SYSTEM_INFO_STORAGE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_SYSTEM_INFO_STORAGE_API_H_

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

// Implementation of the systeminfo.storage.get API. It is an asynchronous
// call relative to browser UI thread.
class SystemInfoStorageGetFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.systemInfo.storage.get",
                             EXPERIMENTAL_SYSTEMINFO_STORAGE_GET)
  SystemInfoStorageGetFunction();

 private:
  virtual ~SystemInfoStorageGetFunction();
  virtual bool RunImpl() OVERRIDE;

  void OnGetStorageInfoCompleted(const StorageInfo& info, bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_SYSTEM_INFO_STORAGE_API_H_
