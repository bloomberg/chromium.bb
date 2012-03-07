// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_PRIVATE_API_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/platform_file.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/extensions/extension_function.h"
#include "googleurl/src/url_util.h"

class GURL;

// Implements the chrome.fileBrowserPrivate.requestLocalFileSystem method.
class RequestLocalFileSystemFunction : public AsyncExtensionFunction {
 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  class LocalFileSystemCallbackDispatcher;

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
      const FilePath& local_path, const FilePath& virtual_path,
      const std::string& extension_id) = 0;

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  bool GetLocalFilePath(const GURL& file_url, FilePath* local_path,
                        FilePath* virtual_path);
  void RespondOnUIThread(bool success);
  void RunFileWatchOperationOnFileThread(const GURL& file_url,
                                         const std::string& extension_id);
};

// Implements the chrome.fileBrowserPrivate.addFileWatch method.
class AddFileWatchBrowserFunction : public FileWatchBrowserFunctionBase {
 protected:
  virtual bool PerformFileWatchOperation(
      const FilePath& local_path, const FilePath& virtual_path,
      const std::string& extension_id) OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.addFileWatch");
};


// Implements the chrome.fileBrowserPrivate.removeFileWatch method.
class RemoveFileWatchBrowserFunction : public FileWatchBrowserFunctionBase {
 protected:
  virtual bool PerformFileWatchOperation(
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
 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  struct FileDefinition {
    GURL target_file_url;
    FilePath virtual_path;
    bool is_directory;
  };
  typedef std::vector<FileDefinition> FileDefinitionList;
  class ExecuteTasksFileSystemCallbackDispatcher;
  // Initates execution of context menu tasks identified with |task_id| for
  // each element of |files_list|.
  bool InitiateFileTaskExecution(const std::string& task_id,
                                 base::ListValue* files_list);
  void RequestFileEntryOnFileThread(
      const std::string& task_id,
      const GURL& handler_base_url,
      const scoped_refptr<const Extension>& handler,
      int handler_pid,
      const std::vector<GURL>& file_urls);
  void RespondFailedOnUIThread(base::PlatformFileError error_code);
  void ExecuteFileActionsOnUIThread(const std::string& task_id,
                                    const std::string& file_system_name,
                                    const GURL& file_system_root,
                                    const FileDefinitionList& file_list);
  void ExecuteFailedOnUIThread();
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.executeTask");
};

// Parent class for the chromium extension APIs for the file dialog.
class FileBrowserFunction
    : public AsyncExtensionFunction {
 public:
  FileBrowserFunction();

 protected:
  typedef std::vector<GURL> UrlList;
  typedef std::vector<FilePath> FilePathList;
  typedef base::Callback<void(const FilePathList&)> GetLocalPathsCallback;

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
  void GetLocalPathsResponseOnUIThread(const FilePathList& files);

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
                                       const FilePathList& files);

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
  void GetLocalPathsResponseOnUIThread(const FilePathList& files);

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
  void RaiseGDataMountEvent(gdata::GDataErrorCode error,
                            const std::string auth_token);
  // Adds gdata mount point.
  void AddGDataMountPoint();
  // A callback method to handle the result of GData authentication request.
  void OnGDataAuthentication(gdata::GDataErrorCode error,
                             const std::string& token);
  // A callback method to handle the result of
  // GetLocalPathsOnFileThreadAndRunCallbackOnUIThread.
  void GetLocalPathsResponseOnUIThread(const std::string& mount_type_str,
                                       const FilePathList& files);

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
  void GetLocalPathsResponseOnUIThread(const FilePathList& files);

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
  void GetLocalPathsResponseOnUIThread(const FilePathList& files);

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
  void GetLocalPathsResponseOnUIThread(const FilePathList& files);

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
  void GetLocalPathsResponseOnUIThread(const FilePathList& files);

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
  virtual ~GetGDataFilePropertiesFunction();

  // Virtual function that can be overridden to do operations on each virtual
  // file path before fetching the properties.  Return false to stop iterating
  // over the files.
  virtual bool DoOperation(const FilePath& file);

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getGDataFileProperties");
};

// Pin multiple files in the cache, returning a list of file properties with the
// updated cache state.  The returned array is the same length as the input list
// of file URLs.  If a particular file has an error, then return a dictionary
// with the key "error" set to the error number (base::PlatformFileError) for
// that entry in the returned list.
class PinGDataFileFunction : public GetGDataFilePropertiesFunction {
 public:
  PinGDataFileFunction();

 protected:
  virtual ~PinGDataFileFunction();

 private:
  // Actually do the pinning of each file.
  virtual bool DoOperation(const FilePath& path) OVERRIDE;

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
  void GetLocalPathsResponseOnUIThread(const FilePathList& files);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getFileLocations");
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_PRIVATE_API_H_
