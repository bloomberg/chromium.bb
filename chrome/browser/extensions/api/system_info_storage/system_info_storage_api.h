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

class SystemInfoStorageAddWatchFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.systemInfo.storage.addWatch",
                             EXPERIMENTAL_SYSTEMINFO_STORAGE_ADDWATCH);
  SystemInfoStorageAddWatchFunction();

 private:
  virtual ~SystemInfoStorageAddWatchFunction();
  virtual bool RunImpl() OVERRIDE;
};

class SystemInfoStorageRemoveWatchFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.systemInfo.storage.removeWatch",
                             EXPERIMENTAL_SYSTEMINFO_STORAGE_REMOVEWATCH);
  SystemInfoStorageRemoveWatchFunction();

 private:
  virtual ~SystemInfoStorageRemoveWatchFunction();
  virtual bool RunImpl() OVERRIDE;
};

class SystemInfoStorageGetAllWatchFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.systemInfo.storage.getAllWatch",
                             EXPERIMENTAL_SYSTEMINFO_STORAGE_GETALLWATCH);
  SystemInfoStorageGetAllWatchFunction();

 private:
  virtual ~SystemInfoStorageGetAllWatchFunction();
  virtual bool RunImpl() OVERRIDE;
};

class SystemInfoStorageRemoveAllWatchFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.systemInfo.storage.removeAllWatch",
                             EXPERIMENTAL_SYSTEMINFO_STORAGE_REMOVEALLWATCH);
  SystemInfoStorageRemoveAllWatchFunction();

 private:
  virtual ~SystemInfoStorageRemoveAllWatchFunction();
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_SYSTEM_INFO_STORAGE_API_H_
