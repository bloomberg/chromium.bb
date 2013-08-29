// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_tasks.h"

#include "chrome/browser/chromeos/drive/drive_app_registry.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_handlers.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/extensions/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/mime_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserContext;
using extensions::app_file_handler_util::FindFileHandlersForFiles;
using extensions::Extension;
using fileapi::FileSystemURL;

namespace file_manager {
namespace {

// Error messages.
const char kInvalidFileUrl[] = "Invalid file URL";

// Default icon path for drive docs.
const char kDefaultIcon[] = "images/filetype_generic.png";

// Logs the default task for debugging.
void LogDefaultTask(const std::set<std::string>& mime_types,
                    const std::set<std::string>& suffixes,
                    const std::string& task_id) {
  if (!mime_types.empty()) {
    std::string mime_types_str;
    for (std::set<std::string>::const_iterator iter = mime_types.begin();
         iter != mime_types.end(); ++iter) {
      if (iter == mime_types.begin()) {
        mime_types_str = *iter;
      } else {
        mime_types_str += ", " + *iter;
      }
    }
    VLOG(1) << "Associating task " << task_id
            << " with the following MIME types: ";
    VLOG(1) << "  " << mime_types_str;
  }

  if (!suffixes.empty()) {
    std::string suffixes_str;
    for (std::set<std::string>::const_iterator iter = suffixes.begin();
         iter != suffixes.end(); ++iter) {
      if (iter == suffixes.begin()) {
        suffixes_str = *iter;
      } else {
        suffixes_str += ", " + *iter;
      }
    }
    VLOG(1) << "Associating task " << task_id
            << " with the following suffixes: ";
    VLOG(1) << "  " << suffixes_str;
  }
}

// Make a set of unique filename suffixes out of the list of file URLs.
std::set<std::string> GetUniqueSuffixes(base::ListValue* file_url_list,
                                        fileapi::FileSystemContext* context) {
  std::set<std::string> suffixes;
  for (size_t i = 0; i < file_url_list->GetSize(); ++i) {
    std::string url_str;
    if (!file_url_list->GetString(i, &url_str))
      return std::set<std::string>();
    FileSystemURL url = context->CrackURL(GURL(url_str));
    if (!url.is_valid() || url.path().empty())
      return std::set<std::string>();
    // We'll skip empty suffixes.
    if (!url.path().Extension().empty())
      suffixes.insert(url.path().Extension());
  }
  return suffixes;
}

// Make a set of unique MIME types out of the list of MIME types.
std::set<std::string> GetUniqueMimeTypes(base::ListValue* mime_type_list) {
  std::set<std::string> mime_types;
  for (size_t i = 0; i < mime_type_list->GetSize(); ++i) {
    std::string mime_type;
    if (!mime_type_list->GetString(i, &mime_type))
      return std::set<std::string>();
    // We'll skip empty MIME types.
    if (!mime_type.empty())
      mime_types.insert(mime_type);
  }
  return mime_types;
}

}  // namespace

ExecuteTaskFunction::ExecuteTaskFunction() {
}

ExecuteTaskFunction::~ExecuteTaskFunction() {
}

bool ExecuteTaskFunction::RunImpl() {
  // First param is task id that was to the extension with getFileTasks call.
  std::string task_id;
  if (!args_->GetString(0, &task_id) || !task_id.size())
    return false;

  // TODO(kaznacheev): Crack the task_id here, store it in the Executor
  // and avoid passing it around.

  // The second param is the list of files that need to be executed with this
  // task.
  ListValue* files_list = NULL;
  if (!args_->GetList(1, &files_list))
    return false;

  file_tasks::TaskDescriptor task;
  if (!file_tasks::ParseTaskID(task_id, &task)) {
    LOG(WARNING) << "Invalid task " << task_id;
    return false;
  }

  if (!files_list->GetSize())
    return true;

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      util::GetFileSystemContextForRenderViewHost(
          profile(), render_view_host());

  std::vector<FileSystemURL> file_urls;
  for (size_t i = 0; i < files_list->GetSize(); i++) {
    std::string file_url_str;
    if (!files_list->GetString(i, &file_url_str)) {
      error_ = kInvalidFileUrl;
      return false;
    }
    FileSystemURL url = file_system_context->CrackURL(GURL(file_url_str));
    if (!chromeos::FileSystemBackend::CanHandleURL(url)) {
      error_ = kInvalidFileUrl;
      return false;
    }
    file_urls.push_back(url);
  }

  int32 tab_id = util::GetTabId(dispatcher());
  return file_tasks::ExecuteFileTask(
      profile(),
      source_url(),
      extension_->id(),
      tab_id,
      task,
      file_urls,
      base::Bind(&ExecuteTaskFunction::OnTaskExecuted, this));
}

void ExecuteTaskFunction::OnTaskExecuted(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
}

struct GetFileTasksFunction::TaskInfo {
  TaskInfo(const string16& app_name, const GURL& icon_url)
      : app_name(app_name), icon_url(icon_url) {
  }

