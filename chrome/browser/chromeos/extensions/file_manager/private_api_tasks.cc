// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_tasks.h"

#include <set>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/mime_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserThread;
using extensions::app_file_handler_util::PathAndMimeTypeSet;
using fileapi::FileSystemURL;

namespace extensions {
namespace {

// Error messages.
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

void SniffMimeType(PathAndMimeTypeSet* path_mime_set,
                   std::vector<GURL>* file_urls) {
  PathAndMimeTypeSet sniffed_path_mime_set;
  std::vector<char> content(net::kMaxBytesToSniff);

  // For each files, sniff its MIME type if it is empty
  for (PathAndMimeTypeSet::iterator it = path_mime_set->begin();
       it != path_mime_set->end();
       ++it) {
    const base::FilePath& file_path = it->first;
    std::string mime_type = it->second;
    // Note: sniff MIME type only for local files.
    if (mime_type.empty() && !drive::util::IsUnderDriveMountPoint(file_path)) {
      int bytes_read = base::ReadFile(file_path, &content[0], content.size());
      if (bytes_read >= 0) {
        net::SniffMimeType(&content[0],
                           bytes_read,
                           net::FilePathToFileURL(file_path),
                           std::string(),  // type_hint (passes no hint)
                           &mime_type);
      }
    }
    sniffed_path_mime_set.insert(std::make_pair(file_path, mime_type));
  }
  path_mime_set->swap(sniffed_path_mime_set);
}

}  // namespace

bool FileBrowserPrivateExecuteTaskFunction::RunImpl() {
  using extensions::api::file_browser_private::ExecuteTask::Params;
  using extensions::api::file_browser_private::ExecuteTask::Results::Create;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  file_manager::file_tasks::TaskDescriptor task;
  if (!file_manager::file_tasks::ParseTaskID(params->task_id, &task)) {
    LOG(WARNING) << "Invalid task " << params->task_id;
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

bool FileBrowserPrivateGetFileTasksFunction::RunImpl() {
  using extensions::api::file_browser_private::GetFileTasks::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->file_urls.empty())
    return false;

  // MIME types can either be empty, or there needs to be one for each file.
  if (params->mime_types.size() != params->file_urls.size() &&
      params->mime_types.size() != 0)
    return false;

  const scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  scoped_ptr<PathAndMimeTypeSet> path_mime_set(new PathAndMimeTypeSet);
  scoped_ptr<std::vector<GURL> > file_urls(new std::vector<GURL>);
  for (size_t i = 0; i < params->file_urls.size(); ++i) {
    std::string mime_type;
    if (params->mime_types.size() != 0)
      mime_type = params->mime_types[i];

    const GURL file_url(params->file_urls[i]);
    fileapi::FileSystemURL file_system_url(
        file_system_context->CrackURL(file_url));
    if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url))
      continue;
    const base::FilePath file_path = file_system_url.path();

    file_urls->push_back(file_url);

    // If MIME type is not provided, guess it from the file path.
    if (mime_type.empty())
      mime_type = file_manager::util::GetMimeTypeForPath(file_path);

    path_mime_set->insert(std::make_pair(file_path, mime_type));
  }

  // In case the MIME type of some files are empty,
  // try to sniff their MIME type by their content.
  PathAndMimeTypeSet* path_mime_set_ptr = path_mime_set.get();
  std::vector<GURL>* file_urls_ptr = file_urls.get();

  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&SniffMimeType, path_mime_set_ptr, file_urls_ptr),
      base::Bind(
          &FileBrowserPrivateGetFileTasksFunction::OnSniffingMimeTypeCompleted,
          this,
          base::Passed(&path_mime_set),
          base::Passed(&file_urls)));
  return true;
}

void FileBrowserPrivateGetFileTasksFunction::OnSniffingMimeTypeCompleted(
    scoped_ptr<PathAndMimeTypeSet> path_mime_set,
    scoped_ptr<std::vector<GURL> > file_urls) {
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks;
  file_manager::file_tasks::FindAllTypesOfTasks(
      GetProfile(),
      drive::util::GetDriveAppRegistryByProfile(GetProfile()),
      *path_mime_set,
      *file_urls,
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

bool FileBrowserPrivateSetDefaultTaskFunction::RunImpl() {
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
