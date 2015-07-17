// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides task related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_TASKS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_TASKS_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {

namespace app_file_handler_util {
class MimeTypeCollector;
}  // namespace app_file_handler_util

// Implements the chrome.fileManagerPrivateInternal.executeTask method.
class FileManagerPrivateInternalExecuteTaskFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.executeTask",
                             FILEMANAGERPRIVATEINTERNAL_EXECUTETASK)

 protected:
  ~FileManagerPrivateInternalExecuteTaskFunction() override {}

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnTaskExecuted(
      extensions::api::file_manager_private::TaskResult success);
};

// Implements the chrome.fileManagerPrivateInternal.getFileTasks method.
class FileManagerPrivateInternalGetFileTasksFunction
    : public LoggedAsyncExtensionFunction {
 public:
  FileManagerPrivateInternalGetFileTasksFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getFileTasks",
                             FILEMANAGERPRIVATEINTERNAL_GETFILETASKS)

 protected:
  ~FileManagerPrivateInternalGetFileTasksFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnMimeTypesCollected(scoped_ptr<std::vector<std::string> > mime_types);

  void OnSniffingMimeTypeCompleted(
      scoped_ptr<app_file_handler_util::PathAndMimeTypeSet> path_mime_set,
      scoped_ptr<std::vector<GURL>> urls);

  scoped_ptr<app_file_handler_util::MimeTypeCollector> collector_;
  std::vector<GURL> urls_;
  std::vector<base::FilePath> local_paths_;
};

// Implements the chrome.fileManagerPrivateInternal.setDefaultTask method.
class FileManagerPrivateInternalSetDefaultTaskFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.setDefaultTask",
                             FILEMANAGERPRIVATEINTERNAL_SETDEFAULTTASK)

 protected:
  ~FileManagerPrivateInternalSetDefaultTaskFunction() override {}

  // SyncExtensionFunction overrides.
  bool RunSync() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_TASKS_H_
