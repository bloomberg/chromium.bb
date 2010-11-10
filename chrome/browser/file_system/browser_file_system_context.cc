// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/browser_file_system_context.h"

#include "base/file_path.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "webkit/fileapi/file_system_quota_manager.h"

BrowserFileSystemContext::BrowserFileSystemContext(
    const FilePath& profile_path, bool is_incognito)
  : fileapi::SandboxedFileSystemContext(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        profile_path,
        is_incognito,
        CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAllowFileAccessFromFiles),
        CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kUnlimitedQuotaForFiles)) {
}

void BrowserFileSystemContext::SetOriginQuotaUnlimited(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  quota_manager()->SetOriginQuotaUnlimited(url);
}

void BrowserFileSystemContext::ResetOriginQuotaUnlimited(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  quota_manager()->ResetOriginQuotaUnlimited(url);
}

BrowserFileSystemContext::~BrowserFileSystemContext() {}
