// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks.h"

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/file_task_executor.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/file_browser_handlers.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_app_registry.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/api/file_browser_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "webkit/browser/fileapi/file_system_url.h"

using extensions::Extension;
using extensions::app_file_handler_util::FindFileHandlersForFiles;
using storage::FileSystemURL;

namespace file_manager {
namespace file_tasks {

namespace {

// The values "file" and "app" are confusing, but cannot be changed easily as
// these are used in default task IDs stored in preferences.
const char kFileBrowserHandlerTaskType[] = "file";
const char kFileHandlerTaskType[] = "app";
const char kDriveAppTaskType[] = "drive";

// Drive apps always use the action ID.
const char kDriveAppActionID[] = "open-with";

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

// Returns true if path_mime_set contains a Google document.
bool ContainsGoogleDocument(const PathAndMimeTypeSet& path_mime_set) {
  for (PathAndMimeTypeSet::const_iterator iter = path_mime_set.begin();
       iter != path_mime_set.end(); ++iter) {
    if (drive::util::HasHostedDocumentExtension(iter->first))
      return true;
  }
  return false;
}

// Leaves tasks handled by the file manger itself as is and removes all others.
void KeepOnlyFileManagerInternalTasks(std::vector<FullTaskDescriptor>* tasks) {
  std::vector<FullTaskDescriptor> filtered;
  for (size_t i = 0; i < tasks->size(); ++i) {
    if ((*tasks)[i].task_descriptor().app_id == kFileManagerAppId)
      filtered.push_back((*tasks)[i]);
  }
  tasks->swap(filtered);
}

void ChooseSuitableGalleryHandler(std::vector<FullTaskDescriptor>* task_list) {
  const bool disable_new_gallery =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kFileManagerEnableNewGallery) == "false";
  std::vector<FullTaskDescriptor>::iterator it = task_list->begin();
  while (it != task_list->end()) {
    if (disable_new_gallery) {
      if (it->task_descriptor().app_id == kGalleryAppId)
        it = task_list->erase(it);
      else
        ++it;
    } else {
      if (it->task_descriptor().app_id == kFileManagerAppId &&
          it->task_descriptor().action_id == "gallery") {
        it = task_list->erase(it);
      } else {
        ++it;
      }
    }
  }
}

// Returns true if the given task is a handler by built-in apps like Files.app
// itself or QuickOffice etc. They are used as the initial default app.
bool IsFallbackFileHandler(const file_tasks::TaskDescriptor& task) {
  if (task.task_type != file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER &&
      task.task_type != file_tasks::TASK_TYPE_FILE_HANDLER)
    return false;

  const char* kBuiltInApps[] = {
    kFileManagerAppId,
    kVideoPlayerAppId,
    kGalleryAppId,
    extension_misc::kQuickOfficeComponentExtensionId,
    extension_misc::kQuickOfficeInternalExtensionId,
    extension_misc::kQuickOfficeExtensionId,
  };

  for (size_t i = 0; i < arraysize(kBuiltInApps); ++i) {
    if (task.app_id == kBuiltInApps[i])
      return true;
  }
  return false;
}

}  // namespace

FullTaskDescriptor::FullTaskDescriptor(
    const TaskDescriptor& task_descriptor,
    const std::string& task_title,
    const GURL& icon_url,
    bool is_default)
    : task_descriptor_(task_descriptor),
      task_title_(task_title),
      icon_url_(icon_url),
      is_default_(is_default) {
}

void UpdateDefaultTask(PrefService* pref_service,
                       const std::string& task_id,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types) {
  if (!pref_service)
    return;

  if (!mime_types.empty()) {
    DictionaryPrefUpdate mime_type_pref(pref_service,
                                        prefs::kDefaultTasksByMimeType);
    for (std::set<std::string>::const_iterator iter = mime_types.begin();
        iter != mime_types.end(); ++iter) {
      base::StringValue* value = new base::StringValue(task_id);
      mime_type_pref->SetWithoutPathExpansion(*iter, value);
    }
  }

  if (!suffixes.empty()) {
    DictionaryPrefUpdate mime_type_pref(pref_service,
                                        prefs::kDefaultTasksBySuffix);
    for (std::set<std::string>::const_iterator iter = suffixes.begin();
        iter != suffixes.end(); ++iter) {
      base::StringValue* value = new base::StringValue(task_id);
      // Suffixes are case insensitive.
      std::string lower_suffix = base::StringToLowerASCII(*iter);
      mime_type_pref->SetWithoutPathExpansion(lower_suffix, value);
    }
  }
}

std::string GetDefaultTaskIdFromPrefs(const PrefService& pref_service,
                                      const std::string& mime_type,
                                      const std::string& suffix) {
  VLOG(1) << "Looking for default for MIME type: " << mime_type
      << " and suffix: " << suffix;
  std::string task_id;
  if (!mime_type.empty()) {
    const base::DictionaryValue* mime_task_prefs =
        pref_service.GetDictionary(prefs::kDefaultTasksByMimeType);
    DCHECK(mime_task_prefs);
    LOG_IF(ERROR, !mime_task_prefs) << "Unable to open MIME type prefs";
    if (mime_task_prefs &&
        mime_task_prefs->GetStringWithoutPathExpansion(mime_type, &task_id)) {
      VLOG(1) << "Found MIME default handler: " << task_id;
      return task_id;
    }
  }

  const base::DictionaryValue* suffix_task_prefs =
      pref_service.GetDictionary(prefs::kDefaultTasksBySuffix);
  DCHECK(suffix_task_prefs);
  LOG_IF(ERROR, !suffix_task_prefs) << "Unable to open suffix prefs";
  std::string lower_suffix = base::StringToLowerASCII(suffix);
  if (suffix_task_prefs)
    suffix_task_prefs->GetStringWithoutPathExpansion(lower_suffix, &task_id);
  VLOG_IF(1, !task_id.empty()) << "Found suffix default handler: " << task_id;
  return task_id;
}

std::string MakeTaskID(const std::string& app_id,
                       TaskType task_type,
                       const std::string& action_id) {
  return base::StringPrintf("%s|%s|%s",
                            app_id.c_str(),
                            TaskTypeToString(task_type).c_str(),
                            action_id.c_str());
}

std::string TaskDescriptorToId(const TaskDescriptor& task_descriptor) {
  return MakeTaskID(task_descriptor.app_id,
                    task_descriptor.task_type,
                    task_descriptor.action_id);
}

bool ParseTaskID(const std::string& task_id, TaskDescriptor* task) {
  DCHECK(task);

  std::vector<std::string> result;
  int count = Tokenize(task_id, std::string("|"), &result);

  // Parse a legacy task ID that only contain two parts. Drive tasks are
  // identified by a prefix "drive-app:" on the extension ID. The legacy task
  // IDs can be stored in preferences.
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
                     const TaskDescriptor& task,
                     const std::vector<FileSystemURL>& file_urls,
                     const FileTaskFinishedCallback& done) {
  // drive::FileTaskExecutor is responsible to handle drive tasks.
  if (task.task_type == TASK_TYPE_DRIVE_APP) {
    DCHECK_EQ(kDriveAppActionID, task.action_id);
    drive::FileTaskExecutor* executor =
        new drive::FileTaskExecutor(profile, task.app_id);
    executor->Execute(file_urls, done);
    return true;
  }

  // Get the extension.
  const Extension* extension = extensions::ExtensionRegistry::Get(
      profile)->enabled_extensions().GetByID(task.app_id);
  if (!extension)
    return false;

  // Execute the task.
  if (task.task_type == TASK_TYPE_FILE_BROWSER_HANDLER) {
    return file_browser_handlers::ExecuteFileBrowserHandler(
        profile,
        extension,
        task.action_id,
        file_urls,
        done);
  } else if (task.task_type == TASK_TYPE_FILE_HANDLER) {
    std::vector<base::FilePath> paths;
    for (size_t i = 0; i != file_urls.size(); ++i)
      paths.push_back(file_urls[i].path());
    apps::LaunchPlatformAppWithFileHandler(
        profile, extension, task.action_id, paths);
    if (!done.is_null())
      done.Run(extensions::api::file_browser_private::TASK_RESULT_MESSAGE_SENT);
    return true;
  }
  NOTREACHED();
  return false;
}

void FindDriveAppTasks(
    const drive::DriveAppRegistry& drive_app_registry,
    const PathAndMimeTypeSet& path_mime_set,
    std::vector<FullTaskDescriptor>* result_list) {
  DCHECK(result_list);

  bool is_first = true;
  typedef std::map<std::string, drive::DriveAppInfo> DriveAppInfoMap;
  DriveAppInfoMap drive_app_map;

  for (PathAndMimeTypeSet::const_iterator it = path_mime_set.begin();
       it != path_mime_set.end(); ++it) {
    const base::FilePath& file_path = it->first;
    const std::string& mime_type = it->second;
    // Return immediately if a file not on Drive is found, as Drive app tasks
    // work only if all files are on Drive.
    if (!drive::util::IsUnderDriveMountPoint(file_path))
      return;

    std::vector<drive::DriveAppInfo> app_info_list;
    drive_app_registry.GetAppsForFile(file_path.Extension(),
                                      mime_type,
                                      &app_info_list);

    if (is_first) {
      // For the first file, we store all the info.
      for (size_t j = 0; j < app_info_list.size(); ++j)
        drive_app_map[app_info_list[j].app_id] = app_info_list[j];
    } else {
      // For remaining files, take the intersection with the current
      // result, based on the app id.
      std::set<std::string> app_id_set;
      for (size_t j = 0; j < app_info_list.size(); ++j)
        app_id_set.insert(app_info_list[j].app_id);
      for (DriveAppInfoMap::iterator iter = drive_app_map.begin();
           iter != drive_app_map.end();) {
        if (app_id_set.count(iter->first) == 0) {
          drive_app_map.erase(iter++);
        } else {
          ++iter;
        }
      }
    }

    is_first = false;
  }

  for (DriveAppInfoMap::const_iterator iter = drive_app_map.begin();
       iter != drive_app_map.end(); ++iter) {
    const drive::DriveAppInfo& app_info = iter->second;
    TaskDescriptor descriptor(app_info.app_id,
                              TASK_TYPE_DRIVE_APP,
                              kDriveAppActionID);
    GURL icon_url = drive::util::FindPreferredIcon(
        app_info.app_icons,
        drive::util::kPreferredIconSize);
    result_list->push_back(
        FullTaskDescriptor(descriptor,
                           app_info.app_name,
                           icon_url,
                           false /* is_default */));
  }
}

void FindFileHandlerTasks(
    Profile* profile,
    const PathAndMimeTypeSet& path_mime_set,
    std::vector<FullTaskDescriptor>* result_list) {
  DCHECK(!path_mime_set.empty());
  DCHECK(result_list);

  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator iter =
           enabled_extensions.begin();
       iter != enabled_extensions.end();
       ++iter) {
    const Extension* extension = iter->get();

    // We don't support using hosted apps to open files.
    if (!extension->is_platform_app())
      continue;

    // Ephemeral apps cannot be file handlers.
    if (extensions::util::IsEphemeralApp(extension->id(), profile))
      continue;

    if (profile->IsOffTheRecord() &&
        !extensions::util::IsIncognitoEnabled(extension->id(), profile))
      continue;

    typedef std::vector<const extensions::FileHandlerInfo*> FileHandlerList;
    FileHandlerList file_handlers =
        FindFileHandlersForFiles(*extension, path_mime_set);
    if (file_handlers.empty())
      continue;

    // Only show the first matching handler from each app.
    const extensions::FileHandlerInfo* file_handler = file_handlers.front();
    std::string task_id = file_tasks::MakeTaskID(
        extension->id(), file_tasks::TASK_TYPE_FILE_HANDLER, file_handler->id);

    GURL best_icon = extensions::ExtensionIconSource::GetIconURL(
        extension,
        drive::util::kPreferredIconSize,
        ExtensionIconSet::MATCH_BIGGER,
        false,  // grayscale
        NULL);  // exists

    result_list->push_back(
        FullTaskDescriptor(TaskDescriptor(extension->id(),
                                          file_tasks::TASK_TYPE_FILE_HANDLER,
                                          file_handler->id),
                           extension->name(),
                           best_icon,
                           false /* is_default */));
  }
}