  string16 app_name;
  GURL icon_url;
};

GetFileTasksFunction::GetFileTasksFunction() {
}

GetFileTasksFunction::~GetFileTasksFunction() {
}

// static
void GetFileTasksFunction::GetAvailableDriveTasks(
    const drive::DriveAppRegistry& drive_app_registry,
    const PathAndMimeTypeSet& path_mime_set,
    TaskInfoMap* task_info_map) {
  DCHECK(task_info_map);
  DCHECK(task_info_map->empty());

  bool is_first = true;
  for (PathAndMimeTypeSet::const_iterator it = path_mime_set.begin();
       it != path_mime_set.end(); ++it) {
    const base::FilePath& file_path = it->first;
    const std::string& mime_type = it->second;
    if (file_path.empty())
      continue;

    ScopedVector<drive::DriveAppInfo> app_info_list;
    drive_app_registry.GetAppsForFile(file_path, mime_type, &app_info_list);

    if (is_first) {
      // For the first file, we store all the info.
      for (size_t j = 0; j < app_info_list.size(); ++j) {
        const drive::DriveAppInfo& app_info = *app_info_list[j];
        GURL icon_url = drive::util::FindPreferredIcon(
            app_info.app_icons,
            drive::util::kPreferredIconSize);
        task_info_map->insert(std::pair<std::string, TaskInfo>(
            file_tasks::MakeDriveAppTaskId(app_info.app_id),
            TaskInfo(app_info.app_name, icon_url)));
      }
    } else {
      // For remaining files, take the intersection with the current result,
      // based on the task id.
      std::set<std::string> task_id_set;
      for (size_t j = 0; j < app_info_list.size(); ++j) {
        task_id_set.insert(
            file_tasks::MakeDriveAppTaskId(app_info_list[j]->app_id));
      }
      for (TaskInfoMap::iterator iter = task_info_map->begin();
           iter != task_info_map->end(); ) {
        if (task_id_set.find(iter->first) == task_id_set.end()) {
          task_info_map->erase(iter++);
        } else {
          ++iter;
        }
      }
    }

    is_first = false;
  }
}

// static
void GetFileTasksFunction::FindDefaultDriveTasks(
    Profile* profile,
    const PathAndMimeTypeSet& path_mime_set,
    const TaskInfoMap& task_info_map,
    std::set<std::string>* default_tasks) {
  DCHECK(default_tasks);

  for (PathAndMimeTypeSet::const_iterator it = path_mime_set.begin();
       it != path_mime_set.end(); ++it) {
    const base::FilePath& file_path = it->first;
    const std::string& mime_type = it->second;
    std::string task_id = file_tasks::GetDefaultTaskIdFromPrefs(
        profile, mime_type, file_path.Extension());
    if (task_info_map.find(task_id) != task_info_map.end())
      default_tasks->insert(task_id);
  }
}

// static
void GetFileTasksFunction::CreateDriveTasks(
    const TaskInfoMap& task_info_map,
    const std::set<std::string>& default_tasks,
    ListValue* result_list,
    bool* default_already_set) {
  DCHECK(result_list);
  DCHECK(default_already_set);

  for (TaskInfoMap::const_iterator iter = task_info_map.begin();
       iter != task_info_map.end(); ++iter) {
    DictionaryValue* task = new DictionaryValue;
    task->SetString("taskId", iter->first);
    task->SetString("title", iter->second.app_name);

    const GURL& icon_url = iter->second.icon_url;
    if (!icon_url.is_empty())
      task->SetString("iconUrl", icon_url.spec());

    task->SetBoolean("driveApp", true);

    // Once we set a default app, we don't want to set any more.
    if (!(*default_already_set) &&
        default_tasks.find(iter->first) != default_tasks.end()) {
      task->SetBoolean("isDefault", true);
      *default_already_set = true;
    } else {
      task->SetBoolean("isDefault", false);
    }
    result_list->Append(task);
  }
}

// static
void GetFileTasksFunction::FindDriveAppTasks(
    Profile* profile,
    const PathAndMimeTypeSet& path_mime_set,
    ListValue* result_list,
    bool* default_already_set) {
  DCHECK(!path_mime_set.empty());
  DCHECK(result_list);
  DCHECK(default_already_set);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile);
  // |integration_service| is NULL if Drive is disabled.
  if (!integration_service || !integration_service->drive_app_registry())
    return;

