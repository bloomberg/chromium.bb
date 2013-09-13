// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_SYSTEM_STORAGE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_SYSTEM_STORAGE_API_H_

#include "chrome/browser/extensions/api/system_storage/storage_info_provider.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"

namespace extensions {

// Implementation of the systeminfo.storage.get API. It is an asynchronous
// call relative to browser UI thread.
class SystemStorageGetInfoFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.storage.getInfo", SYSTEM_STORAGE_GETINFO);
  SystemStorageGetInfoFunction();

 private:
  virtual ~SystemStorageGetInfoFunction();
  virtual bool RunImpl() OVERRIDE;

  void OnGetStorageInfoCompleted(bool success);
};

class SystemStorageEjectDeviceFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.storage.ejectDevice",
                             SYSTEM_STORAGE_EJECTDEVICE);

 protected:
  virtual ~SystemStorageEjectDeviceFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnStorageMonitorInit(const std::string& transient_device_id);

  // Eject device request handler.
  void HandleResponse(StorageMonitor::EjectStatus status);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_SYSTEM_STORAGE_API_H_
