// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/browser_file_system_helper.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_permission_policy.h"
#include "webkit/fileapi/file_system_options.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/quota_manager.h"

namespace content {
namespace {

const char kChromeScheme[] = "chrome";
const char kExtensionScheme[] = "chrome-extension";

using fileapi::FileSystemOptions;

FileSystemOptions CreateBrowserFileSystemOptions(bool is_incognito) {
  std::vector<std::string> additional_allowed_schemes;
  additional_allowed_schemes.push_back(kChromeScheme);
  additional_allowed_schemes.push_back(kExtensionScheme);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowFileAccessFromFiles)) {
    additional_allowed_schemes.push_back("file");
  }
  FileSystemOptions::ProfileMode profile_mode =
      is_incognito ? FileSystemOptions::PROFILE_MODE_INCOGNITO
                   : FileSystemOptions::PROFILE_MODE_NORMAL;
  return FileSystemOptions(profile_mode, additional_allowed_schemes);
}

}  // anonymous namespace

scoped_refptr<fileapi::FileSystemContext> CreateFileSystemContext(
        const base::FilePath& profile_path, bool is_incognito,
        fileapi::ExternalMountPoints* external_mount_points,
        quota::SpecialStoragePolicy* special_storage_policy,
        quota::QuotaManagerProxy* quota_manager_proxy) {
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(fileapi::kMediaTaskRunnerName);

  scoped_ptr<fileapi::FileSystemTaskRunners> task_runners(
      new fileapi::FileSystemTaskRunners(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          pool->GetSequencedTaskRunner(media_sequence_token)));

  return new fileapi::FileSystemContext(
      task_runners.Pass(),
      external_mount_points,
      special_storage_policy,
      quota_manager_proxy,
      profile_path,
      CreateBrowserFileSystemOptions(is_incognito));
}

bool CheckFileSystemPermissionsForProcess(
    fileapi::FileSystemContext* context, int process_id,
    const fileapi::FileSystemURL& url, int permissions,
    base::PlatformFileError* error) {
  DCHECK(error);
  *error = base::PLATFORM_FILE_OK;

  if (!url.is_valid()) {
    *error = base::PLATFORM_FILE_ERROR_INVALID_URL;
    return false;
  }

  fileapi::FileSystemMountPointProvider* mount_point_provider =
      context->GetMountPointProvider(url.type());
  if (!mount_point_provider) {
    *error = base::PLATFORM_FILE_ERROR_INVALID_URL;
    return false;
  }

  base::FilePath file_path;
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  switch (mount_point_provider->GetPermissionPolicy(url, permissions)) {
    case fileapi::FILE_PERMISSION_ALWAYS_DENY:
      *error = base::PLATFORM_FILE_ERROR_SECURITY;
      return false;
    case fileapi::FILE_PERMISSION_ALWAYS_ALLOW:
      CHECK(mount_point_provider == context->sandbox_provider());
      return true;
    case fileapi::FILE_PERMISSION_USE_FILE_PERMISSION: {
      const bool success = policy->HasPermissionsForFile(
          process_id, url.path(), permissions);
      if (!success)
        *error = base::PLATFORM_FILE_ERROR_SECURITY;
      return success;
    }
    case fileapi::FILE_PERMISSION_USE_FILESYSTEM_PERMISSION: {
      const bool success = policy->HasPermissionsForFileSystem(
          process_id, url.mount_filesystem_id(), permissions);
      if (!success)
        *error = base::PLATFORM_FILE_ERROR_SECURITY;
      return success;
    }
  }
  NOTREACHED();
  *error = base::PLATFORM_FILE_ERROR_SECURITY;
  return false;
}

void SyncGetPlatformPath(fileapi::FileSystemContext* context,
                         int process_id,
                         const GURL& path,
                         base::FilePath* platform_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(platform_path);
  *platform_path = base::FilePath();
  fileapi::FileSystemURL url(context->CrackURL(path));
  if (!url.is_valid())
    return;

  // Make sure if this file is ok to be read (in the current architecture
  // which means roughly same as the renderer is allowed to get the platform
  // path to the file).
  base::PlatformFileError error;
  if (!CheckFileSystemPermissionsForProcess(
      context, process_id, url, fileapi::kReadFilePermissions, &error))
    return;

  // This is called only by pepper plugin as of writing to get the
  // underlying platform path to upload a file in the sandboxed filesystem
  // (e.g. TEMPORARY or PERSISTENT).
  // TODO(kinuko): this hack should go away once appropriate upload-stream
  // handling based on element types is supported.
  fileapi::LocalFileSystemOperation* operation =
      context->CreateFileSystemOperation(
          url, NULL)->AsLocalFileSystemOperation();
  DCHECK(operation);
  if (!operation)
    return;

  operation->SyncGetPlatformPath(url, platform_path);

  // The path is to be attached to URLLoader so we grant read permission
  // for the file. (We first need to check if it can already be read not to
  // overwrite existing permissions)
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
          process_id, *platform_path)) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        process_id, *platform_path);
  }
}

}  // namespace content
