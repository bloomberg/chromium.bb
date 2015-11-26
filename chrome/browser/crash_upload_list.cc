// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "chrome/browser/crash_upload_list_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/crash_upload_list_mac.h"
#endif

scoped_refptr<CrashUploadList> CreateCrashUploadList(
    UploadList::Delegate* delegate) {
  base::FilePath crash_dir_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);
#if defined(OS_WIN)
  return new CrashUploadListWin(delegate, upload_log_path,
                                content::BrowserThread::GetBlockingPool());
#elif defined(OS_MACOSX)
  return new CrashUploadListMac(delegate, upload_log_path,
                                content::BrowserThread::GetBlockingPool());
#else
  return new CrashUploadList(delegate, upload_log_path,
                             content::BrowserThread::GetBlockingPool());
#endif
}
