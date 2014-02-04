// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/feedback/feedback_report.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace feedback {
namespace {

const char kFeedbackPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";
const char kProtBufMimeType[] = "application/x-protobuf";

const int64 kRetryDelayMinutes = 60;

}  // namespace

bool FeedbackUploader::ReportsUploadTimeComparator::operator()(
    FeedbackReport* a, FeedbackReport* b) const {
  return a->upload_at() > b->upload_at();
}

FeedbackUploader::FeedbackUploader(content::BrowserContext* context)
    : context_(context),
      retry_delay_(base::TimeDelta::FromMinutes(kRetryDelayMinutes)) {
  CHECK(context_);
  dispatch_callback_ = base::Bind(&FeedbackUploader::DispatchReport,
                                  AsWeakPtr());
}

FeedbackUploader::~FeedbackUploader() {}

void FeedbackUploader::QueueReport(const std::string& data) {
  reports_queue_.push(
      new FeedbackReport(context_, base::Time::Now(), data));
  UpdateUploadTimer();
}

void FeedbackUploader::DispatchReport(const std::string& data) {
  GURL post_url;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kFeedbackServer))
    post_url = GURL(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kFeedbackServer));
  else
    post_url = GURL(kFeedbackPostUrl);

  net::URLFetcher* fetcher = net::URLFetcher::Create(
      post_url, net::URLFetcher::POST,
      new FeedbackUploaderDelegate(
          data,
          base::Bind(&FeedbackUploader::UpdateUploadTimer, AsWeakPtr()),
          base::Bind(&FeedbackUploader::RetryReport, AsWeakPtr())));

  fetcher->SetUploadData(std::string(kProtBufMimeType), data);
  fetcher->SetRequestContext(context_->GetRequestContext());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES);
  fetcher->Start();
}

void FeedbackUploader::UpdateUploadTimer() {
  if (reports_queue_.empty())
    return;

  scoped_refptr<FeedbackReport> report = reports_queue_.top();
  base::Time now = base::Time::Now();
  if (report->upload_at() <= now) {
    reports_queue_.pop();
    dispatch_callback_.Run(report->data());
    report->DeleteReportOnDisk();
  } else {
    // Stop the old timer and start an updated one.
    if (upload_timer_.IsRunning())
      upload_timer_.Stop();
    upload_timer_.Start(
        FROM_HERE, report->upload_at() - now, this,
        &FeedbackUploader::UpdateUploadTimer);
  }
}

void FeedbackUploader::RetryReport(const std::string& data) {
  reports_queue_.push(new FeedbackReport(context_,
                                         base::Time::Now() + retry_delay_,
                                         data));
  UpdateUploadTimer();
}

void FeedbackUploader::setup_for_test(
    const ReportDataCallback& dispatch_callback,
    const base::TimeDelta& retry_delay) {
  dispatch_callback_ = dispatch_callback;
  retry_delay_ = retry_delay;
}

}  // namespace feedback
