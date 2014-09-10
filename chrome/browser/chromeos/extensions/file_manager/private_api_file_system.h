// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides file system related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_

#include <string>

#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"

class GURL;

namespace base {
class FilePath;
}

namespace storage {
class FileSystemContext;
}

namespace file_manager {
namespace util {
struct EntryDefinition;
typedef std::vector<EntryDefinition> EntryDefinitionList;
}  // namespace util
}  // namespace file_manager

namespace extensions {

// Implements the chrome.fileManagerPrivate.requestFileSystem method.
class FileManagerPrivateRequestFileSystemFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.requestFileSystem",
                             FILEMANAGERPRIVATE_REQUESTFILESYSTEM)

 protected:
  virtual ~FileManagerPrivateRequestFileSystemFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void RespondSuccessOnUIThread(const std::string& name,
                                const GURL& root_url);
  void RespondFailedOnUIThread(base::File::Error error_code);

  // Called when something goes wrong. Records the error to |error_| per the
  // error code and reports that the private API function failed.
  void DidFail(base::File::Error error_code);

  // Sets up file system access permissions to the extension identified by
  // |child_id|.
  bool SetupFileSystemAccessPermissions(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      int child_id,
      Profile* profile,
      scoped_refptr<const extensions::Extension> extension);

  // Called when the entry definition is computed.
  void OnEntryDefinition(
      const file_manager::util::EntryDefinition& entry_definition);
};

// Base class for FileManagerPrivateAddFileWatchFunction and
// FileManagerPrivateRemoveFileWatchFunction. Although it's called "FileWatch",
// the class and its sub classes are used only for watching changes in
// directories.
class FileWatchFunctionBase : public LoggedAsyncExtensionFunction {
 protected:
  virtual ~FileWatchFunctionBase() {}

  // Performs a file watch operation (ex. adds or removes a file watch).
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) = 0;

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

  // Calls SendResponse() with |success| converted to base::Value.
  void Respond(bool success);
};

// Implements the chrome.fileManagerPrivate.addFileWatch method.
// Starts watching changes in directories.
class FileManagerPrivateAddFileWatchFunction : public FileWatchFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.addFileWatch",
                             FILEMANAGERPRIVATE_ADDFILEWATCH)

 protected:
  virtual ~FileManagerPrivateAddFileWatchFunction() {}

  // FileWatchFunctionBase override.
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};


// Implements the chrome.fileManagerPrivate.removeFileWatch method.
// Stops watching changes in directories.
class FileManagerPrivateRemoveFileWatchFunction : public FileWatchFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.removeFileWatch",
                             FILEMANAGERPRIVATE_REMOVEFILEWATCH)

 protected:
  virtual ~FileManagerPrivateRemoveFileWatchFunction() {}

  // FileWatchFunctionBase override.
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};

// Implements the chrome.fileManagerPrivate.getSizeStats method.
class FileManagerPrivateGetSizeStatsFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getSizeStats",
                             FILEMANAGERPRIVATE_GETSIZESTATS)

 protected:
  virtual ~FileManagerPrivateGetSizeStatsFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void GetDriveAvailableSpaceCallback(drive::FileError error,
                                      int64 bytes_total,
                                      int64 bytes_used);

  void GetSizeStatsCallback(const uint64* total_size,
                            const uint64* remaining_size);
};

// Implements the chrome.fileManagerPrivate.validatePathNameLength method.
class FileManagerPrivateValidatePathNameLengthFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.validatePathNameLength",
                             FILEMANAGERPRIVATE_VALIDATEPATHNAMELENGTH)

 protected:
  virtual ~FileManagerPrivateValidatePathNameLengthFunction() {}

  void OnFilePathLimitRetrieved(size_t current_length, size_t max_length);

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

// Implements the chrome.fileManagerPrivate.formatVolume method.
// Formats Volume given its mount path.
class FileManagerPrivateFormatVolumeFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.formatVolume",
                             FILEMANAGERPRIVATE_FORMATVOLUME)

 protected:
  virtual ~FileManagerPrivateFormatVolumeFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

// Implements the chrome.fileManagerPrivate.startCopy method.
class FileManagerPrivateStartCopyFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.startCopy",
                             FILEMANAGERPRIVATE_STARTCOPY)

 protected:
  virtual ~FileManagerPrivateStartCopyFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // Part of RunAsync(). Called after Copy() is started on IO thread.
  void RunAfterStartCopy(int operation_id);
};

// Implements the chrome.fileManagerPrivate.cancelCopy method.
class FileManagerPrivateCancelCopyFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.cancelCopy",
                             FILEMANAGERPRIVATE_CANCELCOPY)

 protected:
  virtual ~FileManagerPrivateCancelCopyFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

// Implements the chrome.fileManagerPrivateInternal.resolveIsolatedEntries
// method.
class FileManagerPrivateInternalResolveIsolatedEntriesFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.resolveIsolatedEntries",
      FILEMANAGERPRIVATE_RESOLVEISOLATEDENTRIES)

 protected:
  virtual ~FileManagerPrivateInternalResolveIsolatedEntriesFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList(scoped_ptr<
      file_manager::util::EntryDefinitionList> entry_definition_list);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
