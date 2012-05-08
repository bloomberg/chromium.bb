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

#include "base/platform_file.h"
#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/prefs/pref_service.h"
#include "googleurl/src/url_util.h"

class GURL;

namespace content {
struct SelectedFileInfo;
}

// Implements the chrome.fileBrowserPrivate.requestLocalFileSystem method.
class RequestLocalFileSystemFunction : public AsyncExtensionFunction {
 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  class LocalFileSystemCallbackDispatcher;

  // Adds gdata mount point.
  void AddGDataMountPoint();

  void RespondSuccessOnUIThread(const std::string& name,
                                const GURL& root_path);
  void RespondFailedOnUIThread(base::PlatformFileError error_code);
  void RequestOnFileThread(const GURL& source_url, int child_id);
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.requestLocalFileSystem");
};

// Implements the chrome.fileBrowserPrivate.addFileWatch method.
class FileWatchBrowserFunctionBase : public AsyncExtensionFunction {
 protected:
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
 protected:
  virtual bool PerformFileWatchOperation(
      scoped_refptr<FileBrowserEventRouter> event_router,
      const FilePath& local_path, const FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.addFileWatch");
};


// Implements the chrome.fileBrowserPrivate.removeFileWatch method.
class RemoveFileWatchBrowserFunction : public FileWatchBrowserFunctionBase {
 protected:
  virtual bool PerformFileWatchOperation(
      scoped_refptr<FileBrowserEventRouter> event_router,
      const FilePath& local_path, const FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.removeFileWatch");
};

// Implements the chrome.fileBrowserPrivate.getFileTasks method.
class GetFileTasksFileBrowserFunction : public AsyncExtensionFunction {
 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getFileTasks");
};

// Implements the chrome.fileBrowserPrivate.executeTask method.
class ExecuteTasksFileBrowserFunction : public AsyncExtensionFunction {
 public:
  ExecuteTasksFileBrowserFunction();
  virtual ~ExecuteTasksFileBrowserFunction();

  void OnTaskExecuted(bool success);

 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  class Executor;

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.executeTask");
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
class SelectFileFunction
    : public FileBrowserFunction {
 public:
  SelectFileFunction() {}

 protected:
  virtual ~SelectFileFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.selectFile");
};

// View multiple selected files.  Window stays open.
class ViewFilesFunction
    : public FileBrowserFunction {
 public:
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

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.viewFiles");
};

// Select multiple files.  Closes the dialog window.
class SelectFilesFunction
    : public FileBrowserFunction {
 public:
  SelectFilesFunction();

 protected:
  virtual ~SelectFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.selectFiles");
};

// Cancel file selection Dialog.  Closes the dialog window.
class CancelFileDialogFunction
    : public FileBrowserFunction {
 public:
  CancelFileDialogFunction() {}

 protected:
  virtual ~CancelFileDialogFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.cancelDialog");
};

// Mount a device or a file.
class AddMountFunction
    : public FileBrowserFunction {
 public:
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

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.addMount");
};

// Unmounts selected device. Expects mount point path as an argument.
class RemoveMountFunction
    : public FileBrowserFunction {
 public:
  RemoveMountFunction();

 protected:
  virtual ~RemoveMountFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.removeMount");
};

class GetMountPointsFunction
    : public AsyncExtensionFunction {
 public:
  GetMountPointsFunction();

 protected:
  virtual ~GetMountPointsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getMountPoints");
};

// Formats Device given its mount path.
class FormatDeviceFunction
    : public FileBrowserFunction {
 public:
  FormatDeviceFunction();

 protected:
  virtual ~FormatDeviceFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.formatDevice");
};

class GetSizeStatsFunction
    : public FileBrowserFunction {
 public:
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

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getSizeStats");
};

// Retrieves devices meta-data. Expects volume's device path as an argument.
class GetVolumeMetadataFunction
    : public FileBrowserFunction {
 public:
  GetVolumeMetadataFunction();

 protected:
  virtual ~GetVolumeMetadataFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getVolumeMetadata");
};

// Toggles fullscreen mode for the browser.
class ToggleFullscreenFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.toggleFullscreen");
};

// Checks if the browser is in fullscreen mode.
class IsFullscreenFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.isFullscreen");
};

// File Dialog Strings.
class FileDialogStringsFunction : public SyncExtensionFunction {
 public:
  FileDialogStringsFunction() {}

