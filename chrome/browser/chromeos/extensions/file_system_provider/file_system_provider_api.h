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

class FileSystemProviderInternalUnmountRequestedErrorFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.unmountRequestedError",
      FILESYSTEMPROVIDERINTERNAL_GETMETADATAREQUESTEDERROR)

 protected:
  virtual ~FileSystemProviderInternalUnmountRequestedErrorFunction() {}
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

class FileSystemProviderInternalGetMetadataRequestedErrorFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.getMetadataRequestedError",
      FILESYSTEMPROVIDERINTERNAL_GETMETADATAREQUESTEDERROR)

 protected:
  virtual ~FileSystemProviderInternalGetMetadataRequestedErrorFunction() {}
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

class FileSystemProviderInternalReadDirectoryRequestedErrorFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.readDirectoryRequestedError",
      FILESYSTEMPROVIDERINTERNAL_READDIRECTORYREQUESTEDERROR)

 protected:
  virtual ~FileSystemProviderInternalReadDirectoryRequestedErrorFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

class FileSystemProviderInternalOpenFileRequestedSuccessFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.openFileRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_OPENFILEREQUESTEDSUCCESS)

 protected:
  virtual ~FileSystemProviderInternalOpenFileRequestedSuccessFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

class FileSystemProviderInternalOpenFileRequestedErrorFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.openFileRequestedError",
      FILESYSTEMPROVIDERINTERNAL_OPENFILEREQUESTEDERROR)

 protected:
  virtual ~FileSystemProviderInternalOpenFileRequestedErrorFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

class FileSystemProviderInternalCloseFileRequestedSuccessFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.closeFileRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_CLOSEFILEREQUESTEDSUCCESS)

 protected:
  virtual ~FileSystemProviderInternalCloseFileRequestedSuccessFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

class FileSystemProviderInternalCloseFileRequestedErrorFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.closeFileRequestedError",
      FILESYSTEMPROVIDERINTERNAL_CLOSEFILEREQUESTEDERROR)

 protected:
  virtual ~FileSystemProviderInternalCloseFileRequestedErrorFunction() {}
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

class FileSystemProviderInternalReadFileRequestedErrorFunction
    : public FileSystemProviderInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.readFileRequestedError",
      FILESYSTEMPROVIDERINTERNAL_READFILEREQUESTEDERROR)

 protected:
  virtual ~FileSystemProviderInternalReadFileRequestedErrorFunction() {}
  virtual bool RunWhenValid() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
