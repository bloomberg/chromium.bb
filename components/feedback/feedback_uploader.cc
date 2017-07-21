// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader.h"

#include <stdint.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_switches.h"

namespace feedback {

namespace {

constexpr char kFeedbackPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";

constexpr base::FilePath::CharType kFeedbackReportPath[] =
    FILE_PATH_LITERAL("Feedback Reports");

// The minimum time to wait before uploading reports are retried. Exponential
// backoff delay is applied on successive failures.
// This value can be overriden by tests by calling
// FeedbackUploader::SetMinimumRetryDelayForTesting().
base::TimeDelta g_minimum_retry_delay = base::TimeDelta::FromMinutes(60);

GURL GetFeedbackPostGURL() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return GURL(command_line.HasSwitch(switches::kFeedbackServer)
                  ? command_line.GetSwitchValueASCII(switches::kFeedbackServer)
                  : kFeedbackPostUrl);
}

}  // namespace

FeedbackUploader::FeedbackUploader(
    const base::FilePath& path,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : feedback_reports_path_(path.Append(kFeedbackReportPath)),
      task_runner_(task_runner),
      retry_delay_(g_minimum_retry_delay),
      feedback_post_url_(GetFeedbackPostGURL()) {
  DCHECK(task_runner_);
}

FeedbackUploader::~FeedbackUploader() {}

// static
void FeedbackUploader::SetMinimumRetryDelayForTesting(base::TimeDelta delay) {
  g_minimum_retry_delay = delay;
}

void FeedbackUploader::QueueReport(const std::string& data) {
  QueueReportWithDelay(data, base::TimeDelta());
}

void FeedbackUploader::OnReportUploadSuccess() {
  retry_delay_ = g_minimum_retry_delay;
  UpdateUploadTimer();
}

void FeedbackUploader::OnReportUploadFailure(
    scoped_refptr<FeedbackReport> report) {
  // Implement a backoff delay by doubling the retry delay on each failure.
  retry_delay_ *= 2;
  report->set_upload_at(retry_delay_ + base::Time::Now());
  reports_queue_.emplace(report);
  UpdateUploadTimer();
}

bool FeedbackUploader::ReportsUploadTimeComparator::operator()(
    const scoped_refptr<FeedbackReport>& a,
    const scoped_refptr<FeedbackReport>& b) const {
  return a->upload_at() > b->upload_at();
}

void FeedbackUploader::UpdateUploadTimer() {
  if (reports_queue_.empty())
    return;

  scoped_refptr<FeedbackReport> report = reports_queue_.top();
  const base::Time now = base::Time::Now();
  if (report->upload_at() <= now) {
    reports_queue_.pop();
    DispatchReport(report);
    report->DeleteReportOnDisk();
  } else {
    // Stop the old timer and start an updated one.
    upload_timer_.Stop();
    upload_timer_.Start(
        FROM_HERE, report->upload_at() - now, this,
        &FeedbackUploader::UpdateUploadTimer);
  }
}

void FeedbackUploader::QueueReportWithDelay(const std::string& data,
                                            base::TimeDelta delay) {
  reports_queue_.emplace(base::MakeRefCounted<FeedbackReport>(
      feedback_reports_path_, base::Time::Now() + delay, data, task_runner_));
  UpdateUploadTimer();
}

}  // namespace feedback
