// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system/browser_file_system_helper.h"

#include "base/file_path.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"

scoped_refptr<fileapi::FileSystemContext> CreateFileSystemContext(
        const FilePath& profile_path, bool is_incognito) {
  return new fileapi::FileSystemContext(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      profile_path,
      is_incognito,
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowFileAccessFromFiles),
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUnlimitedQuotaForFiles));
}
