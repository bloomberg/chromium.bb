// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_PRIVATE_API_H_
#pragma once

#include <map>
#include <string>
#include <queue>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/prefs/pref_service.h"
#include "googleurl/src/url_util.h"

class GURL;

namespace content {
struct SelectedFileInfo;
}

namespace gdata {
struct SearchResultInfo;
}

// Implements the chrome.fileBrowserPrivate.requestLocalFileSystem method.
class RequestLocalFileSystemFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.requestLocalFileSystem");

 protected:
  virtual ~RequestLocalFileSystemFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  class LocalFileSystemCallbackDispatcher;

  void RespondSuccessOnUIThread(const std::string& name,
                                const GURL& root_path);
  void RespondFailedOnUIThread(base::PlatformFileError error_code);
  void RequestOnFileThread(const GURL& source_url, int child_id);
};

// Implements the chrome.fileBrowserPrivate.addFileWatch method.
class FileWatchBrowserFunctionBase : public AsyncExtensionFunction {
 protected:
  virtual ~FileWatchBrowserFunctionBase() {}

  virtual bool PerformFileWatchOperation(
      scoped_refptr<FileBrowserEventRouter> event_router,
      const FilePath& local_path, const FilePath& virtual_path,
      const std::string& extension_id) = 0;

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  bool GetLocalFilePath(const GURL& file_url, FilePath* local_path,
                        FilePath* virtual_path);
  void RespondOnUIThread(bool success);
  void RunFileWatchOperationOnFileThread(
      scoped_refptr<FileBrowserEventRouter> event_router,
      const GURL& file_url,
      const std::string& extension_id);
};

// Implements the chrome.fileBrowserPrivate.addFileWatch method.
class AddFileWatchBrowserFunction : public FileWatchBrowserFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.addFileWatch");

 protected:
  virtual ~AddFileWatchBrowserFunction() {}

  virtual bool PerformFileWatchOperation(
      scoped_refptr<FileBrowserEventRouter> event_router,
      const FilePath& local_path, const FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};


// Implements the chrome.fileBrowserPrivate.removeFileWatch method.
class RemoveFileWatchBrowserFunction : public FileWatchBrowserFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.removeFileWatch");

 protected:
  virtual ~RemoveFileWatchBrowserFunction() {}

  virtual bool PerformFileWatchOperation(
      scoped_refptr<FileBrowserEventRouter> event_router,
      const FilePath& local_path, const FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.getFileTasks method.
class GetFileTasksFileBrowserFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getFileTasks");

 protected:
  virtual ~GetFileTasksFileBrowserFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  bool FindDriveAppTasks(const std::vector<GURL>& file_urls,
                         ListValue* result_list);
};

// Implements the chrome.fileBrowserPrivate.executeTask method.
class ExecuteTasksFileBrowserFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.executeTask");

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
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.setDefaultTask");

  SetDefaultTaskFileBrowserFunction();

 protected:
  virtual ~SetDefaultTaskFileBrowserFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Parent class for the chromium extension APIs for the file dialog.
class FileBrowserFunction
    : public AsyncExtensionFunction {
 public:
  FileBrowserFunction();

 protected:
  typedef std::vector<GURL> UrlList;
  typedef std::vector<content::SelectedFileInfo> SelectedFileInfoList;
  typedef base::Callback<void(const SelectedFileInfoList&)>
      GetLocalPathsCallback;

  virtual ~FileBrowserFunction();

  // Converts virtual paths to local paths by calling GetLocalPathsOnFileThread
  // on the file thread and call |callback| on the UI thread with the result.
  void GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
      const UrlList& file_urls,
      GetLocalPathsCallback callback);

  // Figure out the tab_id of the hosting tab.
  int32 GetTabId() const;

 private:
  // Converts virtual paths to local paths and call |callback| (on the UI
  // thread) with the results.
  // This method must be called from the file thread.
  void GetLocalPathsOnFileThread(const UrlList& file_urls,
                                 GetLocalPathsCallback callback);
};

// Select a single file.  Closes the dialog window.
class SelectFileFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.selectFile");

  SelectFileFunction() {}

 protected:
  virtual ~SelectFileFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);
};

// View multiple selected files.  Window stays open.
class ViewFilesFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.viewFiles");

  ViewFilesFunction();

 protected:
  virtual ~ViewFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const std::string& internal_task_id,
                                       const SelectedFileInfoList& files);
};

// Select multiple files.  Closes the dialog window.
class SelectFilesFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.selectFiles");

  SelectFilesFunction();

 protected:
  virtual ~SelectFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);
};

