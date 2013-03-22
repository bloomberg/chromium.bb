// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_data.h"

#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/string_util.h"
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

#if defined(OS_CHROMEOS)
#include "chrome/common/zip.h"
#endif

using content::BrowserThread;

#if defined(OS_CHROMEOS)
namespace {

const char kLogsFilename[] = "system_logs.txt";

const char kMultilineIndicatorString[] = "<multiline>\n";
const char kMultilineStartString[] = "---------- START ----------\n";
const char kMultilineEndString[] = "---------- END ----------\n\n";

std::string LogsToString(chromeos::SystemLogsResponse* sys_info) {
  std::string syslogs_string;
  for (chromeos::SystemLogsResponse::const_iterator it = sys_info->begin();
      it != sys_info->end(); ++it) {
    std::string key = it->first;
    std::string value = it->second;

    TrimString(key, "\n ", &key);
    TrimString(value, "\n ", &value);

    if (value.find("\n") != std::string::npos) {
      syslogs_string.append(
          key + "=" + kMultilineIndicatorString +
          kMultilineStartString +
          value + "\n" +
          kMultilineEndString);
    } else {
      syslogs_string.append(key + "=" + value + "\n");
    }
  }
  return syslogs_string;
}

bool ZipString(const std::string& logs,
               std::string* compressed_logs) {
  base::FilePath temp_path;
  base::FilePath zip_file;

  // Create a temporary directory, put the logs into a file in it. Create
  // another temporary file to receive the zip file in.
  if (!file_util::CreateNewTempDirectory("", &temp_path))
    return false;
  if (file_util::WriteFile(
      temp_path.Append(kLogsFilename), logs.c_str(), logs.size()) == -1)
    return false;
  if (!file_util::CreateTemporaryFile(&zip_file))
    return false;

  if (!zip::Zip(temp_path, zip_file, false))
    return false;

  if (!file_util::ReadFileToString(zip_file, compressed_logs))
    return false;

  return true;
}

void ZipLogs(chromeos::SystemLogsResponse* sys_info,
             std::string* compressed_logs) {
  DCHECK(compressed_logs);
  std::string logs_string = LogsToString(sys_info);
  if (!ZipString(logs_string, compressed_logs)) {
    compressed_logs->clear();
  }
}

}  // namespace
#endif // OS_CHROMEOS

FeedbackData::FeedbackData() : profile_(NULL),
                               feedback_page_data_complete_(false) {
#if defined(OS_CHROMEOS)
  sys_info_.reset(NULL);
  attached_filedata_.reset(NULL);
  send_sys_info_ = true;
  read_attached_file_complete_ = false;
  syslogs_collection_complete_ = false;
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
      !attached_filedata_.get()) {
    // Read the attached file and then send this report. The allocated string
    // will be freed in FeedbackUtil::SendReport.
    attached_filedata_.reset(new std::string);

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
void FeedbackData::CompressSyslogs(
    scoped_ptr<chromeos::SystemLogsResponse> sys_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We get the pointer first since base::Passed will nullify the scoper, hence
  // it's not safe to use <scoper>.get() as a parameter to PostTaskAndReply.
  chromeos::SystemLogsResponse* sys_info_ptr = sys_info.get();
  std::string* compressed_logs_ptr = new std::string;
  scoped_ptr<std::string> compressed_logs(compressed_logs_ptr);
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&ZipLogs,
                 sys_info_ptr,
                 compressed_logs_ptr),
      base::Bind(&FeedbackData::SyslogsComplete,
                 this,
                 base::Passed(&sys_info),
                 base::Passed(&compressed_logs)));
}

void FeedbackData::SyslogsComplete(
    scoped_ptr<chromeos::SystemLogsResponse> sys_info,
    scoped_ptr<std::string> compressed_logs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (send_sys_info_) {
    sys_info_ = sys_info.Pass();
    compressed_logs_ = compressed_logs.Pass();
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
  fetcher->Fetch(base::Bind(&FeedbackData::CompressSyslogs, this));
}

// private
void FeedbackData::ReadAttachedFile(const base::FilePath& from) {
  if (!file_util::ReadFileToString(from, attached_filedata_.get())) {
    if (attached_filedata_.get())
      attached_filedata_->clear();
  }
}
#endif
