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
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"

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

 private:
  typedef extensions::app_file_handler_util::PathAndMimeTypeSet
      PathAndMimeTypeSet;

  // Holds fields to build a task result.
  struct TaskInfo;

  // Map from a task id to TaskInfo.
  typedef std::map<std::string, TaskInfo> TaskInfoMap;

  // Looks up available apps for each file in |path_mime_set| in the
  // |registry|, and returns the intersection of all available apps as a
  // map from task id to TaskInfo.
  static void GetAvailableDriveTasks(const drive::DriveAppRegistry& registry,
                                     const PathAndMimeTypeSet& path_mime_set,
                                     TaskInfoMap* task_info_map);

  // Looks in the preferences and finds any of the available apps that are
  // also listed as default apps for any of the files in the info list.
  static void FindDefaultDriveTasks(const PrefService& pref_service,
                                    const PathAndMimeTypeSet& path_mime_set,
                                    const TaskInfoMap& task_info_map,
                                    std::set<std::string>* default_tasks);

  // Creates a list of each task in |task_info_map| and stores the result into
  // |result_list|. If a default task is set in the result list,
  // |default_already_set| is set to true.
  static void CreateDriveTasks(const TaskInfoMap& task_info_map,
                               const std::set<std::string>& default_tasks,
                               ListValue* result_list,
                               bool* default_already_set);

  // Finds the drive app tasks that can be used with the given files, and
  // append them to the |result_list|. |*default_already_set| indicates if
  // the |result_list| already contains the default task. If the value is
  // false, the function will find the default task and set the value to true
  // if found.
  //
  // "taskId" field in |result_list| will look like
  // "<drive-app-id>|drive|open-with" (See also file_tasks.h).
  // "driveApp" field in |result_list| will be set to "true".
  static void FindDriveAppTasks(Profile* profile,
                                const PathAndMimeTypeSet& path_mime_set,
                                ListValue* result_list,
                                bool* default_already_set);

  // Finds the file handler tasks (apps declaring "file_handlers" in
  // manifest.json) that can be used with the given files, appending them to
  // the |result_list|. See the comment at FindDriveAppTasks() about
  // |default_already_set|
  static void FindFileHandlerTasks(Profile* profile,
                                   const PathAndMimeTypeSet& path_mime_set,
                                   ListValue* result_list,
                                   bool* default_already_set);

  // Finds the file browser handler tasks (app/extensions declaring
  // "file_browser_handlers" in manifest.json) that can be used with the
  // given files, appending them to the |result_list|. See the comment at
  // FindDriveAppTasks() about |default_already_set|
  static void FindFileBrowserHandlerTasks(
      Profile* profile,
      const std::vector<GURL>& file_urls,
      const std::vector<base::FilePath>& file_paths,
      ListValue* result_list,
      bool* default_already_set);

  // Finds all types (drive, file handlers, file browser handlers) of
  // tasks. See the comment at FindDriveAppTasks() about |result_list|.
  static void FindAllTypesOfTasks(
    Profile* profile,
    const PathAndMimeTypeSet& path_mime_set,
    const std::vector<GURL>& file_urls,
    const std::vector<base::FilePath>& file_paths,
    ListValue* result_list);
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