// Cancel file selection Dialog.  Closes the dialog window.
class CancelFileDialogFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.cancelDialog");

  CancelFileDialogFunction() {}

 protected:
  virtual ~CancelFileDialogFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Mount a device or a file.
class AddMountFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.addMount");

  AddMountFunction();

 protected:
  virtual ~AddMountFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Sends gdata mount event to renderers.
  void RaiseGDataMountEvent(gdata::GDataErrorCode error);
  // A callback method to handle the result of GData authentication request.
  void OnGDataAuthentication(gdata::GDataErrorCode error,
                             const std::string& token);
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const std::string& mount_type_str,
                                       const SelectedFileInfoList& files);
  // A callback method to handle the result of SetMountedState.
  void OnMountedStateSet(const std::string& mount_type,
                         const FilePath::StringType& file_name,
                         base::PlatformFileError error,
                         const FilePath& file_path);
};

// Unmounts selected device. Expects mount point path as an argument.
class RemoveMountFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.removeMount");

  RemoveMountFunction();

 protected:
  virtual ~RemoveMountFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);
};

class GetMountPointsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getMountPoints");

  GetMountPointsFunction();

 protected:
  virtual ~GetMountPointsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Formats Device given its mount path.
class FormatDeviceFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.formatDevice");

  FormatDeviceFunction();

 protected:
  virtual ~FormatDeviceFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);
};

class GetSizeStatsFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getSizeStats");

  GetSizeStatsFunction();

 protected:
  virtual ~GetSizeStatsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  void GetSizeStatsCallbackOnUIThread(const std::string& mount_path,
                                      size_t total_size_kb,
                                      size_t remaining_size_kb);
  void CallGetSizeStatsOnFileThread(const std::string& mount_path);
};

// Retrieves devices meta-data. Expects volume's device path as an argument.
class GetVolumeMetadataFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getVolumeMetadata");

  GetVolumeMetadataFunction();

 protected:
  virtual ~GetVolumeMetadataFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);
};

// Toggles fullscreen mode for the browser.
class ToggleFullscreenFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.toggleFullscreen");

 protected:
  virtual ~ToggleFullscreenFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Checks if the browser is in fullscreen mode.
class IsFullscreenFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.isFullscreen");

 protected:
  virtual ~IsFullscreenFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// File Dialog Strings.
class FileDialogStringsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getStrings");

  FileDialogStringsFunction() {}

 protected:
  virtual ~FileDialogStringsFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Retrieve property information for multiple files, returning a list of the
// same length as the input list of file URLs.  If a particular file has an
// error, then return a dictionary with the key "error" set to the error number
// (base::PlatformFileError) for that entry in the returned list.
class GetGDataFilePropertiesFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getGDataFileProperties");

  GetGDataFilePropertiesFunction();

 protected:
  virtual ~GetGDataFilePropertiesFunction();

  void GetNextFileProperties();
  void CompleteGetFileProperties();

  // Virtual function that can be overridden to do operations on each virtual
  // file path and update its the properties.
  virtual void DoOperation(const FilePath& file_path,
                           base::DictionaryValue* properties,
                           scoped_ptr<gdata::GDataFileProto> file_proto);

  void OnOperationComplete(const FilePath& file_path,
                           base::DictionaryValue* properties,
                           base::PlatformFileError error,
                           scoped_ptr<gdata::GDataFileProto> file_proto);

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Builds list of file properies. Calls DoOperation for each file.
  void PrepareResults();

 private:
  void OnGetFileInfo(const FilePath& file_path,
                     base::DictionaryValue* property_dict,
                     base::PlatformFileError error,
                     scoped_ptr<gdata::GDataFileProto> file_proto);

  void CacheStateReceived(base::DictionaryValue* property_dict,
                          base::PlatformFileError error,
                          int cache_state);

  size_t current_index_;
  base::ListValue* path_list_;
  scoped_ptr<base::ListValue> file_properties_;
};

// Pin/unpin multiple files in the cache, returning a list of file
// properties with the updated cache state.  The returned array is the
// same length as the input list of file URLs.  If a particular file
// has an error, then return a dictionary with the key "error" set to
// the error number (base::PlatformFileError) for that entry in the
// returned list.
class PinGDataFileFunction : public GetGDataFilePropertiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.pinGDataFile");

  PinGDataFileFunction();

 protected:
  virtual ~PinGDataFileFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Actually do the pinning/unpinning of each file.
  virtual void DoOperation(
      const FilePath& file_path,
      base::DictionaryValue* properties,
      scoped_ptr<gdata::GDataFileProto> file_proto) OVERRIDE;

  // Callback for SetPinState. Updates properties with error.
  void OnPinStateSet(const FilePath& path,
                     base::DictionaryValue* properties,
                     scoped_ptr<gdata::GDataFileProto> file_proto,
                     base::PlatformFileError error,
                     const std::string& resource_id,
                     const std::string& md5);

  // True for pin, false for unpin.
  bool set_pin_;
};