void FindFileBrowserHandlerTasks(
    Profile* profile,
    const std::vector<GURL>& file_urls,
    std::vector<FullTaskDescriptor>* result_list) {
  DCHECK(!file_urls.empty());
  DCHECK(result_list);

  file_browser_handlers::FileBrowserHandlerList common_tasks =
      file_browser_handlers::FindFileBrowserHandlers(profile, file_urls);
  if (common_tasks.empty())
    return;

  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (file_browser_handlers::FileBrowserHandlerList::const_iterator iter =
           common_tasks.begin();
       iter != common_tasks.end();
       ++iter) {
    const FileBrowserHandler* handler = *iter;
    const std::string extension_id = handler->extension_id();
    const Extension* extension = enabled_extensions.GetByID(extension_id);
    DCHECK(extension);

    // TODO(zelidrag): Figure out how to expose icon URL that task defined in
    // manifest instead of the default extension icon.
    const GURL icon_url = extensions::ExtensionIconSource::GetIconURL(
        extension,
        extension_misc::EXTENSION_ICON_BITTY,
        ExtensionIconSet::MATCH_BIGGER,
        false,  // grayscale
        NULL);  // exists

    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(extension_id,
                       file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
                       handler->id()),
        handler->title(),
        icon_url,
        false /* is_default */));
  }
}

