// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list_crashpad.h"

#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/common/chrome_constants.h"
#include "components/crash/content/app/crashpad.h"

namespace {

#if defined(OS_WIN)
typedef void (*GetUploadedReportsPointer)(
    const crash_reporter::UploadedReport** reports,
    size_t* report_count);

void GetUploadedReportsThunk(
    std::vector<crash_reporter::UploadedReport>* uploaded_reports) {
  static GetUploadedReportsPointer get_uploaded_reports = []() {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    return reinterpret_cast<GetUploadedReportsPointer>(
        exe_module ? GetProcAddress(exe_module, "GetUploadedReportsImpl")
                   : nullptr);
  }();

  if (get_uploaded_reports) {
    const crash_reporter::UploadedReport* reports;
    size_t report_count;
    get_uploaded_reports(&reports, &report_count);
    *uploaded_reports = std::vector<crash_reporter::UploadedReport>(
        reports, reports + report_count);
  }
}
#endif  // OS_WIN

}  // namespace

CrashUploadListCrashpad::CrashUploadListCrashpad(
    Delegate* delegate,
    const scoped_refptr<base::SequencedWorkerPool>& worker_pool)
    : CrashUploadList(delegate, base::FilePath(), worker_pool) {}

CrashUploadListCrashpad::~CrashUploadListCrashpad() {}

void CrashUploadListCrashpad::LoadUploadList() {
  std::vector<crash_reporter::UploadedReport> uploaded_reports;
#if defined(OS_WIN)
  // On Windows, we only link crash client into chrome.exe (not the dlls), and
  // it does the registration. That means the global that holds the crash report
  // database lives in the .exe, so we need to grab a pointer to a helper in the
  // exe to get our uploaded reports list.
  GetUploadedReportsThunk(&uploaded_reports);
#else
  crash_reporter::GetUploadedReports(&uploaded_reports);
#endif

  ClearUploads();
  for (const crash_reporter::UploadedReport& uploaded_report :
       uploaded_reports) {
    AppendUploadInfo(
        UploadInfo(uploaded_report.remote_id,
                   base::Time::FromTimeT(uploaded_report.creation_time),
                   uploaded_report.local_id, base::Time()));
  }
}
