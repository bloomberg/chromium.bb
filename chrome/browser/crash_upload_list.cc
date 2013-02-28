// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#if defined(OS_WIN)
#include "chrome/browser/crash_upload_list_win.h"
#endif
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

CrashUploadList::CrashInfo::CrashInfo(const std::string& c, const base::Time& t)
    : crash_id(c), crash_time(t) {}

CrashUploadList::CrashInfo::~CrashInfo() {}

// static
CrashUploadList* CrashUploadList::Create(Delegate* delegate) {
#if defined(OS_WIN)
  return new CrashUploadListWin(delegate);
#else
  return new CrashUploadList(delegate);
#endif
}

// static
const char* CrashUploadList::kReporterLogFilename = "uploads.log";

CrashUploadList::CrashUploadList(Delegate* delegate) : delegate_(delegate) {}

CrashUploadList::~CrashUploadList() {}

void CrashUploadList::LoadCrashListAsynchronously() {
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&CrashUploadList::LoadCrashListAndInformDelegateOfCompletion,
                 this));
}

void CrashUploadList::ClearDelegate() {
  delegate_ = NULL;
}


void CrashUploadList::LoadCrashListAndInformDelegateOfCompletion() {
  LoadCrashList();
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CrashUploadList::InformDelegateOfCompletion, this));
}

void CrashUploadList::LoadCrashList() {
  base::FilePath crash_dir_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path = crash_dir_path.AppendASCII("uploads.log");
  if (file_util::PathExists(upload_log_path)) {
    std::string contents;
    file_util::ReadFileToString(upload_log_path, &contents);
    std::vector<std::string> log_entries;
    base::SplitStringAlongWhitespace(contents, &log_entries);
    ParseLogEntries(log_entries);
  }
}

void CrashUploadList::ParseLogEntries(
    const std::vector<std::string>& log_entries) {
  std::vector<std::string>::const_reverse_iterator i;
  for (i = log_entries.rbegin(); i != log_entries.rend(); ++i) {
    std::vector<std::string> components;
    base::SplitString(*i, ',', &components);
    // Skip any blank (or corrupted) lines.
    if (components.size() != 2)
      continue;
    double seconds_since_epoch;
    if (!base::StringToDouble(components[0], &seconds_since_epoch))
      continue;
    CrashInfo info(components[1], base::Time::FromDoubleT(seconds_since_epoch));
    crashes_.push_back(info);
  }
}

void CrashUploadList::InformDelegateOfCompletion() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_)
    delegate_->OnCrashListAvailable();
}

void CrashUploadList::GetUploadedCrashes(unsigned int max_count,
                                         std::vector<CrashInfo>* crashes) {
  std::copy(crashes_.begin(),
            crashes_.begin() + std::min<size_t>(crashes_.size(), max_count),
            std::back_inserter(*crashes));
}

std::vector<CrashUploadList::CrashInfo>& CrashUploadList::crashes() {
  return crashes_;
}
