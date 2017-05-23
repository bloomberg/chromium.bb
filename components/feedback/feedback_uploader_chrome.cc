// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader_chrome.h"

#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/task_runner_util.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_switches.h"
#include "components/feedback/feedback_uploader_delegate.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace feedback {
namespace {

const char kProtoBufMimeType[] = "application/x-protobuf";

}  // namespace

FeedbackUploaderChrome::FeedbackUploaderChrome(content::BrowserContext* context)
    : FeedbackUploader(context ? context->GetPath() : base::FilePath()),
      context_(context) {
  CHECK(context_);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kFeedbackServer))
    url_ = command_line.GetSwitchValueASCII(switches::kFeedbackServer);
}

void FeedbackUploaderChrome::DispatchReport(const std::string& data) {
  GURL post_url(url_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chrome_feedback_report_app", R"(
        semantics {
          sender: "Chrome Feedback Report App"
          description:
            "Users can press Alt+Shift+i to report a bug or a feedback in "
            "general. Along with the free-form text they entered, system logs "
            "that helps in diagnosis of the issue are sent to Google. This "
            "service uploads the report to Google Feedback server."
          trigger:
            "When user chooses to send a feedback to Google."
          data:
            "The free-form text that user has entered and useful debugging "
            "logs (UI logs, Chrome logs, kernel logs, auto update engine logs, "
            "ARC++ logs, etc.). The logs are anonymized to remove any "
            "user-private data. The user can view the system information "
            "before sending, and choose to send the feedback report without "
            "system information and the logs (unchecking 'Send system "
            "information' prevents sending logs as well), the screenshot, or "
            "even his/her email address."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: false
          setting:
            "This feature cannot be disabled by settings and is only activated "
            "by direct user request."
          policy_exception_justification: "Not implemented."
        })");
  // Note: FeedbackUploaderDelegate deletes itself and the fetcher.
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(
          post_url, net::URLFetcher::POST,
          new FeedbackUploaderDelegate(
              data,
              base::Bind(&FeedbackUploaderChrome::UpdateUploadTimer,
                         AsWeakPtr()),
              base::Bind(&FeedbackUploaderChrome::RetryReport, AsWeakPtr())),
          traffic_annotation)
          .release();
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher, data_use_measurement::DataUseUserData::FEEDBACK_UPLOADER);
  // Tell feedback server about the variation state of this install.
  net::HttpRequestHeaders headers;
  // Note: It's OK to pass |is_signed_in| false if it's unknown, as it does
  // not affect transmission of experiments coming from the variations server.
  bool is_signed_in = false;
  variations::AppendVariationHeaders(fetcher->GetOriginalURL(),
                                     context_->IsOffTheRecord(), false,
                                     is_signed_in, &headers);
  fetcher->SetExtraRequestHeaders(headers.ToString());

  fetcher->SetUploadData(kProtoBufMimeType, data);
  fetcher->SetRequestContext(
      content::BrowserContext::GetDefaultStoragePartition(context_)->
          GetURLRequestContext());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES);
  fetcher->Start();
}

}  // namespace feedback
