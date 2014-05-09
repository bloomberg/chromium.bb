// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader_chrome.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_switches.h"
#include "components/feedback/feedback_uploader_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace feedback {
namespace {

const char kProtoBufMimeType[] = "application/x-protobuf";

}  // namespace

FeedbackUploaderChrome::FeedbackUploaderChrome(
    content::BrowserContext* context)
    : FeedbackUploader(context ? context->GetPath() : base::FilePath(),
                       BrowserThread::GetBlockingPool()),
      context_(context) {
  CHECK(context_);
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kFeedbackServer))
    url_ = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kFeedbackServer);
}

void FeedbackUploaderChrome::DispatchReport(const std::string& data) {
  GURL post_url(url_);

  net::URLFetcher* fetcher = net::URLFetcher::Create(
      post_url, net::URLFetcher::POST,
      new FeedbackUploaderDelegate(
          data,
          base::Bind(&FeedbackUploaderChrome::UpdateUploadTimer, AsWeakPtr()),
          base::Bind(&FeedbackUploaderChrome::RetryReport, AsWeakPtr())));

  fetcher->SetUploadData(std::string(kProtoBufMimeType), data);
  fetcher->SetRequestContext(context_->GetRequestContext());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES);
  fetcher->Start();
}

}  // namespace feedback
