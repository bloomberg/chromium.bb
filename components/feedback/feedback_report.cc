// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_report.h"

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/guid.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"

namespace {

constexpr base::FilePath::CharType kFeedbackReportFilenameWildcard[] =
    FILE_PATH_LITERAL("Feedback Report.*");

constexpr char kFeedbackReportFilenamePrefix[] = "Feedback Report.";

void WriteReportOnBlockingPool(const base::FilePath reports_path,
                               const base::FilePath& file,
                               const std::string& data) {
  DCHECK(reports_path.IsParent(file));
  if (!base::DirectoryExists(reports_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(reports_path, &error))
      return;
  }
  base::ImportantFileWriter::WriteFileAtomically(file, data, "FeedbackReport");
}

}  // namespace

namespace feedback {

FeedbackReport::FeedbackReport(
    const base::FilePath& path,
    const base::Time& upload_at,
    const std::string& data,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : reports_path_(path),
      upload_at_(upload_at),
      data_(data),
      reports_task_runner_(task_runner) {
  if (reports_path_.empty())
    return;
  file_ = reports_path_.AppendASCII(
      kFeedbackReportFilenamePrefix + base::GenerateGUID());

  reports_task_runner_->PostTask(FROM_HERE, base::Bind(
      &WriteReportOnBlockingPool, reports_path_, file_, data_));
}

// static
const char FeedbackReport::kCrashReportIdsKey[]  = "crash_report_ids";

// static
void FeedbackReport::LoadReportsAndQueue(
    const base::FilePath& user_dir, QueueCallback callback) {
  if (user_dir.empty())
    return;

  base::FileEnumerator enumerator(user_dir,
                                  false,
                                  base::FileEnumerator::FILES,
                                  kFeedbackReportFilenameWildcard);
  for (base::FilePath name = enumerator.Next();
       !name.empty();
       name = enumerator.Next()) {
    std::string data;
    if (ReadFileToString(name, &data))
      callback.Run(data);
    base::DeleteFile(name, false);
  }
}

void FeedbackReport::DeleteReportOnDisk() {
  reports_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::DeleteFile), file_, false));
}

FeedbackReport::~FeedbackReport() {}

}  // namespace feedback