void FindAllTypesOfTasks(
    Profile* profile,
    const drive::DriveAppRegistry* drive_app_registry,
    const PathAndMimeTypeSet& path_mime_set,
    const std::vector<GURL>& file_urls,
    std::vector<FullTaskDescriptor>* result_list) {
  DCHECK(profile);
  DCHECK(result_list);

  // Find Drive app tasks, if the drive app registry is present.
  if (drive_app_registry)
    FindDriveAppTasks(*drive_app_registry, path_mime_set, result_list);

  // Find and append file handler tasks. We know there aren't duplicates
  // because Drive apps and platform apps are entirely different kinds of
  // tasks.
  FindFileHandlerTasks(profile, path_mime_set, result_list);

  // Find and append file browser handler tasks. We know there aren't
  // duplicates because "file_browser_handlers" and "file_handlers" shouldn't
  // be used in the same manifest.json.
  FindFileBrowserHandlerTasks(profile, file_urls, result_list);

  // Google documents can only be handled by internal handlers.
  if (ContainsGoogleDocument(path_mime_set))
    KeepOnlyFileManagerInternalTasks(result_list);

  ChooseSuitableGalleryHandler(result_list);
  ChooseAndSetDefaultTask(*profile->GetPrefs(), path_mime_set, result_list);
}

