// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/drive/file_task_executor.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_handlers.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using extensions::Extension;
using fileapi::FileSystemURL;

namespace file_manager {
namespace file_tasks {

namespace {

// The values "file" and "app" are confusing, but cannot be changed easily as
// these are used in default task IDs stored in preferences.
//
// TODO(satorux): We should rename them to "file_browser_handler" and
// "file_handler" respectively when switching from preferences to
// chrome.storage crbug.com/267359
const char kFileBrowserHandlerTaskType[] = "file";
const char kFileHandlerTaskType[] = "app";
const char kDriveAppTaskType[] = "drive";

// Converts a TaskType to a string.
std::string TaskTypeToString(TaskType task_type) {
  switch (task_type) {
    case TASK_TYPE_FILE_BROWSER_HANDLER:
      return kFileBrowserHandlerTaskType;
    case TASK_TYPE_FILE_HANDLER:
      return kFileHandlerTaskType;
    case TASK_TYPE_DRIVE_APP:
      return kDriveAppTaskType;
    case TASK_TYPE_UNKNOWN:
      break;
  }
  NOTREACHED();
  return "";
}

// Converts a string to a TaskType. Returns TASK_TYPE_UNKNOWN on error.
TaskType StringToTaskType(const std::string& str) {
  if (str == kFileBrowserHandlerTaskType)
    return TASK_TYPE_FILE_BROWSER_HANDLER;
  if (str == kFileHandlerTaskType)
    return TASK_TYPE_FILE_HANDLER;
  if (str == kDriveAppTaskType)
    return TASK_TYPE_DRIVE_APP;
  return TASK_TYPE_UNKNOWN;
}

// Legacy Drive task extension prefix, used by CrackTaskID.
const char kDriveTaskExtensionPrefix[] = "drive-app:";
const size_t kDriveTaskExtensionPrefixLength =
    arraysize(kDriveTaskExtensionPrefix) - 1;

// Checks if the file browser extension has permissions for the files in its
// file system context.
bool FileBrowserHasAccessPermissionForFiles(
    Profile* profile,
    const GURL& source_url,
    const std::string& file_browser_id,
    const std::vector<FileSystemURL>& files) {
  fileapi::ExternalFileSystemBackend* backend =
      util::GetFileSystemContextForExtensionId(
          profile, file_browser_id)->external_backend();
  if (!backend)
    return false;

  for (size_t i = 0; i < files.size(); ++i) {
    // Make sure this url really being used by the right caller extension.
    if (source_url.GetOrigin() != files[i].origin())
      return false;

    if (!chromeos::FileSystemBackend::CanHandleURL(files[i]) ||
        !backend->IsAccessAllowed(files[i])) {
      return false;
    }
  }

  return true;
}

}  // namespace

void UpdateDefaultTask(Profile* profile,
                       const std::string& task_id,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types) {
  if (!profile || !profile->GetPrefs())
    return;

  if (!mime_types.empty()) {
    DictionaryPrefUpdate mime_type_pref(profile->GetPrefs(),
                                        prefs::kDefaultTasksByMimeType);
    for (std::set<std::string>::const_iterator iter = mime_types.begin();
        iter != mime_types.end(); ++iter) {
      base::StringValue* value = new base::StringValue(task_id);
      mime_type_pref->SetWithoutPathExpansion(*iter, value);
    }
  }

  if (!suffixes.empty()) {
    DictionaryPrefUpdate mime_type_pref(profile->GetPrefs(),
                                        prefs::kDefaultTasksBySuffix);
    for (std::set<std::string>::const_iterator iter = suffixes.begin();
        iter != suffixes.end(); ++iter) {
      base::StringValue* value = new base::StringValue(task_id);
      // Suffixes are case insensitive.
      std::string lower_suffix = StringToLowerASCII(*iter);
      mime_type_pref->SetWithoutPathExpansion(lower_suffix, value);
    }
  }
}

std::string GetDefaultTaskIdFromPrefs(Profile* profile,
                                      const std::string& mime_type,
                                      const std::string& suffix) {
  VLOG(1) << "Looking for default for MIME type: " << mime_type
      << " and suffix: " << suffix;
  std::string task_id;
  if (!mime_type.empty()) {
    const DictionaryValue* mime_task_prefs =
        profile->GetPrefs()->GetDictionary(prefs::kDefaultTasksByMimeType);
    DCHECK(mime_task_prefs);
    LOG_IF(ERROR, !mime_task_prefs) << "Unable to open MIME type prefs";
    if (mime_task_prefs &&
        mime_task_prefs->GetStringWithoutPathExpansion(mime_type, &task_id)) {
      VLOG(1) << "Found MIME default handler: " << task_id;
      return task_id;
    }
  }

  const DictionaryValue* suffix_task_prefs =
      profile->GetPrefs()->GetDictionary(prefs::kDefaultTasksBySuffix);
  DCHECK(suffix_task_prefs);
  LOG_IF(ERROR, !suffix_task_prefs) << "Unable to open suffix prefs";
  std::string lower_suffix = StringToLowerASCII(suffix);
  if (suffix_task_prefs)
    suffix_task_prefs->GetStringWithoutPathExpansion(lower_suffix, &task_id);
  VLOG_IF(1, !task_id.empty()) << "Found suffix default handler: " << task_id;
  return task_id;
}

std::string MakeTaskID(const std::string& extension_id,
                       TaskType task_type,
                       const std::string& action_id) {
  return base::StringPrintf("%s|%s|%s",
                            extension_id.c_str(),
                            TaskTypeToString(task_type).c_str(),
                            action_id.c_str());
}

std::string MakeDriveAppTaskId(const std::string& app_id) {
  return MakeTaskID(app_id, TASK_TYPE_DRIVE_APP, "open-with");
}

bool ParseTaskID(const std::string& task_id, TaskDescriptor* task) {
  DCHECK(task);

  std::vector<std::string> result;
  int count = Tokenize(task_id, std::string("|"), &result);

  // Parse a legacy task ID that only contain two parts. Drive tasks are
  // identified by a prefix "drive-app:" on the extension ID. The legacy task
  // IDs can be stored in preferences.
  // TODO(satorux): We should get rid of this code: crbug.com/267359.
  if (count == 2) {
    if (StartsWithASCII(result[0], kDriveTaskExtensionPrefix, true)) {
      task->task_type = TASK_TYPE_DRIVE_APP;
      task->app_id = result[0].substr(kDriveTaskExtensionPrefixLength);
    } else {
      task->task_type = TASK_TYPE_FILE_BROWSER_HANDLER;
      task->app_id = result[0];
    }

    task->action_id = result[1];

    return true;
  }

  if (count != 3)
    return false;

  TaskType task_type = StringToTaskType(result[1]);
  if (task_type == TASK_TYPE_UNKNOWN)
    return false;

  task->app_id = result[0];
  task->task_type = task_type;
  task->action_id = result[2];

  return true;
}

bool ExecuteFileTask(Profile* profile,
                     const GURL& source_url,
                     const std::string& file_browser_id,
                     int32 tab_id,
                     const TaskDescriptor& task,
                     const std::vector<FileSystemURL>& file_urls,
                     const FileTaskFinishedCallback& done) {
  if (!FileBrowserHasAccessPermissionForFiles(profile, source_url,
                                              file_browser_id, file_urls))
    return false;

  // drive::FileTaskExecutor is responsible to handle drive tasks.
  if (task.task_type == TASK_TYPE_DRIVE_APP) {
    DCHECK_EQ("open-with", task.action_id);
    drive::FileTaskExecutor* executor =
        new drive::FileTaskExecutor(profile, task.app_id);
    executor->Execute(file_urls, done);
    return true;
  }

  // Get the extension.
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const Extension* extension = service ?
      service->GetExtensionById(task.app_id, false) : NULL;
  if (!extension)
    return false;

  // Execute the task.
  if (task.task_type == TASK_TYPE_FILE_BROWSER_HANDLER) {
    return file_browser_handlers::ExecuteFileBrowserHandler(
        profile,
        extension,
        tab_id,
        task.action_id,
        file_urls,
        done);
  } else if (task.task_type == TASK_TYPE_FILE_HANDLER) {
    for (size_t i = 0; i != file_urls.size(); ++i) {
      apps::LaunchPlatformAppWithFileHandler(
          profile, extension, task.action_id, file_urls[i].path());
    }

    if (!done.is_null())
      done.Run(true);
    return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace file_tasks
}  // namespace file_manager
