// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_data.h"

#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#endif

using content::BrowserThread;

FeedbackData::FeedbackData() : profile_(NULL),
                               feedback_page_data_complete_(false) {
#if defined(OS_CHROMEOS)
  sys_info_.reset(NULL);
  attached_filedata_ = NULL;
  send_sys_info_ = true;
  syslogs_collection_complete_ = false;
  read_attached_file_complete_ = false;
#endif
}

FeedbackData::~FeedbackData() {
}

bool FeedbackData::DataCollectionComplete() {
#if defined(OS_CHROMEOS)
  return (syslogs_collection_complete_ || !send_sys_info_) &&
      read_attached_file_complete_ &&
      feedback_page_data_complete_;
#else
  return feedback_page_data_complete_;
#endif
}
void FeedbackData::SendReport() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (DataCollectionComplete())
    FeedbackUtil::SendReport(this);
}

void FeedbackData::FeedbackPageDataComplete() {
#if defined(OS_CHROMEOS)
  if (attached_filename_.size() &&
      base::FilePath::IsSeparator(attached_filename_[0]) &&
      !attached_filedata_) {
    // Read the attached file and then send this report. The allocated string
    // will be freed in FeedbackUtil::SendReport.
    attached_filedata_ = new std::string;

    base::FilePath root =
        ash::Shell::GetInstance()->delegate()->
            GetCurrentBrowserContext()->GetPath();
    base::FilePath filepath = root.Append(attached_filename_.substr(1));
    attached_filename_ = filepath.BaseName().value();

    // Read the file into file_data, then call send report again with the
    // stripped filename and file data (which will skip this code path).
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&FeedbackData::ReadAttachedFile, this, filepath),
        base::Bind(&FeedbackData::ReadFileComplete, this));
  } else {
    read_attached_file_complete_ = true;
  }
#endif
  feedback_page_data_complete_ = true;
  SendReport();
}

#if defined(OS_CHROMEOS)
void FeedbackData::SyslogsComplete(
    scoped_ptr<chromeos::SystemLogsResponse> sys_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (send_sys_info_) {
    sys_info_ = sys_info.Pass();
    syslogs_collection_complete_ = true;
    SendReport();
  }
}

void FeedbackData::ReadFileComplete() {
  read_attached_file_complete_ = true;
  SendReport();
}

void FeedbackData::StartSyslogsCollection() {
  chromeos::SystemLogsFetcher* fetcher = new chromeos::SystemLogsFetcher();
  fetcher->Fetch(base::Bind(&FeedbackData::SyslogsComplete, this));
}

// private
void FeedbackData::ReadAttachedFile(const base::FilePath& from) {
  if (!file_util::ReadFileToString(from, attached_filedata_)) {
    if (attached_filedata_)
      attached_filedata_->clear();
  }
}
#endif
