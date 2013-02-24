// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/browser_file_system_helper.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_options.h"
#include "webkit/fileapi/file_system_task_runners.h"
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

}  // namespace content