void ChooseAndSetDefaultTask(const PrefService& pref_service,
                             const PathAndMimeTypeSet& path_mime_set,
                             std::vector<FullTaskDescriptor>* tasks) {
  // Collect the task IDs of default tasks from the preferences into a set.
  std::set<std::string> default_task_ids;
  for (PathAndMimeTypeSet::const_iterator it = path_mime_set.begin();
       it != path_mime_set.end(); ++it) {
    const base::FilePath& file_path = it->first;
    const std::string& mime_type = it->second;
    std::string task_id = file_tasks::GetDefaultTaskIdFromPrefs(
        pref_service, mime_type, file_path.Extension());
    default_task_ids.insert(task_id);
  }

  // Go through all the tasks from the beginning and see if there is any
  // default task. If found, pick and set it as default and return.
  for (size_t i = 0; i < tasks->size(); ++i) {
    FullTaskDescriptor* task = &tasks->at(i);
    DCHECK(!task->is_default());
    const std::string task_id = TaskDescriptorToId(task->task_descriptor());
    if (ContainsKey(default_task_ids, task_id)) {
      task->set_is_default(true);
      return;
    }
  }

  // No default tasks found. If there is any fallback file browser handler,
  // make it as default task, so it's selected by default.
  for (size_t i = 0; i < tasks->size(); ++i) {
    FullTaskDescriptor* task = &tasks->at(i);
    DCHECK(!task->is_default());
    if (IsFallbackFileHandler(task->task_descriptor())) {
      task->set_is_default(true);
      return;
    }
  }
}

}  // namespace file_tasks
}  // namespace file_manager
