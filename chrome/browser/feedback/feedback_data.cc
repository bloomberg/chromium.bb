// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_data.h"

#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/feedback/tracing_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#endif

using content::BrowserThread;

#if defined(OS_CHROMEOS)
namespace {

const char kMultilineIndicatorString[] = "<multiline>\n";
const char kMultilineStartString[] = "---------- START ----------\n";
const char kMultilineEndString[] = "---------- END ----------\n\n";

const char kTraceFilename[] = "tracing.log\n";

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

void ZipLogs(chromeos::SystemLogsResponse* sys_info,
             std::string* compressed_logs) {
  DCHECK(compressed_logs);
  std::string logs_string = LogsToString(sys_info);
  if (!FeedbackUtil::ZipString(logs_string, compressed_logs)) {
    compressed_logs->clear();
  }
}

}  // namespace
#endif // OS_CHROMEOS

FeedbackData::FeedbackData() : profile_(NULL),
                               feedback_page_data_complete_(false) {
#if defined(OS_CHROMEOS)
  sys_info_.reset(NULL);
  trace_id_ = 0;
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
  if (trace_id_ != 0) {
    TracingManager* manager = TracingManager::Get();
    // When there is a trace id attached to this report, fetch it and attach it
    // as a file.  In this case, return early and retry this function later.
    if (manager &&
        manager->GetTraceData(
            trace_id_,
            base::Bind(&FeedbackData::OnGetTraceData, this))) {
      return;
    } else {
      trace_id_ = 0;
    }
  }
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
void FeedbackData::set_sys_info(
    scoped_ptr<chromeos::SystemLogsResponse> sys_info) {
  if (sys_info.get())
    CompressSyslogs(sys_info.Pass());
}

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
  chromeos::ScrubbedSystemLogsFetcher* fetcher =
      new chromeos::ScrubbedSystemLogsFetcher();
  fetcher->Fetch(base::Bind(&FeedbackData::CompressSyslogs, this));
}

// private
void FeedbackData::ReadAttachedFile(const base::FilePath& from) {
  if (!file_util::ReadFileToString(from, attached_filedata_.get())) {
    if (attached_filedata_.get())
      attached_filedata_->clear();
  }
}

void FeedbackData::OnGetTraceData(
    scoped_refptr<base::RefCountedString> trace_data) {
  scoped_ptr<std::string> data(new std::string(trace_data->data()));

  set_attached_filename(kTraceFilename);
  set_attached_filedata(data.Pass());
  trace_id_ = 0;
  FeedbackPageDataComplete();
}

#endif