  // Map of task_id to TaskInfo of available tasks.
  TaskInfoMap task_info_map;
  GetAvailableDriveTasks(*integration_service->drive_app_registry(),
                         path_mime_set,
                         &task_info_map);

  std::set<std::string> default_tasks;
  FindDefaultDriveTasks(profile,
                        path_mime_set,
                        task_info_map,
                        &default_tasks);
  CreateDriveTasks(
      task_info_map, default_tasks, result_list, default_already_set);
}

void GetFileTasksFunction::FindFileHandlerTasks(
    Profile* profile,
    const PathAndMimeTypeSet& path_mime_set,
    ListValue* result_list,
    bool* default_already_set) {
  DCHECK(!path_mime_set.empty());
  DCHECK(result_list);
  DCHECK(default_already_set);

  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return;

  std::set<std::string> default_tasks;
  for (PathAndMimeTypeSet::iterator it = path_mime_set.begin();
       it != path_mime_set.end(); ++it) {
    default_tasks.insert(file_tasks::GetDefaultTaskIdFromPrefs(
        profile, it->second, it->first.Extension()));
  }

  for (ExtensionSet::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = iter->get();

    // We don't support using hosted apps to open files.
    if (!extension->is_platform_app())
      continue;

    if (profile->IsOffTheRecord() &&
        !service->IsIncognitoEnabled(extension->id()))
      continue;

    typedef std::vector<const extensions::FileHandlerInfo*> FileHandlerList;
    FileHandlerList file_handlers =
        FindFileHandlersForFiles(*extension, path_mime_set);
    if (file_handlers.empty())
      continue;

    for (FileHandlerList::iterator i = file_handlers.begin();
         i != file_handlers.end(); ++i) {
      DictionaryValue* task = new DictionaryValue;
      std::string task_id = file_tasks::MakeTaskID(
          extension->id(), file_tasks::TASK_TYPE_FILE_HANDLER, (*i)->id);
      task->SetString("taskId", task_id);
      task->SetString("title", (*i)->title);
      if (!(*default_already_set) && ContainsKey(default_tasks, task_id)) {
        task->SetBoolean("isDefault", true);
        *default_already_set = true;
      } else {
        task->SetBoolean("isDefault", false);
      }

      GURL best_icon = extensions::ExtensionIconSource::GetIconURL(
          extension,
          drive::util::kPreferredIconSize,
          ExtensionIconSet::MATCH_BIGGER,
          false,  // grayscale
          NULL);  // exists
      if (!best_icon.is_empty())
        task->SetString("iconUrl", best_icon.spec());
      else
        task->SetString("iconUrl", kDefaultIcon);

      task->SetBoolean("driveApp", false);
      result_list->Append(task);
    }
  }
}

