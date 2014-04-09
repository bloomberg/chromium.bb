// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/file_system_provider.h"

namespace extensions {

class FileSystemProviderMountFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.mount",
                             FILESYSTEMPROVIDER_MOUNT)

 protected:
  virtual ~FileSystemProviderMountFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemProviderUnmountFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.unmount",
                             FILESYSTEMPROVIDER_UNMOUNT)

 protected:
  virtual ~FileSystemProviderUnmountFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemProviderInternalUnmountRequestedSuccessFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.unmountRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_UNMOUNTREQUESTEDSUCCESS)

 protected:
  virtual ~FileSystemProviderInternalUnmountRequestedSuccessFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemProviderInternalUnmountRequestedErrorFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProviderInternal.unmountRequestedError",
                             FILESYSTEMPROVIDERINTERNAL_UNMOUNTREQUESTEDERROR)

 protected:
  virtual ~FileSystemProviderInternalUnmountRequestedErrorFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
