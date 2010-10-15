// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/file_system_host_context.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webkit_glue.h"

FileSystemHostContext::FileSystemHostContext(
    const FilePath& data_path, bool is_incognito)
    : allow_file_access_from_files_(CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAllowFileAccessFromFiles)),
      quota_manager_(new fileapi::FileSystemQuota()),
      path_manager_(new fileapi::FileSystemPathManager(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          data_path, is_incognito, allow_file_access_from_files_)) {
}

bool FileSystemHostContext::CheckOriginQuota(const GURL& url, int64 growth) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // If allow-file-access-from-files flag is explicitly given and the scheme
  // is file, always return true.
  if (url.SchemeIsFile() && allow_file_access_from_files_)
    return true;
  return quota_manager_->CheckOriginQuota(url, growth);
}

void FileSystemHostContext::SetOriginQuotaUnlimited(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  quota_manager_->SetOriginQuotaUnlimited(url);
}

void FileSystemHostContext::ResetOriginQuotaUnlimited(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  quota_manager_->ResetOriginQuotaUnlimited(url);
}
