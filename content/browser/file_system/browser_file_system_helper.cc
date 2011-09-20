// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system/browser_file_system_helper.h"

#include "base/file_path.h"
#include "base/command_line.h"
#include "content/common/content_switches.h"
#include "content/browser/browser_thread.h"
#include "webkit/quota/quota_manager.h"

scoped_refptr<fileapi::FileSystemContext> CreateFileSystemContext(
        const FilePath& profile_path, bool is_incognito,
        quota::SpecialStoragePolicy* special_storage_policy,
        quota::QuotaManagerProxy* quota_manager_proxy) {
  return new fileapi::FileSystemContext(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      special_storage_policy,
      quota_manager_proxy,
      profile_path,
      is_incognito,
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowFileAccessFromFiles),
      NULL);
}
