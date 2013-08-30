// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_tasks.h"

#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/extensions/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/mime_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_view_host.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using extensions::app_file_handler_util::PathAndMimeTypeSet;
using extensions::Extension;
using fileapi::FileSystemURL;

namespace extensions {
namespace {

// Error messages.
const char kInvalidFileUrl[] = "Invalid file URL";

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

FileBrowserPrivateExecuteTaskFunction::FileBrowserPrivateExecuteTaskFunction() {
}

FileBrowserPrivateExecuteTaskFunction::
    ~FileBrowserPrivateExecuteTaskFunction() {
}

bool FileBrowserPrivateExecuteTaskFunction::RunImpl() {
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

  file_manager::file_tasks::TaskDescriptor task;
  if (!file_manager::file_tasks::ParseTaskID(task_id, &task)) {
    LOG(WARNING) << "Invalid task " << task_id;
    return false;
  }

  if (!files_list->GetSize())
    return true;

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
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

  int32 tab_id = file_manager::util::GetTabId(dispatcher());
  return file_manager::file_tasks::ExecuteFileTask(
      profile(),
      source_url(),
      extension_->id(),
      tab_id,
      task,
      file_urls,
      base::Bind(&FileBrowserPrivateExecuteTaskFunction::OnTaskExecuted, this));
}

void FileBrowserPrivateExecuteTaskFunction::OnTaskExecuted(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
}

FileBrowserPrivateGetFileTasksFunction::
    FileBrowserPrivateGetFileTasksFunction() {
}

FileBrowserPrivateGetFileTasksFunction::
    ~FileBrowserPrivateGetFileTasksFunction() {
}

bool FileBrowserPrivateGetFileTasksFunction::RunImpl() {
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
      file_manager::util::GetFileSystemContextForRenderViewHost(
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
      mime_type = file_manager::util::GetMimeTypeForPath(file_path);

    path_mime_set.insert(std::make_pair(file_path, mime_type));
  }

  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks;
  file_manager::file_tasks::FindAllTypesOfTasks(profile_,
                                                path_mime_set,
                                                file_urls,
                                                file_paths,
                                                &tasks);
  // Convert the tasks into JSON format.
  ListValue* result_list = new ListValue();
  for (size_t i = 0; i < tasks.size(); ++i)
    result_list->Append(tasks[i].AsDictionaryValue().release());

  SetResult(result_list);
  SendResponse(true);
  return true;
}

FileBrowserPrivateSetDefaultTaskFunction::
    FileBrowserPrivateSetDefaultTaskFunction() {
}

FileBrowserPrivateSetDefaultTaskFunction::
    ~FileBrowserPrivateSetDefaultTaskFunction() {
}

bool FileBrowserPrivateSetDefaultTaskFunction::RunImpl() {
  // First param is task id that was to the extension with setDefaultTask call.
  std::string task_id;
  if (!args_->GetString(0, &task_id) || !task_id.size())
    return false;

  base::ListValue* file_url_list;
  if (!args_->GetList(1, &file_url_list))
    return false;

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
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

  file_manager::file_tasks::UpdateDefaultTask(profile_->GetPrefs(),
                                              task_id,
                                              suffixes,
                                              mime_types);
  return true;
}

}  // namespace extensions
