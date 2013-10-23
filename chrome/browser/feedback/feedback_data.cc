// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_data.h"

#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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

namespace {

const char kMultilineIndicatorString[] = "<multiline>\n";
const char kMultilineStartString[] = "---------- START ----------\n";
const char kMultilineEndString[] = "---------- END ----------\n\n";

const size_t kFeedbackMaxLength = 4 * 1024;
const size_t kFeedbackMaxLineCount = 40;

const char kTraceFilename[] = "tracing.zip\n";
const char kPerformanceCategoryTag[] = "Performance";

const char kZipExt[] = ".zip";

const base::FilePath::CharType kLogsFilename[] =
    FILE_PATH_LITERAL("system_logs.txt");

// Converts the system logs into a string that we can compress and send
// with the report. This method only converts those logs that we want in
// the compressed zip file sent with the report, hence it ignores any logs
// below the size threshold of what we want compressed.
std::string LogsToString(FeedbackData::SystemLogsMap* sys_info) {
  std::string syslogs_string;
  for (FeedbackData::SystemLogsMap::const_iterator it = sys_info->begin();
      it != sys_info->end(); ++it) {
    std::string key = it->first;
    std::string value = it->second;

    if (FeedbackData::BelowCompressionThreshold(value))
      continue;

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

void ZipFile(const base::FilePath& filename,
             const std::string& data, std::string* compressed_data) {
  if (!feedback_util::ZipString(filename, data, compressed_data))
    compressed_data->clear();
}

void ZipLogs(FeedbackData::SystemLogsMap* sys_info,
             std::string* compressed_logs) {
  DCHECK(compressed_logs);
  std::string logs_string = LogsToString(sys_info);
  if (logs_string.empty() ||
      !feedback_util::ZipString(
          base::FilePath(kLogsFilename), logs_string, compressed_logs)) {
    compressed_logs->clear();
  }
}

}  // namespace

// static
bool FeedbackData::BelowCompressionThreshold(const std::string& content) {
  if (content.length() > kFeedbackMaxLength)
    return false;
  const size_t line_count = std::count(content.begin(), content.end(), '\n');
  if (line_count > kFeedbackMaxLineCount)
    return false;
  return true;
}

FeedbackData::FeedbackData() : profile_(NULL),
                               trace_id_(0),
                               feedback_page_data_complete_(false),
                               syslogs_compression_complete_(false),
                               attached_file_compression_complete_(false),
                               report_sent_(false) {
}

FeedbackData::~FeedbackData() {
}

void FeedbackData::OnFeedbackPageDataComplete() {
  feedback_page_data_complete_ = true;
  SendReport();
}

void FeedbackData::SetAndCompressSystemInfo(
    scoped_ptr<FeedbackData::SystemLogsMap> sys_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (trace_id_ != 0) {
    TracingManager* manager = TracingManager::Get();
    if (!manager ||
        !manager->GetTraceData(
            trace_id_,
            base::Bind(&FeedbackData::OnGetTraceData, this, trace_id_))) {
      trace_id_ = 0;
    }
  }

  sys_info_ = sys_info.Pass();
  if (sys_info_.get()) {
    std::string* compressed_logs_ptr = new std::string;
    scoped_ptr<std::string> compressed_logs(compressed_logs_ptr);
    BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&ZipLogs,
                   sys_info_.get(),
                   compressed_logs_ptr),
        base::Bind(&FeedbackData::OnCompressLogsComplete,
                   this,
                   base::Passed(&compressed_logs)));
  }
}

void FeedbackData::AttachAndCompressFileData(
    scoped_ptr<std::string> attached_filedata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  attached_filedata_ = attached_filedata.Pass();

  if (!attached_filename_.empty() && attached_filedata_.get()) {
    std::string* compressed_file_ptr = new std::string;
    scoped_ptr<std::string> compressed_file(compressed_file_ptr);
#if defined(OS_WIN)
    base::FilePath attached_file(base::UTF8ToWide(attached_filename_));
#else
    base::FilePath attached_file(attached_filename_);
#endif
    BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&ZipFile,
                   attached_file,
                   *(attached_filedata_.get()),
                   compressed_file_ptr),
        base::Bind(&FeedbackData::OnCompressFileComplete,
                   this,
                   base::Passed(&compressed_file)));
  }
}

void FeedbackData::OnGetTraceData(
    int trace_id_,
    scoped_refptr<base::RefCountedString> trace_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TracingManager* manager = TracingManager::Get();
  if (manager)
    manager->DiscardTraceData(trace_id_);

  scoped_ptr<std::string> data(new std::string(trace_data->data()));

  attached_filename_ = kTraceFilename;
  attached_filedata_ = data.Pass();
  trace_id_ = 0;

  set_category_tag(kPerformanceCategoryTag);

  SendReport();
}

void FeedbackData::OnCompressLogsComplete(
    scoped_ptr<std::string> compressed_logs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  compressed_logs_ = compressed_logs.Pass();
  syslogs_compression_complete_ = true;

  SendReport();
}

void FeedbackData::OnCompressFileComplete(
    scoped_ptr<std::string> compressed_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (compressed_file.get()) {
    attached_filedata_ = compressed_file.Pass();
    attached_filename_.append(kZipExt);
    attached_file_compression_complete_ = true;
  } else {
    attached_filename_.clear();
    attached_filedata_.reset(NULL);
  }

  SendReport();
}

bool FeedbackData::IsDataComplete() {
  return (!sys_info_.get() || syslogs_compression_complete_) &&
      (!attached_filedata_.get() || attached_file_compression_complete_) &&
      !trace_id_ &&
      feedback_page_data_complete_;
}

void FeedbackData::SendReport() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (IsDataComplete() && !report_sent_) {
    report_sent_ = true;
    feedback_util::SendReport(this);
  }
}
