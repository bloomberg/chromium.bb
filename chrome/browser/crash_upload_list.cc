// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#if defined(OS_WIN)
#include "chrome/browser/crash_upload_list_win.h"
#endif
#include "components/breakpad/common/breakpad_paths.h"

// static
const char* CrashUploadList::kReporterLogFilename = "uploads.log";

// static
CrashUploadList* CrashUploadList::Create(Delegate* delegate) {
  base::FilePath crash_dir_path;
  PathService::Get(breakpad::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(kReporterLogFilename);
#if defined(OS_WIN)
  return new CrashUploadListWin(delegate, upload_log_path);
#else
  return new CrashUploadList(delegate, upload_log_path);
#endif
}

CrashUploadList::CrashUploadList(Delegate* delegate,
                                 const base::FilePath& upload_log_path)
    : UploadList(delegate, upload_log_path) {}

CrashUploadList::~CrashUploadList() {}
