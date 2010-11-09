// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/browser_file_system_context.h"

#include "base/file_path.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota_manager.h"

static inline bool GetAllowFileAccessFromFiles() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowFileAccessFromFiles);
}

BrowserFileSystemContext::BrowserFileSystemContext(
    const FilePath& data_path, bool is_incognito) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool allow_file_access_from_files =
      command_line->HasSwitch(switches::kAllowFileAccessFromFiles);
  bool unlimited_quota =
      command_line->HasSwitch(switches::kUnlimitedQuotaForFiles);
  quota_manager_.reset(new fileapi::FileSystemQuotaManager(
      allow_file_access_from_files, unlimited_quota));
  path_manager_.reset(new fileapi::FileSystemPathManager(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      data_path, is_incognito, allow_file_access_from_files));
}

bool BrowserFileSystemContext::CheckOriginQuota(const GURL& url, int64 growth) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return quota_manager_->CheckOriginQuota(url, growth);
}

void BrowserFileSystemContext::SetOriginQuotaUnlimited(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  quota_manager_->SetOriginQuotaUnlimited(url);
}

void BrowserFileSystemContext::ResetOriginQuotaUnlimited(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  quota_manager_->ResetOriginQuotaUnlimited(url);
}

BrowserFileSystemContext::~BrowserFileSystemContext() {}
