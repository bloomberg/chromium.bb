// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides task related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_TASKS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_TASKS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"

class PrefService;

namespace drive {
class DriveAppRegistry;
}

namespace file_manager {

// Implements the chrome.fileBrowserPrivate.executeTask method.
class ExecuteTaskFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.executeTask",
                             FILEBROWSERPRIVATE_EXECUTETASK)

  ExecuteTaskFunction();

 protected:
  virtual ~ExecuteTaskFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  void OnTaskExecuted(bool success);
};

// Implements the chrome.fileBrowserPrivate.getFileTasks method.
class GetFileTasksFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getFileTasks",
                             FILEBROWSERPRIVATE_GETFILETASKS)

  GetFileTasksFunction();

 protected:
  virtual ~GetFileTasksFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.setDefaultTask method.
class SetDefaultTaskFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setDefaultTask",
                             FILEBROWSERPRIVATE_SETDEFAULTTASK)

  SetDefaultTaskFunction();

 protected:
  virtual ~SetDefaultTaskFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_TASKS_H_
