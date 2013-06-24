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
#include "base/time.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/extensions/file_manager/zip_file_creator.h"
#include "chrome/browser/extensions/extension_function.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class FileManagerEventRouter;
class GURL;
class Profile;

namespace base {
class Value;
}

namespace fileapi {
class FileSystemContext;
class FileSystemURL;
}

namespace drive {
class FileCacheEntry;
class DriveAppRegistry;
struct DriveAppInfo;
struct SearchResultInfo;
}

namespace ui {
struct SelectedFileInfo;
}

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

// Parent class for the chromium extension APIs for the file manager.
class FileBrowserFunction : public AsyncExtensionFunction {
 public:
  FileBrowserFunction();

 protected:
  typedef std::vector<GURL> UrlList;
  typedef std::vector<ui::SelectedFileInfo> SelectedFileInfoList;
  typedef base::Callback<void(const SelectedFileInfoList&)>
      GetSelectedFileInfoCallback;

  virtual ~FileBrowserFunction();

  // Figures out the tab_id of the hosting tab.
  int32 GetTabId() const;

  // Returns the local FilePath associated with |url|. If the file isn't of the
  // type CrosMountPointProvider handles, returns an empty FilePath.
  //
  // Local paths will look like "/home/chronos/user/Downloads/foo/bar.txt" or
  // "/special/drive/foo/bar.txt".
  base::FilePath GetLocalPathFromURL(const GURL& url);

  // Runs |callback| with SelectedFileInfoList created from |file_urls|.
  void GetSelectedFileInfo(const UrlList& file_urls,
                           bool for_opening,
                           GetSelectedFileInfoCallback callback);

  virtual void SendResponse(bool success) OVERRIDE;

 protected:
  base::TimeDelta GetElapsedTime();
  void set_log_on_completion(bool log_on_completion) {
    log_on_completion_ = log_on_completion;
  }

 private:
  struct GetSelectedFileInfoParams;

  // Used to implement GetSelectedFileInfo().
  void GetSelectedFileInfoInternal(
      scoped_ptr<GetSelectedFileInfoParams> params);

  // Used to implement GetSelectedFileInfo().
  void ContinueGetSelectedFileInfo(scoped_ptr<GetSelectedFileInfoParams> params,
                                   drive::FileError error,
                                   const base::FilePath& local_file_path,
                                   scoped_ptr<drive::ResourceEntry> entry);

  base::Time start_time_;
  bool log_on_completion_;
};

// Select a single file.  Closes the dialog window.
// Implements the chrome.fileBrowserPrivate.logoutUser method.
class LogoutUserFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.logoutUser",
                             FILEBROWSERPRIVATE_LOGOUTUSER)

 protected:
  virtual ~LogoutUserFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.requestFileSystem method.
class RequestFileSystemFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.requestFileSystem",
                             FILEBROWSERPRIVATE_REQUESTFILESYSTEM)

 protected:
  virtual ~RequestFileSystemFunction() {}

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
class FileWatchBrowserFunctionBase : public AsyncExtensionFunction {
 protected:
  virtual ~FileWatchBrowserFunctionBase() {}

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

 protected:
  virtual ~AddFileWatchBrowserFunction() {}

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

 protected:
  virtual ~RemoveFileWatchBrowserFunction() {}

  // FileWatchBrowserFunctionBase override.
  virtual void PerformFileWatchOperation(
      const base::FilePath& local_path,
      const base::FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.getFileTasks method.
class GetFileTasksFileBrowserFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getFileTasks",
                             FILEBROWSERPRIVATE_GETFILETASKS)

 protected:
  virtual ~GetFileTasksFileBrowserFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  struct FileInfo;
  typedef std::vector<FileInfo> FileInfoList;

  // Holds fields to build a task result.
  struct TaskInfo;

  // Map from a task id to TaskInfo.
  typedef std::map<std::string, TaskInfo> TaskInfoMap;

  // Looks up available apps for each file in |file_info_list| in the
  // |registry|, and returns the intersection of all available apps as a
  // map from task id to TaskInfo.
  static void GetAvailableDriveTasks(drive::DriveAppRegistry* registry,
                                     const FileInfoList& file_info_list,
                                     TaskInfoMap* task_info_map);

  // Looks in the preferences and finds any of the available apps that are
  // also listed as default apps for any of the files in the info list.
  void FindDefaultDriveTasks(const FileInfoList& file_info_list,
                             const TaskInfoMap& task_info_map,
                             std::set<std::string>* default_tasks);

  // Creates a list of each task in |task_info_map| and stores the result into
  // |result_list|. If a default task is set in the result list,
  // |default_already_set| is set to true.
  static void CreateDriveTasks(const TaskInfoMap& task_info_map,
                               const std::set<std::string>& default_tasks,
                               ListValue* result_list,
                               bool* default_already_set);

