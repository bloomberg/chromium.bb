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
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/search_metadata.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/zip_file_creator.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "googleurl/src/url_util.h"

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
class DriveWebAppsRegistry;
struct DriveWebAppInfo;
struct SearchResultInfo;
}

namespace ui {
struct SelectedFileInfo;
}

// Manages and registers the fileBrowserPrivate API with the extension system.
class FileBrowserPrivateAPI : public ProfileKeyedService {
 public:
  explicit FileBrowserPrivateAPI(Profile* profile);
  virtual ~FileBrowserPrivateAPI();

  // ProfileKeyedService overrides.
  virtual void Shutdown() OVERRIDE;

  // Convenience function to return the FileBrowserPrivateAPI for a Profile.
  static FileBrowserPrivateAPI* Get(Profile* profile);

  FileManagerEventRouter* event_router() {
    return event_router_.get();
  }

 private:
  scoped_ptr<FileManagerEventRouter> event_router_;
};

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

// Implements the chrome.fileBrowserPrivate.requestLocalFileSystem method.
class RequestLocalFileSystemFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.requestLocalFileSystem",
                             FILEBROWSERPRIVATE_REQUESTLOCALFILESYSTEM)

 protected:
  virtual ~RequestLocalFileSystemFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  class LocalFileSystemCallbackDispatcher;

  void RespondSuccessOnUIThread(const std::string& name,
                                const GURL& root_path);
  void RespondFailedOnUIThread(base::PlatformFileError error_code);
  void RequestOnFileThread(
      scoped_refptr<fileapi::FileSystemContext> file_system_context,
      const GURL& source_url,
      int child_id);
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
  struct FileInfo {
    GURL file_url;
    base::FilePath file_path;
    std::string mime_type;
  };
  typedef std::vector<FileInfo> FileInfoList;

  // Typedef for holding a map from app_id to DriveWebAppInfo so
  // we can look up information on the apps.
  typedef std::map<std::string, drive::DriveWebAppInfo*> WebAppInfoMap;

  // Look up apps in the registry, and collect applications that match the file
  // paths given. Returns the intersection of all available application ids in
  // |available_apps| and a map of application ID to the Drive web application
  // info collected in |app_info| so details can be collected later. The caller
  // takes ownership of the pointers in |app_info|.
  static void IntersectAvailableDriveTasks(
      drive::DriveWebAppsRegistry* registry,
      const FileInfoList& file_info_list,
      WebAppInfoMap* app_info,
      std::set<std::string>* available_apps);

  // Takes a map of app_id to application information in |app_info|, and the set
  // of |available_apps| and adds Drive tasks to the |result_list| for each of
  // the |available_apps|.  If a default task is set in the result list,
  // then |default_already_set| is set to true.
  static void CreateDriveTasks(drive::DriveWebAppsRegistry* registry,
                               const WebAppInfoMap& app_info,
                               const std::set<std::string>& available_apps,
                               const std::set<std::string>& default_apps,
                               ListValue* result_list,
                               bool* default_already_set);

  // Looks in the preferences and finds any of the available apps that are
  // also listed as default apps for any of the files in the info list.
  void FindDefaultDriveTasks(const FileInfoList& file_info_list,
                             const std::set<std::string>& available_apps,
                             std::set<std::string>* default_apps);

  // Find the list of drive apps that can be used with the given file types. If
  // a default task is set in the result list, then |default_already_set| is set
  // to true.
  bool FindDriveAppTasks(const FileInfoList& file_info_list,
                         ListValue* result_list,
                         bool* default_already_set);

  // Find the list of app file handlers that can be used with the given file
  // types, appending them to the |result_list|.
  bool FindAppTasks(const std::vector<base::FilePath>& file_paths,
                    ListValue* result_list);
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

// Parent class for the chromium extension APIs for the file dialog.
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

 private:
  struct GetSelectedFileInfoParams;

  // Used to implement GetSelectedFileInfo().
  void GetSelectedFileInfoInternal(
      scoped_ptr<GetSelectedFileInfoParams> params);

  // Used to implement GetSelectedFileInfo().
  void ContinueGetSelectedFileInfo(scoped_ptr<GetSelectedFileInfoParams> params,
                                   drive::FileError error,
                                   const base::FilePath& local_file_path,
                                   const std::string& mime_type,
                                   drive::DriveFileType file_type);
};

// Select a single file.  Closes the dialog window.
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

class GetMountPointsFunction : public AsyncExtensionFunction {
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

// Get file locations for the given list of file URLs. Returns a list of
// location identifiers, like ['drive', 'local'], where 'drive' means the
// file is on gdata, and 'local' means the file is on the local drive.
class GetFileLocationsFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getFileLocations",
                             FILEBROWSERPRIVATE_GETFILELOCATIONS)

  GetFileLocationsFunction();

 protected:
  virtual ~GetFileLocationsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
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
                   const std::string& unused_mime_type,
                   drive::DriveFileType file_type);

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
  // Callback fo OpenFileSystem called from RunImpl.
  void OnFileSystemOpened(base::PlatformFileError result,
                          const std::string& file_system_name,
                          const GURL& file_system_url);
  // Callback for google_apis::SearchAsync called after file system is opened.
  void OnSearch(drive::FileError error,
                const GURL& next_feed,
                scoped_ptr<std::vector<drive::SearchResultInfo> > result_paths);

  // Query for which the search is being performed.
  std::string query_;
  std::string next_feed_;
  // Information about remote file system we will need to create file entries
  // to represent search results.
  std::string file_system_name_;
  GURL file_system_url_;
};

// Similar to SearchDriveFunction but this one is used for searching drive
// metadata which is stored locally.
class SearchDriveMetadataFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.searchDriveMetadata",
                             FILEBROWSERPRIVATE_SEARCHDRIVEMETADATA)

  SearchDriveMetadataFunction();

 protected:
  virtual ~SearchDriveMetadataFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  // Callback fo OpenFileSystem called from RunImpl.
  void OnFileSystemOpened(base::PlatformFileError result,
                          const std::string& file_system_name,
                          const GURL& file_system_url);
  // Callback for LocalSearch().
  void OnSearchMetadata(
      drive::FileError error,
      scoped_ptr<drive::MetadataSearchResultVector> results);

  // Query for which the search is being performed.
  std::string query_;
  // String representing what type of entries are needed. It corresponds to
  // SearchMetadataOptions:
  // SEARCH_METADATA_OPTION_EXCLUDE_DIRECTORIES => "EXCLUDE_DIRECTORIES"
  // SEARCH_METADATA_OPTION_ALL => "ALL"
  std::string types_;
  // Maximum number of results. The results contains |max_num_results_| entries
  // at most.
  int max_results_;

  // Information about remote file system we will need to create file entries
  // to represent search results.
  std::string file_system_name_;
  GURL file_system_url_;
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

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_
