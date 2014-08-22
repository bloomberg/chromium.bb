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

// Implements the chrome.fileBrowserPrivate.requestFileSystem method.
class FileBrowserPrivateRequestFileSystemFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.requestFileSystem",
                             FILEBROWSERPRIVATE_REQUESTFILESYSTEM)

 protected:
  virtual ~FileBrowserPrivateRequestFileSystemFunction() {}

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

// Base class for FileBrowserPrivateAddFileWatchFunction and
// FileBrowserPrivateRemoveFileWatchFunction. Although it's called "FileWatch",
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

// Implements the chrome.fileBrowserPrivate.addFileWatch method.
// Starts watching changes in directories.
class FileBrowserPrivateAddFileWatchFunction : public FileWatchFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.addFileWatch",
                             FILEBROWSERPRIVATE_ADDFILEWATCH)

 protected:
  virtual ~FileBrowserPrivateAddFileWatchFunction() {}

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

 protected:
  virtual ~FileBrowserPrivateRemoveFileWatchFunction() {}

  // FileWatchFunctionBase override.
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.getSizeStats method.
class FileBrowserPrivateGetSizeStatsFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getSizeStats",
                             FILEBROWSERPRIVATE_GETSIZESTATS)

 protected:
  virtual ~FileBrowserPrivateGetSizeStatsFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void GetDriveAvailableSpaceCallback(drive::FileError error,
                                      int64 bytes_total,
                                      int64 bytes_used);

  void GetSizeStatsCallback(const uint64* total_size,
                            const uint64* remaining_size);
};

// Implements the chrome.fileBrowserPrivate.validatePathNameLength method.
class FileBrowserPrivateValidatePathNameLengthFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.validatePathNameLength",
                             FILEBROWSERPRIVATE_VALIDATEPATHNAMELENGTH)

 protected:
  virtual ~FileBrowserPrivateValidatePathNameLengthFunction() {}

  void OnFilePathLimitRetrieved(size_t current_length, size_t max_length);

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.formatVolume method.
// Formats Volume given its mount path.
class FileBrowserPrivateFormatVolumeFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.formatVolume",
                             FILEBROWSERPRIVATE_FORMATVOLUME)

 protected:
  virtual ~FileBrowserPrivateFormatVolumeFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.startCopy method.
class FileBrowserPrivateStartCopyFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.startCopy",
                             FILEBROWSERPRIVATE_STARTCOPY)

 protected:
  virtual ~FileBrowserPrivateStartCopyFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // Part of RunAsync(). Called after Copy() is started on IO thread.
  void RunAfterStartCopy(int operation_id);
};

// Implements the chrome.fileBrowserPrivate.cancelCopy method.
class FileBrowserPrivateCancelCopyFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.cancelCopy",
                             FILEBROWSERPRIVATE_CANCELCOPY)

 protected:
  virtual ~FileBrowserPrivateCancelCopyFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivateInternal.resolveIsolatedEntries
// method.
class FileBrowserPrivateInternalResolveIsolatedEntriesFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileBrowserPrivateInternal.resolveIsolatedEntries",
      FILEBROWSERPRIVATE_RESOLVEISOLATEDENTRIES)

 protected:
  virtual ~FileBrowserPrivateInternalResolveIsolatedEntriesFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList(scoped_ptr<
      file_manager::util::EntryDefinitionList> entry_definition_list);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
