// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list_crashpad.h"

#include <stddef.h>

#include <utility>

#include "base/task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "components/crash/content/app/crashpad.h"

namespace {

#if defined(OS_WIN)
typedef void (*GetCrashReportsPointer)(
    const crash_reporter::Report** reports,
    size_t* report_count);
typedef void (*RequestSingleCrashUploadPointer)(const std::string& local_id);

void GetReportsThunk(
    std::vector<crash_reporter::Report>* reports) {
  static GetCrashReportsPointer get_crash_reports = []() {
    // The crash reporting is handled by chrome_elf.dll which loads early in
    // the chrome process.
    HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
    return reinterpret_cast<GetCrashReportsPointer>(
        elf_module ? GetProcAddress(elf_module, "GetCrashReportsImpl")
                   : nullptr);
  }();

  if (get_crash_reports) {
    const crash_reporter::Report* reports_pointer;
    size_t report_count;
    get_crash_reports(&reports_pointer, &report_count);
    *reports = std::vector<crash_reporter::Report>(
        reports_pointer, reports_pointer + report_count);
  }
}

void RequestSingleCrashUploadThunk(const std::string& local_id) {
  static RequestSingleCrashUploadPointer request_single_crash_upload = []() {
    // The crash reporting is handled by chrome_elf.dll which loads early in
    // the chrome process.
    HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
    return reinterpret_cast<RequestSingleCrashUploadPointer>(
        elf_module ? GetProcAddress(elf_module, "RequestSingleCrashUploadImpl")
                   : nullptr);
  }();

  if (request_single_crash_upload)
    request_single_crash_upload(local_id);
}

#endif  // OS_WIN

UploadList::UploadInfo::State ReportUploadStateToUploadInfoState(
    crash_reporter::ReportUploadState state) {
  switch (state) {
    case crash_reporter::ReportUploadState::NotUploaded:
      return UploadList::UploadInfo::State::NotUploaded;

    case crash_reporter::ReportUploadState::Pending:
      return UploadList::UploadInfo::State::Pending;

    case crash_reporter::ReportUploadState::Pending_UserRequested:
      return UploadList::UploadInfo::State::Pending_UserRequested;

    case crash_reporter::ReportUploadState::Uploaded:
      return UploadList::UploadInfo::State::Uploaded;
  }

  NOTREACHED();
  return UploadList::UploadInfo::State::Uploaded;
}

}  // namespace

CrashUploadListCrashpad::CrashUploadListCrashpad(
    Delegate* delegate,
    scoped_refptr<base::TaskRunner> task_runner)
    : CrashUploadList(delegate, base::FilePath(), std::move(task_runner)) {}

CrashUploadListCrashpad::~CrashUploadListCrashpad() {}

void CrashUploadListCrashpad::LoadUploadList(
    std::vector<UploadList::UploadInfo>* uploads) {
  std::vector<crash_reporter::Report> reports;
#if defined(OS_WIN)
  // On Windows, we only link crash client into chrome.exe (not the dlls), and
  // it does the registration. That means the global that holds the crash report
  // database lives in the .exe, so we need to grab a pointer to a helper in the
  // exe to get our reports list.
  GetReportsThunk(&reports);
#else
  crash_reporter::GetReports(&reports);
#endif

  for (const crash_reporter::Report& report : reports) {
    uploads->push_back(
        UploadInfo(report.remote_id, base::Time::FromTimeT(report.upload_time),
                   report.local_id, base::Time::FromTimeT(report.capture_time),
                   ReportUploadStateToUploadInfoState(report.state)));
  }
}

void CrashUploadListCrashpad::RequestSingleCrashUpload(
    const std::string& local_id) {
#if defined(OS_WIN)
  // On Windows, crash reporting is handled by chrome_elf.dll, that's why we
  // can't call crash_reporter::RequestSingleCrashUpload directly.
  RequestSingleCrashUploadThunk(local_id);
#else
  crash_reporter::RequestSingleCrashUpload(local_id);
#endif
}
