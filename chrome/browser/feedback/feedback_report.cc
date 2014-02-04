// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_report.h"

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/important_file_writer.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/directory_lister.h"

using content::BrowserThread;

namespace {

const base::FilePath::CharType kFeedbackReportPath[] =
    FILE_PATH_LITERAL("Feedback Reports");
const base::FilePath::CharType kFeedbackReportFilenameWildcard[] =
    FILE_PATH_LITERAL("Feedback Report.*");

const char kFeedbackReportFilenamePrefix[] = "Feedback Report.";

base::FilePath GetFeedbackReportsPath(content::BrowserContext* context) {
  if (!context)
    return base::FilePath();
  return context->GetPath().Append(kFeedbackReportPath);
}

void WriteReportOnBlockingPool(const base::FilePath reports_path,
                               const base::FilePath& file,
                               const std::string& data) {
  DCHECK(reports_path.IsParent(file));
  if (!base::DirectoryExists(reports_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(reports_path, &error))
      return;
  }
  base::ImportantFileWriter::WriteFileAtomically(file, data);
}

}  // namespace

namespace feedback {

FeedbackReport::FeedbackReport(content::BrowserContext* context,
                               const base::Time& upload_at,
                               const std::string& data)
    : upload_at_(upload_at),
      data_(data) {
  reports_path_ = GetFeedbackReportsPath(context);
  if (reports_path_.empty())
    return;
  file_ = reports_path_.AppendASCII(
      kFeedbackReportFilenamePrefix + base::GenerateGUID());

  // Uses a BLOCK_SHUTDOWN file task runner because we really don't want to
  // lose reports.
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  reports_task_runner_ = pool->GetSequencedTaskRunnerWithShutdownBehavior(
      pool->GetSequenceToken(),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);

  reports_task_runner_->PostTask(FROM_HERE, base::Bind(
      &WriteReportOnBlockingPool, reports_path_, file_, data_));
}

FeedbackReport::~FeedbackReport() {}

void FeedbackReport::DeleteReportOnDisk() {
  reports_task_runner_->PostTask(FROM_HERE, base::Bind(
      base::IgnoreResult(&base::DeleteFile), file_, false));
}

// static
void FeedbackReport::LoadReportsAndQueue(
    content::BrowserContext* context, QueueCallback callback) {
  base::FilePath reports_path = GetFeedbackReportsPath(context);
  if (reports_path.empty())
    return;

  base::FileEnumerator enumerator(reports_path,
                                  false,
                                  base::FileEnumerator::FILES,
                                  kFeedbackReportFilenameWildcard);
  for (base::FilePath name = enumerator.Next();
       !name.empty();
       name = enumerator.Next()) {
    std::string data;
    if (ReadFileToString(name, &data))
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, base::Bind(callback, data));
    base::DeleteFile(name, false);
  }
}

}  // namespace feedback
