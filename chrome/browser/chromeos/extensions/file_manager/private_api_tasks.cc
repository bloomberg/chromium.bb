// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_tasks.h"

#include <set>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/api/file_handlers/mime_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/filename_util.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserThread;
using fileapi::FileSystemURL;

namespace extensions {
namespace {

// Error messages.
const char kInvalidTask[] = "Invalid task: ";
const char kInvalidFileUrl[] = "Invalid file URL";

// Make a set of unique filename suffixes out of the list of file URLs.
std::set<std::string> GetUniqueSuffixes(
    const std::vector<std::string>& file_url_list,
    const fileapi::FileSystemContext* context) {
  std::set<std::string> suffixes;
  for (size_t i = 0; i < file_url_list.size(); ++i) {
    const FileSystemURL url = context->CrackURL(GURL(file_url_list[i]));
    if (!url.is_valid() || url.path().empty())
      return std::set<std::string>();
    // We'll skip empty suffixes.
    if (!url.path().Extension().empty())
      suffixes.insert(url.path().Extension());
  }
  return suffixes;
}

// Make a set of unique MIME types out of the list of MIME types.
std::set<std::string> GetUniqueMimeTypes(
    const std::vector<std::string>& mime_type_list) {
  std::set<std::string> mime_types;
  for (size_t i = 0; i < mime_type_list.size(); ++i) {
    const std::string mime_type = mime_type_list[i];
    // We'll skip empty MIME types and existing MIME types.
    if (!mime_type.empty())
      mime_types.insert(mime_type);
  }
  return mime_types;
}

}  // namespace

bool FileBrowserPrivateExecuteTaskFunction::RunAsync() {
  using extensions::api::file_browser_private::ExecuteTask::Params;
  using extensions::api::file_browser_private::ExecuteTask::Results::Create;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  file_manager::file_tasks::TaskDescriptor task;
  if (!file_manager::file_tasks::ParseTaskID(params->task_id, &task)) {
    SetError(kInvalidTask + params->task_id);
    results_ =
        Create(extensions::api::file_browser_private::TASK_RESULT_FAILED);
    return false;
  }

  if (params->file_urls.empty()) {
    results_ = Create(extensions::api::file_browser_private::TASK_RESULT_EMPTY);
    SendResponse(true);
    return true;
  }

  const scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  std::vector<FileSystemURL> file_urls;
  for (size_t i = 0; i < params->file_urls.size(); i++) {
    const FileSystemURL url =
        file_system_context->CrackURL(GURL(params->file_urls[i]));
    if (!chromeos::FileSystemBackend::CanHandleURL(url)) {
      SetError(kInvalidFileUrl);
      results_ =
          Create(extensions::api::file_browser_private::TASK_RESULT_FAILED);
      return false;
    }
    file_urls.push_back(url);
  }

  const bool result = file_manager::file_tasks::ExecuteFileTask(
      GetProfile(),
      source_url(),
      task,
      file_urls,
      base::Bind(&FileBrowserPrivateExecuteTaskFunction::OnTaskExecuted, this));
  if (!result) {
    results_ =
        Create(extensions::api::file_browser_private::TASK_RESULT_FAILED);
  }
  return result;
}

void FileBrowserPrivateExecuteTaskFunction::OnTaskExecuted(
    extensions::api::file_browser_private::TaskResult result) {
  results_ =
      extensions::api::file_browser_private::ExecuteTask::Results::Create(
          result);
  SendResponse(result !=
               extensions::api::file_browser_private::TASK_RESULT_FAILED);
}

FileBrowserPrivateGetFileTasksFunction::
    FileBrowserPrivateGetFileTasksFunction() {
}

FileBrowserPrivateGetFileTasksFunction::
    ~FileBrowserPrivateGetFileTasksFunction() {
}

bool FileBrowserPrivateGetFileTasksFunction::RunAsync() {
  using extensions::api::file_browser_private::GetFileTasks::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->file_urls.empty())
    return false;

  const scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  for (size_t i = 0; i < params->file_urls.size(); ++i) {
    const GURL file_url(params->file_urls[i]);
    fileapi::FileSystemURL file_system_url(
        file_system_context->CrackURL(file_url));
    if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url))
      continue;
    file_urls_.push_back(file_url);
    local_paths_.push_back(file_system_url.path());
  }

  collector_.reset(new app_file_handler_util::MimeTypeCollector(GetProfile()));
  collector_->CollectForLocalPaths(
      local_paths_,
      base::Bind(&FileBrowserPrivateGetFileTasksFunction::OnMimeTypesCollected,
                 this));

  return true;
}

void FileBrowserPrivateGetFileTasksFunction::OnMimeTypesCollected(
    scoped_ptr<std::vector<std::string> > mime_types) {
  app_file_handler_util::PathAndMimeTypeSet path_mime_set;
  for (size_t i = 0; i < local_paths_.size(); ++i) {
    path_mime_set.insert(std::make_pair(local_paths_[i], (*mime_types)[i]));
  }

  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks;
  file_manager::file_tasks::FindAllTypesOfTasks(
      GetProfile(),
      drive::util::GetDriveAppRegistryByProfile(GetProfile()),
      path_mime_set,
      file_urls_,
      &tasks);

  // Convert the tasks into JSON compatible objects.
  using api::file_browser_private::FileTask;
  std::vector<linked_ptr<FileTask> > results;
  for (size_t i = 0; i < tasks.size(); ++i) {
    const file_manager::file_tasks::FullTaskDescriptor& task = tasks[i];
    const linked_ptr<FileTask> converted(new FileTask);
    converted->task_id = file_manager::file_tasks::TaskDescriptorToId(
        task.task_descriptor());
    if (!task.icon_url().is_empty())
      converted->icon_url = task.icon_url().spec();
    converted->title = task.task_title();
    converted->is_default = task.is_default();
    results.push_back(converted);
  }

  results_ = extensions::api::file_browser_private::GetFileTasks::Results::
      Create(results);
  SendResponse(true);
}

bool FileBrowserPrivateSetDefaultTaskFunction::RunSync() {
  using extensions::api::file_browser_private::SetDefaultTask::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  const std::set<std::string> suffixes =
      GetUniqueSuffixes(params->file_urls, file_system_context.get());

  // MIME types are an optional parameter.
  std::set<std::string> mime_types;
  if (params->mime_types && !params->mime_types->empty()) {
    if (params->mime_types->size() != params->file_urls.size())
      return false;
    mime_types = GetUniqueMimeTypes(*params->mime_types);
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

  file_manager::file_tasks::UpdateDefaultTask(
      GetProfile()->GetPrefs(), params->task_id, suffixes, mime_types);
  return true;
}

}  // namespace extensions
