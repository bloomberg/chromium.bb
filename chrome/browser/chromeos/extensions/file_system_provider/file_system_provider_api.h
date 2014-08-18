// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_

#include "chrome/browser/chromeos/extensions/file_system_provider/provider_function.h"
#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

class FileSystemProviderMountFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.mount",
                             FILESYSTEMPROVIDER_MOUNT)

 protected:
  virtual ~FileSystemProviderMountFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class FileSystemProviderUnmountFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.unmount",
                             FILESYSTEMPROVIDER_UNMOUNT)

 protected:
  virtual ~FileSystemProviderUnmountFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class FileSystemProviderGetAllFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.getAll",
                             FILESYSTEMPROVIDER_GETALL)

 protected:
  virtual ~FileSystemProviderGetAllFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class FileSystemProviderInternalUnmountRequestedSuccessFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.unmountRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_GETMETADATAREQUESTEDSUCCESS)

 protected:
  virtual ~FileSystemProviderInternalUnmountRequestedSuccessFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

class FileSystemProviderInternalGetMetadataRequestedSuccessFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.getMetadataRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_GETMETADATAREQUESTEDSUCCESS)

 protected:
  virtual ~FileSystemProviderInternalGetMetadataRequestedSuccessFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

class FileSystemProviderInternalReadDirectoryRequestedSuccessFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.readDirectoryRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_READDIRECTORYREQUESTEDSUCCESS)

 protected:
  virtual ~FileSystemProviderInternalReadDirectoryRequestedSuccessFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

class FileSystemProviderInternalReadFileRequestedSuccessFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.readFileRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_READFILEREQUESTEDSUCCESS)

 protected:
  virtual ~FileSystemProviderInternalReadFileRequestedSuccessFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

class FileSystemProviderInternalOperationRequestedSuccessFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.operationRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_OPERATIONREQUESTEDSUCCESS)

 protected:
  virtual ~FileSystemProviderInternalOperationRequestedSuccessFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

class FileSystemProviderInternalOperationRequestedErrorFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.operationRequestedError",
      FILESYSTEMPROVIDERINTERNAL_OPERATIONREQUESTEDERROR)

 protected:
  virtual ~FileSystemProviderInternalOperationRequestedErrorFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