  // Find the list of drive apps that can be used with the given file types. If
  // a default task is set in the result list, then |default_already_set| is set
  // to true.
  bool FindDriveAppTasks(const FileInfoList& file_info_list,
                         ListValue* result_list,
                         bool* default_already_set);

  // Find the list of app file handlers that can be used with the given file
  // types, appending them to the |result_list|. If a default task is set in the
  // result list, then |default_already_set| is set to true.
  bool FindAppTasks(const std::vector<base::FilePath>& file_paths,
                    ListValue* result_list,
                    bool* default_already_set);
};

// Implements the chrome.fileBrowserPrivate.executeTask method.
class ExecuteTasksFileBrowserFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.executeTask",
                             FILEBROWSERPRIVATE_EXECUTETASK)

  ExecuteTasksFileBrowserFunction();

  void OnTaskExecuted(bool success);

 protected:
  virtual ~ExecuteTasksFileBrowserFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.setDefaultTask method.
class SetDefaultTaskFileBrowserFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setDefaultTask",
                             FILEBROWSERPRIVATE_SETDEFAULTTASK)

  SetDefaultTaskFileBrowserFunction();

 protected:
  virtual ~SetDefaultTaskFileBrowserFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class SelectFileFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.selectFile",
                             FILEBROWSERPRIVATE_SELECTFILE)

  SelectFileFunction() {}

 protected:
  virtual ~SelectFileFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(const SelectedFileInfoList& files);
};

// View multiple selected files.  Window stays open.
class ViewFilesFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.viewFiles",
                             FILEBROWSERPRIVATE_VIEWFILES)

  ViewFilesFunction();

 protected:
  virtual ~ViewFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Select multiple files.  Closes the dialog window.
class SelectFilesFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.selectFiles",
                             FILEBROWSERPRIVATE_SELECTFILES)

  SelectFilesFunction();

 protected:
  virtual ~SelectFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(const SelectedFileInfoList& files);
};

// Cancel file selection Dialog.  Closes the dialog window.
class CancelFileDialogFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.cancelDialog",
                             FILEBROWSERPRIVATE_CANCELDIALOG)

  CancelFileDialogFunction() {}

 protected:
  virtual ~CancelFileDialogFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Mount a device or a file.
class AddMountFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.addMount",
                             FILEBROWSERPRIVATE_ADDMOUNT)

  AddMountFunction();

 protected:
  virtual ~AddMountFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of MarkCacheAsMounted.
  void OnMountedStateSet(const std::string& mount_type,
                         const base::FilePath::StringType& file_name,
                         drive::FileError error,
                         const base::FilePath& file_path);
};

// Unmounts selected device. Expects mount point path as an argument.
class RemoveMountFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.removeMount",
                             FILEBROWSERPRIVATE_REMOVEMOUNT)

  RemoveMountFunction();

 protected:
  virtual ~RemoveMountFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(const SelectedFileInfoList& files);
};

class GetMountPointsFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getMountPoints",
                             FILEBROWSERPRIVATE_GETMOUNTPOINTS)

  GetMountPointsFunction();

 protected:
  virtual ~GetMountPointsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Formats Device given its mount path.
class FormatDeviceFunction : public FileBrowserFunction {
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
class SetLastModifiedFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setLastModified",
                             FILEBROWSERPRIVATE_SETLASTMODIFIED)

  SetLastModifiedFunction();

 protected:
  virtual ~SetLastModifiedFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class GetSizeStatsFunction : public FileBrowserFunction {
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

  void GetSizeStatsCallback(const size_t* total_size_kb,
                            const size_t* remaining_size_kb);
};

// Retrieves devices meta-data. Expects volume's device path as an argument.
class GetVolumeMetadataFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getVolumeMetadata",
                             FILEBROWSERPRIVATE_GETVOLUMEMETADATA)

  GetVolumeMetadataFunction();

 protected:
  virtual ~GetVolumeMetadataFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// File Dialog Strings.
class FileDialogStringsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getStrings",
                             FILEBROWSERPRIVATE_GETSTRINGS)

  FileDialogStringsFunction() {}

 protected:
  virtual ~FileDialogStringsFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Retrieves property information for an entry and returns it as a dictionary.
// On error, returns a dictionary with the key "error" set to the error number
// (drive::FileError).
class GetDriveEntryPropertiesFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getDriveEntryProperties",
                             FILEBROWSERPRIVATE_GETDRIVEFILEPROPERTIES)

  GetDriveEntryPropertiesFunction();

 protected:
  virtual ~GetDriveEntryPropertiesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnGetFileInfo(drive::FileError error,
                     scoped_ptr<drive::ResourceEntry> entry);

  void CacheStateReceived(bool success,
                          const drive::FileCacheEntry& cache_entry);

  void CompleteGetFileProperties(drive::FileError error);

  base::FilePath file_path_;
  scoped_ptr<base::DictionaryValue> properties_;
};

