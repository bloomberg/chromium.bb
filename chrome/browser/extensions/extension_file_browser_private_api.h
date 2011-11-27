// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/platform_file.h"
#include "chrome/browser/extensions/extension_function.h"
#include "googleurl/src/url_util.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"

#ifdef OS_CHROMEOS
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"
#endif

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
  void RequestFileEntryOnFileThread(const GURL& source_url,
                                    const std::string& task_id,
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
    : public SyncExtensionFunction {
 public:
  GetVolumeMetadataFunction();

 protected:
  virtual ~GetVolumeMetadataFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
#if defined(OS_CHROMEOS)
  std::string DeviceTypeToString(chromeos::DeviceType type);
#endif

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getVolumeMetadata");
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

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
