// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides file system related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_

#include <string>

#include "base/platform_file.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"

class GURL;

namespace base {
class FilePath;
}

namespace fileapi {
class FileSystemContext;
}

namespace extensions {

// Implements the chrome.fileBrowserPrivate.requestFileSystem method.
class FileBrowserPrivateRequestFileSystemFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.requestFileSystem",
                             FILEBROWSERPRIVATE_REQUESTFILESYSTEM)

  FileBrowserPrivateRequestFileSystemFunction();

 protected:
  virtual ~FileBrowserPrivateRequestFileSystemFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  void RespondSuccessOnUIThread(const std::string& name,
                                const GURL& root_url);
  void RespondFailedOnUIThread(base::PlatformFileError error_code);

  // Called when FileSystemContext::OpenFileSystem() is done.
  void DidOpenFileSystem(
      scoped_refptr<fileapi::FileSystemContext> file_system_context,
      base::PlatformFileError result,
      const std::string& name,
      const GURL& root_url);

  // Called when something goes wrong. Records the error to |error_| per the
  // error code and reports that the private API function failed.
  void DidFail(base::PlatformFileError error_code);

  // Sets up file system access permissions to the extension identified by
  // |child_id|.
  bool SetupFileSystemAccessPermissions(
      scoped_refptr<fileapi::FileSystemContext> file_system_context,
      int child_id,
      scoped_refptr<const extensions::Extension> extension);
};

// Base class for FileBrowserPrivateAddFileWatchFunction and
// FileBrowserPrivateRemoveFileWatchFunction. Although it's called "FileWatch",
// the class and its sub classes are used only for watching changes in
// directories.
class FileWatchFunctionBase : public LoggedAsyncExtensionFunction {
 public:
  FileWatchFunctionBase();

 protected:
  virtual ~FileWatchFunctionBase();

  // Performs a file watch operation (ex. adds or removes a file watch).
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) = 0;

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Calls SendResponse() with |success| converted to base::Value.
  void Respond(bool success);
};

// Implements the chrome.fileBrowserPrivate.addFileWatch method.
// Starts watching changes in directories.
class FileBrowserPrivateAddFileWatchFunction : public FileWatchFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.addFileWatch",
                             FILEBROWSERPRIVATE_ADDFILEWATCH)

  FileBrowserPrivateAddFileWatchFunction();

 protected:
  virtual ~FileBrowserPrivateAddFileWatchFunction();

  // FileWatchFunctionBase override.
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};


// Implements the chrome.fileBrowserPrivate.removeFileWatch method.
// Stops watching changes in directories.
class FileBrowserPrivateRemoveFileWatchFunction : public FileWatchFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.removeFileWatch",
                             FILEBROWSERPRIVATE_REMOVEFILEWATCH)

  FileBrowserPrivateRemoveFileWatchFunction();

 protected:
  virtual ~FileBrowserPrivateRemoveFileWatchFunction();

  // FileWatchFunctionBase override.
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.setLastModified method.
// Sets last modified date in seconds of local file
class FileBrowserPrivateSetLastModifiedFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setLastModified",
                             FILEBROWSERPRIVATE_SETLASTMODIFIED)

  FileBrowserPrivateSetLastModifiedFunction();

 protected:
  virtual ~FileBrowserPrivateSetLastModifiedFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.getSizeStats method.
class FileBrowserPrivateGetSizeStatsFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getSizeStats",
                             FILEBROWSERPRIVATE_GETSIZESTATS)

  FileBrowserPrivateGetSizeStatsFunction();

 protected:
  virtual ~FileBrowserPrivateGetSizeStatsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetDriveAvailableSpaceCallback(drive::FileError error,
                                      int64 bytes_total,
                                      int64 bytes_used);

  void GetSizeStatsCallback(const uint64* total_size,
                            const uint64* remaining_size);
};

// Implements the chrome.fileBrowserPrivate.getVolumeMetadata method.
// Retrieves devices meta-data. Expects volume's device path as an argument.
class FileBrowserPrivateGetVolumeMetadataFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getVolumeMetadata",
                             FILEBROWSERPRIVATE_GETVOLUMEMETADATA)

  FileBrowserPrivateGetVolumeMetadataFunction();

 protected:
  virtual ~FileBrowserPrivateGetVolumeMetadataFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.validatePathNameLength method.
class FileBrowserPrivateValidatePathNameLengthFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.validatePathNameLength",
                             FILEBROWSERPRIVATE_VALIDATEPATHNAMELENGTH)

  FileBrowserPrivateValidatePathNameLengthFunction();

 protected:
  virtual ~FileBrowserPrivateValidatePathNameLengthFunction();

  void OnFilePathLimitRetrieved(size_t current_length, size_t max_length);

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.formatDevice method.
// Formats Device given its mount path.
class FileBrowserPrivateFormatDeviceFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.formatDevice",
                             FILEBROWSERPRIVATE_FORMATDEVICE)

  FileBrowserPrivateFormatDeviceFunction();

 protected:
  virtual ~FileBrowserPrivateFormatDeviceFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.startCopy method.
class FileBrowserPrivateStartCopyFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.startCopy",
                             FILEBROWSERPRIVATE_STARTCOPY)

  FileBrowserPrivateStartCopyFunction();

 protected:
  virtual ~FileBrowserPrivateStartCopyFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Part of RunImpl(). Called after Copy() is started on IO thread.
  void RunAfterStartCopy(int operation_id);
};

// Implements the chrome.fileBrowserPrivate.cancelCopy method.
class FileBrowserPrivateCancelCopyFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.cancelCopy",
                             FILEBROWSERPRIVATE_CANCELCOPY)

  FileBrowserPrivateCancelCopyFunction();

 protected:
  virtual ~FileBrowserPrivateCancelCopyFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