// Implements the chrome.fileBrowserPrivate.pinDriveFile method.
class PinDriveFileFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.pinDriveFile",
                             FILEBROWSERPRIVATE_PINDRIVEFILE)

  PinDriveFileFunction();

 protected:
  virtual ~PinDriveFileFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Callback for RunImpl().
  void OnPinStateSet(drive::FileError error);
};

// Get gdata files for the given list of file URLs. Initiate downloading of
// gdata files if these are not cached. Return a list of local file names.
// This function puts empty strings instead of local paths for files could
// not be obtained. For instance, this can happen if the user specifies a new
// file name to save a file on gdata. There may be other reasons to fail. The
// file manager should check if the local paths returned from getDriveFiles()
// contain empty paths.
// TODO(satorux): Should we propagate error types to the JavasScript layer?
class GetDriveFilesFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getDriveFiles",
                             FILEBROWSERPRIVATE_GETDRIVEFILES)

  GetDriveFilesFunction();

 protected:
  virtual ~GetDriveFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Gets the file on the top of the |remaining_drive_paths_| or sends the
  // response if the queue is empty.
  void GetFileOrSendResponse();

  // Called by FileSystem::GetFile(). Pops the file from
  // |remaining_drive_paths_|, and calls GetFileOrSendResponse().
  void OnFileReady(drive::FileError error,
                   const base::FilePath& local_path,
                   scoped_ptr<drive::ResourceEntry> entry);

  std::queue<base::FilePath> remaining_drive_paths_;
  ListValue* local_paths_;
};

// Implements the chrome.fileBrowserPrivate.cancelFileTransfers method.
class CancelFileTransfersFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.cancelFileTransfers",
                             FILEBROWSERPRIVATE_CANCELFILETRANSFERS)

  CancelFileTransfersFunction();

 protected:
  virtual ~CancelFileTransfersFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.transferFile method.
class TransferFileFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.transferFile",
                             FILEBROWSERPRIVATE_TRANSFERFILE)

  TransferFileFunction();

 protected:
  virtual ~TransferFileFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Helper callback for handling response from FileSystem::TransferFile().
  void OnTransferCompleted(drive::FileError error);
};

// Read setting value.
class GetPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getPreferences",
                             FILEBROWSERPRIVATE_GETPREFERENCES)

 protected:
  virtual ~GetPreferencesFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

// Write setting value.
class SetPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setPreferences",
                             FILEBROWSERPRIVATE_SETPREFERENCES)

 protected:
  virtual ~SetPreferencesFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

class SearchDriveFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.searchDrive",
                             FILEBROWSERPRIVATE_SEARCHDRIVE)

  SearchDriveFunction();

 protected:
  virtual ~SearchDriveFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  // Callback for Search().
  void OnSearch(drive::FileError error,
                const GURL& next_feed,
                scoped_ptr<std::vector<drive::SearchResultInfo> > result_paths);
};

// Similar to SearchDriveFunction but this one is used for searching drive
// metadata which is stored locally.
class SearchDriveMetadataFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.searchDriveMetadata",
                             FILEBROWSERPRIVATE_SEARCHDRIVEMETADATA)

  SearchDriveMetadataFunction();

 protected:
  virtual ~SearchDriveMetadataFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  // Callback for SearchMetadata();
  void OnSearchMetadata(drive::FileError error,
                        scoped_ptr<drive::MetadataSearchResultVector> results);
};

class ClearDriveCacheFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.clearDriveCache",
                             FILEBROWSERPRIVATE_CLEARDRIVECACHE)

 protected:
  virtual ~ClearDriveCacheFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.reloadDrive method,
// which is used to reload the file system metadata from the server.
class ReloadDriveFunction: public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.reloadDrive",
                             FILEBROWSERPRIVATE_RELOADDRIVE)

 protected:
  virtual ~ReloadDriveFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.getDriveConnectionState method.
class GetDriveConnectionStateFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileBrowserPrivate.getDriveConnectionState",
      FILEBROWSERPRIVATE_GETDRIVECONNECTIONSTATE);

 protected:
  virtual ~GetDriveConnectionStateFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.requestDirectoryRefresh method.
class RequestDirectoryRefreshFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileBrowserPrivate.requestDirectoryRefresh",
      FILEBROWSERPRIVATE_REQUESTDIRECTORYREFRESH);

 protected:
  virtual ~RequestDirectoryRefreshFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

// Create a zip file for the selected files.
class ZipSelectionFunction : public FileBrowserFunction,
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
class ValidatePathNameLengthFunction : public AsyncExtensionFunction {
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

// Implements the chrome.fileBrowserPrivate.newWindow method.
class OpenNewWindowFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.openNewWindow",
                             FILEBROWSERPRIVATE_OPENNEWWINDOW)

  OpenNewWindowFunction();

 protected:
  virtual ~OpenNewWindowFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class ZoomFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.zoom",
                             FILEBROWSERPRIVATE_ZOOM);

 protected:
  virtual ~ZoomFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_
