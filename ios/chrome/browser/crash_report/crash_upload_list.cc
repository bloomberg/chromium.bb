// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_upload_list.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/web/public/web_thread.h"

namespace ios {

scoped_refptr<CrashUploadList> CreateCrashUploadList(
    UploadList::Delegate* delegate) {
  base::FilePath crash_dir_path;
  PathService::Get(ios::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);
  return new CrashUploadList(delegate, upload_log_path,
                             web::WebThread::GetBlockingPool());
}

}  // namespace ios
