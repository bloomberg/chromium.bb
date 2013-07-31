// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/chromeos/extensions/file_manager/zip_file_creator.h"
#include "chrome/browser/extensions/extension_function.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class FileManagerEventRouter;
class GURL;
class Profile;

namespace fileapi {
class FileSystemContext;
}

namespace file_manager {

// Manages and registers the fileBrowserPrivate API with the extension system.
class FileBrowserPrivateAPI : public BrowserContextKeyedService {
 public:
  explicit FileBrowserPrivateAPI(Profile* profile);
  virtual ~FileBrowserPrivateAPI();

  // BrowserContextKeyedService overrides.
  virtual void Shutdown() OVERRIDE;

  // Convenience function to return the FileBrowserPrivateAPI for a Profile.
  static FileBrowserPrivateAPI* Get(Profile* profile);

  FileManagerEventRouter* event_router() {
    return event_router_.get();
  }

 private:
  scoped_ptr<FileManagerEventRouter> event_router_;
};

// Select a single file.  Closes the dialog window.
// Implements the chrome.fileBrowserPrivate.logoutUser method.
class LogoutUserFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.logoutUser",
                             FILEBROWSERPRIVATE_LOGOUTUSER)

  LogoutUserFunction();

 protected:
  virtual ~LogoutUserFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.requestFileSystem method.
class RequestFileSystemFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.requestFileSystem",
                             FILEBROWSERPRIVATE_REQUESTFILESYSTEM)

  RequestFileSystemFunction();

 protected:
  virtual ~RequestFileSystemFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  void RespondSuccessOnUIThread(const std::string& name,
                                const GURL& root_path);
  void RespondFailedOnUIThread(base::PlatformFileError error_code);

  // Called when FileSystemContext::OpenFileSystem() is done.
  void DidOpenFileSystem(
      scoped_refptr<fileapi::FileSystemContext> file_system_context,
      base::PlatformFileError result,
      const std::string& name,
      const GURL& root_path);

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

// Implements the chrome.fileBrowserPrivate.addFileWatch method.
class FileWatchBrowserFunctionBase : public LoggedAsyncExtensionFunction {
 public:
  FileWatchBrowserFunctionBase();

 protected:
  virtual ~FileWatchBrowserFunctionBase();

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
class AddFileWatchBrowserFunction : public FileWatchBrowserFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.addFileWatch",
                             FILEBROWSERPRIVATE_ADDFILEWATCH)

  AddFileWatchBrowserFunction();

 protected:
  virtual ~AddFileWatchBrowserFunction();

  // FileWatchBrowserFunctionBase override.
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};


// Implements the chrome.fileBrowserPrivate.removeFileWatch method.
class RemoveFileWatchBrowserFunction : public FileWatchBrowserFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.removeFileWatch",
                             FILEBROWSERPRIVATE_REMOVEFILEWATCH)

  RemoveFileWatchBrowserFunction();

 protected:
  virtual ~RemoveFileWatchBrowserFunction();

  // FileWatchBrowserFunctionBase override.
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};

// View multiple selected files.  Window stays open.
class ViewFilesFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.viewFiles",
                             FILEBROWSERPRIVATE_VIEWFILES)

  ViewFilesFunction();

 protected:
  virtual ~ViewFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Formats Device given its mount path.
class FormatDeviceFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.formatDevice",
                             FILEBROWSERPRIVATE_FORMATDEVICE)

  FormatDeviceFunction();

 protected:
  virtual ~FormatDeviceFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Sets last modified date in seconds of local file
class SetLastModifiedFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setLastModified",
                             FILEBROWSERPRIVATE_SETLASTMODIFIED)

  SetLastModifiedFunction();

 protected:
  virtual ~SetLastModifiedFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class GetSizeStatsFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getSizeStats",
                             FILEBROWSERPRIVATE_GETSIZESTATS)

  GetSizeStatsFunction();

 protected:
  virtual ~GetSizeStatsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetDriveAvailableSpaceCallback(drive::FileError error,
                                      int64 bytes_total,
                                      int64 bytes_used);

  void GetSizeStatsCallback(const uint64* total_size,
                            const uint64* remaining_size);
};

// Retrieves devices meta-data. Expects volume's device path as an argument.
class GetVolumeMetadataFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getVolumeMetadata",
                             FILEBROWSERPRIVATE_GETVOLUMEMETADATA)

  GetVolumeMetadataFunction();

 protected:
  virtual ~GetVolumeMetadataFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.getStrings method.
// Used to get strings for the file manager from JavaScript.
class GetStringsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getStrings",
                             FILEBROWSERPRIVATE_GETSTRINGS)

  GetStringsFunction();

 protected:
  virtual ~GetStringsFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Read setting value.
class GetPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getPreferences",
                             FILEBROWSERPRIVATE_GETPREFERENCES)

  GetPreferencesFunction();

 protected:
  virtual ~GetPreferencesFunction();

  virtual bool RunImpl() OVERRIDE;
};

// Write setting value.
class SetPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setPreferences",
                             FILEBROWSERPRIVATE_SETPREFERENCES)

  SetPreferencesFunction();

 protected:
  virtual ~SetPreferencesFunction();

  virtual bool RunImpl() OVERRIDE;
};

// Create a zip file for the selected files.
class ZipSelectionFunction : public LoggedAsyncExtensionFunction,
                             public extensions::ZipFileCreator::Observer {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.zipSelection",
                             FILEBROWSERPRIVATE_ZIPSELECTION)

  ZipSelectionFunction();

 protected:
  virtual ~ZipSelectionFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // extensions::ZipFileCreator::Delegate overrides.
  virtual void OnZipDone(bool success) OVERRIDE;

 private:
  scoped_refptr<extensions::ZipFileCreator> zip_file_creator_;
};

// Implements the chrome.fileBrowserPrivate.validatePathNameLength method.
class ValidatePathNameLengthFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.validatePathNameLength",
                             FILEBROWSERPRIVATE_VALIDATEPATHNAMELENGTH)

  ValidatePathNameLengthFunction();

 protected:
  virtual ~ValidatePathNameLengthFunction();

  void OnFilePathLimitRetrieved(size_t current_length, size_t max_length);

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Changes the zoom level of the file manager by internally calling
// RenderViewHost::Zoom(). TODO(hirono): Remove this function once the zoom
// level change is supported for all apps. crbug.com/227175.
class ZoomFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.zoom",
                             FILEBROWSERPRIVATE_ZOOM);

  ZoomFunction();

 protected:
  virtual ~ZoomFunction();
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_