// Get file locations for the given list of file URLs. Returns a list of
// location idenfitiers, like ['drive', 'local'], where 'drive' means the
// file is on gdata, and 'local' means the file is on the local drive.
class GetFileLocationsFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getFileLocations");

  GetFileLocationsFunction();

 protected:
  virtual ~GetFileLocationsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);
};

// Get gdata files for the given list of file URLs. Initiate downloading of
// gdata files if these are not cached. Return a list of local file names.
// This function puts empty strings instead of local paths for files could
// not be obtained. For instance, this can happen if the user specifies a new
// file name to save a file on gdata. There may be other reasons to fail. The
// file manager should check if the local paths returned from getGDataFiles()
// contain empty paths.
// TODO(satorux): Should we propagate error types to the JavasScript layer?
class GetGDataFilesFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getGDataFiles");

  GetGDataFilesFunction();

 protected:
  virtual ~GetGDataFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  // Gets the file on the top of the |remaining_gdata_paths_| or sends the
  // response if the queue is empty.
  void GetFileOrSendResponse();

  // Called by GDataFileSystem::GetFile(). Pops the file from
  // |remaining_gdata_paths_|, and calls GetFileOrSendResponse().
  void OnFileReady(base::PlatformFileError error,
                   const FilePath& local_path,
                   const std::string& unused_mime_type,
                   gdata::GDataFileType file_type);

  std::queue<FilePath> remaining_gdata_paths_;
  ListValue* local_paths_;
};

// Implements the chrome.fileBrowserPrivate.executeTask method.
class GetFileTransfersFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getFileTransfers");

  GetFileTransfersFunction();

 protected:
  virtual ~GetFileTransfersFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  ListValue* GetFileTransfersList();
};

// Implements the chrome.fileBrowserPrivate.cancelFileTransfers method.
class CancelFileTransfersFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.cancelFileTransfers");

  CancelFileTransfersFunction();

 protected:
  virtual ~CancelFileTransfersFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);
};

// Implements the chrome.fileBrowserPrivate.transferFile method.
class TransferFileFunction : public FileBrowserFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.transferFile");

  TransferFileFunction();

 protected:
  virtual ~TransferFileFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Helper callback for handling response from
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread()
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  // Helper callback for handling response from GDataFileSystem::TransferFile().
  void OnTransferCompleted(base::PlatformFileError error);
};

// Read setting value.
class GetGDataPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getGDataPreferences");

 protected:
  virtual ~GetGDataPreferencesFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

// Write setting value.
class SetGDataPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.setGDataPreferences");

 protected:
  virtual ~SetGDataPreferencesFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

class GetPathForDriveSearchResultFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "fileBrowserPrivate.getPathForDriveSearchResult");

 protected:
  virtual ~GetPathForDriveSearchResultFunction() {}

  virtual bool RunImpl() OVERRIDE;

  void OnEntryFound(base::PlatformFileError error,
                    const FilePath& entry_path,
                    scoped_ptr<gdata::GDataEntryProto> entry_proto);
};

class SearchDriveFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.searchGData");

 protected:
  virtual ~SearchDriveFunction() {}

  virtual bool RunImpl() OVERRIDE;

 private:
  // Callback fo OpenFileSystem called from RunImpl.
  void OnFileSystemOpened(base::PlatformFileError result,
                          const std::string& file_system_name,
                          const GURL& file_system_url);
  // Callback for gdata::SearchAsync called after file system is opened.
  void OnSearch(base::PlatformFileError error,
                scoped_ptr<std::vector<gdata::SearchResultInfo> > result_paths);

  // Query for which the search is being performed.
  std::string query_;
  // Information about remote file system we will need to create file entries
  // to represent search results.
  std::string file_system_name_;
  GURL file_system_url_;
};

// Implements the chrome.fileBrowserPrivate.getNetworkConnectionState method.
class GetNetworkConnectionStateFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "fileBrowserPrivate.getNetworkConnectionState");

 protected:
  virtual ~GetNetworkConnectionStateFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.requestDirectoryRefresh method.
class RequestDirectoryRefreshFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "fileBrowserPrivate.requestDirectoryRefresh");

 protected:
  virtual ~RequestDirectoryRefreshFunction() {}

  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_PRIVATE_API_H_
