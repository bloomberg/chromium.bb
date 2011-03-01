// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list.h"

#include "base/path_service.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/browser_thread.h"

CrashUploadList::CrashInfo::CrashInfo(const std::string& c, const base::Time& t)
    : crash_id(c), crash_time(t) {}

CrashUploadList::CrashInfo::~CrashInfo() {}

CrashUploadList::CrashUploadList(Delegate* delegate) : delegate_(delegate) {}

CrashUploadList::~CrashUploadList() {}

void CrashUploadList::LoadCrashListAsynchronously() {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &CrashUploadList::LoadUploadLog));
}

void CrashUploadList::ClearDelegate() {
  delegate_ = NULL;
}

void CrashUploadList::LoadUploadLog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath crash_dir_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path);
  FilePath upload_log_path = crash_dir_path.AppendASCII("uploads.log");
  if (file_util::PathExists(upload_log_path)) {
    std::string contents;
    file_util::ReadFileToString(upload_log_path, &contents);
    base::SplitStringAlongWhitespace(contents, &log_entries_);
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &CrashUploadList::InformDelegateOfCompletion));
}

void CrashUploadList::InformDelegateOfCompletion() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_)
    delegate_->OnCrashListAvailable();
}

void CrashUploadList::GetUploadedCrashes(unsigned int max_count,
                                         std::vector<CrashInfo>* crashes) {
  std::vector<std::string>::reverse_iterator i;
  for (i = log_entries_.rbegin(); i != log_entries_.rend(); ++i) {
    std::vector<std::string> components;
    base::SplitString(*i, ',', &components);
    // Skip any blank (or corrupted) lines.
    if (components.size() != 2)
      continue;
    double seconds_since_epoch;
    if (!base::StringToDouble(components[0], &seconds_since_epoch))
      continue;
    CrashInfo info(components[1], base::Time::FromDoubleT(seconds_since_epoch));
    crashes->push_back(info);
  }
}