void GetFileTasksFunction::FindFileBrowserHandlerTasks(
    Profile* profile,
    const std::vector<GURL>& file_urls,
    const std::vector<base::FilePath>& file_paths,
    ListValue* result_list,
    bool* default_already_set) {
  DCHECK(!file_paths.empty());
  DCHECK(!file_urls.empty());
  DCHECK(result_list);
  DCHECK(default_already_set);

  file_browser_handlers::FileBrowserHandlerList common_tasks =
      file_browser_handlers::FindCommonFileBrowserHandlers(profile, file_urls);
  if (common_tasks.empty())
    return;
  file_browser_handlers::FileBrowserHandlerList default_tasks =
      file_browser_handlers::FindDefaultFileBrowserHandlers(
          profile, file_paths, common_tasks);

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  for (file_browser_handlers::FileBrowserHandlerList::const_iterator iter =
           common_tasks.begin();
       iter != common_tasks.end();
       ++iter) {
    const FileBrowserHandler* handler = *iter;
    const std::string extension_id = handler->extension_id();
    const Extension* extension = service->GetExtensionById(extension_id, false);
    CHECK(extension);
    DictionaryValue* task = new DictionaryValue;
    task->SetString("taskId", file_tasks::MakeTaskID(
        extension_id,
        file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
        handler->id()));
    task->SetString("title", handler->title());
    // TODO(zelidrag): Figure out how to expose icon URL that task defined in
    // manifest instead of the default extension icon.
    GURL icon = extensions::ExtensionIconSource::GetIconURL(
        extension,
        extension_misc::EXTENSION_ICON_BITTY,
        ExtensionIconSet::MATCH_BIGGER,
        false,  // grayscale
        NULL);  // exists
    task->SetString("iconUrl", icon.spec());
    task->SetBoolean("driveApp", false);

    // Only set the default if there isn't already a default set.
    if (!*default_already_set &&
        std::find(default_tasks.begin(), default_tasks.end(), *iter) !=
        default_tasks.end()) {
      task->SetBoolean("isDefault", true);
      *default_already_set = true;
    } else {
      task->SetBoolean("isDefault", false);
    }

    result_list->Append(task);
  }
}

// static
void GetFileTasksFunction::FindAllTypesOfTasks(
    Profile* profile,
    const PathAndMimeTypeSet& path_mime_set,
    const std::vector<GURL>& file_urls,
    const std::vector<base::FilePath>& file_paths,
    ListValue* result_list) {
  // Check if file_paths contain a google document.
  bool has_google_document = false;
  for (size_t i = 0; i < file_paths.size(); ++i) {
    if (google_apis::ResourceEntry::ClassifyEntryKindByFileExtension(
            file_paths[i]) &
        google_apis::ResourceEntry::KIND_OF_GOOGLE_DOCUMENT) {
      has_google_document = true;
      break;
    }
  }

  // Find the Drive app tasks first, because we want them to take precedence
  // when setting the default app.
  bool default_already_set = false;
  // Google document are not opened by drive apps but file manager.
  if (!has_google_document) {
    FindDriveAppTasks(profile,
                      path_mime_set,
                      result_list,
                      &default_already_set);
  }

  // Find and append file handler tasks. We know there aren't duplicates
  // because Drive apps and platform apps are entirely different kinds of
  // tasks.
  FindFileHandlerTasks(profile,
                       path_mime_set,
                       result_list,
                       &default_already_set);

  // Find and append file browser handler tasks. We know there aren't
  // duplicates because "file_browser_handlers" and "file_handlers" shouldn't
  // be used in the same manifest.json.
  FindFileBrowserHandlerTasks(profile,
                              file_urls,
                              file_paths,
                              result_list,
                              &default_already_set);
}