 protected:
  virtual ~FileDialogStringsFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getStrings");
};

// Retrieve property information for multiple files, returning a list of the
// same length as the input list of file URLs.  If a particular file has an
// error, then return a dictionary with the key "error" set to the error number
// (base::PlatformFileError) for that entry in the returned list.
class GetGDataFilePropertiesFunction : public FileBrowserFunction {
 public:
  GetGDataFilePropertiesFunction();

 protected:
  void GetNextFileProperties();
  void CompleteGetFileProperties();

  virtual ~GetGDataFilePropertiesFunction();

  // Virtual function that can be overridden to do operations on each virtual
  // file path and update its the properties.
  virtual void DoOperation(const FilePath& file,
                           base::DictionaryValue* properties);

  void OnOperationComplete(const FilePath& file,
                           base::DictionaryValue* properties,
                           base::PlatformFileError error);

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Builds list of file properies. Calls DoOperation for each file.
  void PrepareResults();

 private:
  void OnGetFileInfo(base::DictionaryValue* property_dict,
                     const FilePath& file_path,
                     base::PlatformFileError error,
                     scoped_ptr<gdata::GDataFileProto> file_proto);

  void CacheStateReceived(base::DictionaryValue* property_dict,
                          base::PlatformFileError error,
                          int cache_state);

  size_t current_index_;
  base::ListValue* path_list_;
  scoped_ptr<base::ListValue> file_properties_;

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getGDataFileProperties");
};

// Pin/unpin multiple files in the cache, returning a list of file
// properties with the updated cache state.  The returned array is the
// same length as the input list of file URLs.  If a particular file
// has an error, then return a dictionary with the key "error" set to
// the error number (base::PlatformFileError) for that entry in the
// returned list.
class PinGDataFileFunction : public GetGDataFilePropertiesFunction {
 public:
  PinGDataFileFunction();

 protected:
  virtual ~PinGDataFileFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Actually do the pinning/unpinning of each file.
  virtual void DoOperation(const FilePath& path,
                           base::DictionaryValue* properties) OVERRIDE;

  // Callback for SetPinState. Updates properties with error.
  void OnPinStateSet(const FilePath& path,
                     base::DictionaryValue* properties,
                     base::PlatformFileError error);

  // True for pin, false for unpin.
  bool set_pin_;

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.pinGDataFile");
};

// Get file locations for the given list of file URLs. Returns a list of
// location idenfitiers, like ['gdata', 'local'], where 'gdata' means the
// file is on gdata, and 'local' means the file is on the local drive.
class GetFileLocationsFunction : public FileBrowserFunction {
 public:
  GetFileLocationsFunction();

 protected:
  virtual ~GetFileLocationsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getFileLocations");
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

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getGDataFiles");
};

// Implements the chrome.fileBrowserPrivate.executeTask method.
class GetFileTransfersFunction : public AsyncExtensionFunction {
 public:
  GetFileTransfersFunction();
  virtual ~GetFileTransfersFunction();

 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  ListValue* GetFileTransfersList();

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getFileTransfers");
};

// Implements the chrome.fileBrowserPrivate.cancelFileTransfers method.
class CancelFileTransfersFunction : public FileBrowserFunction {
 public:
  CancelFileTransfersFunction();
  virtual ~CancelFileTransfersFunction();

 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);
 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.cancelFileTransfers");
};

// Implements the chrome.fileBrowserPrivate.transferFile method.
class TransferFileFunction : public FileBrowserFunction {
 public:
  TransferFileFunction();
  virtual ~TransferFileFunction();

 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Helper callback for handling response from
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread()
  void GetLocalPathsResponseOnUIThread(const SelectedFileInfoList& files);

  // Helper callback for handling response from GDataFileSystem::TransferFile().
  void OnTransferCompleted(base::PlatformFileError error);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.transferFile");
};

// Read setting value.
class GetGDataPreferencesFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getGDataPreferences");
};

// Write setting value.
class SetGDataPreferencesFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.setGDataPreferences");
};

class GetPathForDriveSearchResultFunction : public AsyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;

  void OnEntryFound(base::PlatformFileError error,
                    const FilePath& entry_path,
                    scoped_ptr<gdata::GDataEntryProto> entry_proto);

 private:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "fileBrowserPrivate.getPathForDriveSearchResult");
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_PRIVATE_API_H_
