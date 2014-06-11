// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_data.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/feedback/feedback_util.h"
#include "components/feedback/tracing_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace feedback {
namespace {

const char kTraceFilename[] = "tracing.zip\n";
const char kPerformanceCategoryTag[] = "Performance";

const base::FilePath::CharType kHistogramsFilename[] =
    FILE_PATH_LITERAL("histograms.txt");

const char kHistogramsAttachmentName[] = "histograms.zip";

}  // namespace

FeedbackData::FeedbackData()
    : send_report_(base::Bind(&feedback_util::SendReport)), context_(NULL),
      trace_id_(0), pending_op_count_(1), report_sent_(false) {}

FeedbackData::~FeedbackData() {
}

void FeedbackData::OnFeedbackPageDataComplete() {
  pending_op_count_--;
  SendReport();
}

void FeedbackData::SetAndCompressSystemInfo(
    scoped_ptr<FeedbackData::SystemLogsMap> sys_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (trace_id_ != 0) {
    TracingManager* manager = TracingManager::Get();
    ++pending_op_count_;
    if (!manager ||
        !manager->GetTraceData(
            trace_id_,
            base::Bind(&FeedbackData::OnGetTraceData, this, trace_id_))) {
      pending_op_count_--;
      trace_id_ = 0;
    }
  }

  if (sys_info.get()) {
    ++pending_op_count_;
    AddLogs(sys_info.Pass());
    BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&FeedbackCommon::CompressLogs, this),
        base::Bind(&FeedbackData::OnCompressComplete, this));
  }
}

void FeedbackData::SetAndCompressHistograms(
    scoped_ptr<std::string> histograms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!histograms.get())
    return;
  ++pending_op_count_;
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&FeedbackCommon::CompressFile,
                 this,
                 base::FilePath(kHistogramsFilename),
                 kHistogramsAttachmentName,
                 base::Passed(&histograms)),
      base::Bind(&FeedbackData::OnCompressComplete, this));
}

void FeedbackData::AttachAndCompressFileData(
    scoped_ptr<std::string> attached_filedata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!attached_filedata.get() || attached_filedata->empty())
    return;
  ++pending_op_count_;
#if defined(OS_WIN)
  base::FilePath attached_file(base::UTF8ToWide(attached_filename_));
#else
  base::FilePath attached_file(attached_filename_);
#endif
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&FeedbackCommon::CompressFile,
                 this,
                 attached_file,
                 std::string(),
                 base::Passed(&attached_filedata)),
      base::Bind(&FeedbackData::OnCompressComplete, this));
}

void FeedbackData::OnGetTraceData(
    int trace_id,
    scoped_refptr<base::RefCountedString> trace_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TracingManager* manager = TracingManager::Get();
  if (manager)
    manager->DiscardTraceData(trace_id);

  scoped_ptr<std::string> data(new std::string);
  data->swap(trace_data->data());

  AddFile(kTraceFilename, data.Pass());

  set_category_tag(kPerformanceCategoryTag);
  --pending_op_count_;
  trace_id_ = 0;
  SendReport();
}

void FeedbackData::OnCompressComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  --pending_op_count_;
  SendReport();
}

bool FeedbackData::IsDataComplete() {
  return pending_op_count_ == 0;
}

void FeedbackData::SendReport() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (IsDataComplete() && !report_sent_) {
    report_sent_ = true;
    send_report_.Run(this);
  }
}

}  // namespace feedback