bool GetFileTasksFunction::RunImpl() {
  // First argument is the list of files to get tasks for.
  ListValue* files_list = NULL;
  if (!args_->GetList(0, &files_list))
    return false;

  if (files_list->GetSize() == 0)
    return false;

  // Second argument is the list of mime types of each of the files in the list.
  ListValue* mime_types_list = NULL;
  if (!args_->GetList(1, &mime_types_list))
    return false;

  // MIME types can either be empty, or there needs to be one for each file.
  if (mime_types_list->GetSize() != files_list->GetSize() &&
      mime_types_list->GetSize() != 0)
    return false;

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      util::GetFileSystemContextForRenderViewHost(
          profile(), render_view_host());

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  PathAndMimeTypeSet path_mime_set;
  std::vector<GURL> file_urls;
  std::vector<base::FilePath> file_paths;
  for (size_t i = 0; i < files_list->GetSize(); ++i) {
    std::string file_url_str;
    if (!files_list->GetString(i, &file_url_str))
      return false;

    std::string mime_type;
    if (mime_types_list->GetSize() != 0 &&
        !mime_types_list->GetString(i, &mime_type))
      return false;

    GURL file_url(file_url_str);
    fileapi::FileSystemURL file_system_url(
        file_system_context->CrackURL(file_url));
    if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url))
      continue;
    const base::FilePath file_path = file_system_url.path();

    file_urls.push_back(file_url);
    file_paths.push_back(file_path);

    // If MIME type is not provided, guess it from the file path.
    if (mime_type.empty())
      mime_type = util::GetMimeTypeForPath(file_path);

    path_mime_set.insert(std::make_pair(file_path, mime_type));
  }

  ListValue* result_list = new ListValue();
  SetResult(result_list);

  FindAllTypesOfTasks(profile_,
                      path_mime_set,
                      file_urls,
                      file_paths,
                      result_list);
  SendResponse(true);
  return true;
}

SetDefaultTaskFunction::SetDefaultTaskFunction() {
}

SetDefaultTaskFunction::~SetDefaultTaskFunction() {
}

bool SetDefaultTaskFunction::RunImpl() {
  // First param is task id that was to the extension with setDefaultTask call.
  std::string task_id;
  if (!args_->GetString(0, &task_id) || !task_id.size())
    return false;

  base::ListValue* file_url_list;
  if (!args_->GetList(1, &file_url_list))
    return false;

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      util::GetFileSystemContextForRenderViewHost(
          profile(), render_view_host());

  std::set<std::string> suffixes =
      GetUniqueSuffixes(file_url_list, file_system_context.get());

  // MIME types are an optional parameter.
  base::ListValue* mime_type_list;
  std::set<std::string> mime_types;
  if (args_->GetList(2, &mime_type_list) && !mime_type_list->empty()) {
    if (mime_type_list->GetSize() != file_url_list->GetSize())
      return false;
    mime_types = GetUniqueMimeTypes(mime_type_list);
  }

  if (VLOG_IS_ON(1))
    LogDefaultTask(mime_types, suffixes, task_id);

  // If there weren't any mime_types, and all the suffixes were blank,
  // then we "succeed", but don't actually associate with anything.
  // Otherwise, any time we set the default on a file with no extension
  // on the local drive, we'd fail.
  // TODO(gspencer): Fix file manager so that it never tries to set default in
  // cases where extensionless local files are part of the selection.
  if (suffixes.empty() && mime_types.empty()) {
    SetResult(new base::FundamentalValue(true));
    return true;
  }

  file_tasks::UpdateDefaultTask(profile_, task_id, suffixes, mime_types);

  return true;
}

}  // namespace file_manager
