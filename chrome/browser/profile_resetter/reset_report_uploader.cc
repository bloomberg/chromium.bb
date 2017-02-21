// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "chrome/browser/profile_resetter/profile_reset_report.pb.h"
#include "chrome/browser/profile_resetter/reset_report_uploader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
const char kResetReportUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/chrome-reset";

GURL GetClientReportUrl(const std::string& report_url) {
  GURL url(report_url);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty())
    url = url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));

  return url;
}

}  // namespace

ResetReportUploader::ResetReportUploader(content::BrowserContext* context)
    : url_request_context_getter_(
          content::BrowserContext::GetDefaultStoragePartition(context)->
              GetURLRequestContext()) {}

ResetReportUploader::~ResetReportUploader() {}

void ResetReportUploader::DispatchReport(
    const reset_report::ChromeResetReport& report) {
  std::string request_data;
  CHECK(report.SerializeToString(&request_data));

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("profile_resetter_upload", R"(
        semantics {
          sender: "Profile Resetter"
          description:
            "When users choose to reset their profile, they are offered the "
            "choice to report to Google the settings and their values that are "
            "affected by the reset. The user can inspect the values before "
            "they are sent to Google and needs to consent to sending them."
          trigger:
            "Users reset their profile in Chrome settings and consent to "
            "sending a report."
          data:
            "Startup URLs, homepage URL, default search engine, installed "
            "extensions, Chrome shortcut on the desktop and the Windows start "
            "menu, some settings. See "
            "chrome/browser/profile_resetter/profile_reset_report.proto "
            "for details."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: false
          setting: "None, the user needs to actively send the data."
          policy_exception_justification:
            "None, considered not useful because the user needs to actively "
            "send the data."
        })");

  // Note fetcher will be deleted by OnURLFetchComplete.
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(GetClientReportUrl(kResetReportUrl),
                              net::URLFetcher::POST, this, traffic_annotation)
          .release();
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(url_request_context_getter_.get());
  fetcher->SetUploadData("application/octet-stream", request_data);
  fetcher->Start();
}

void ResetReportUploader::OnURLFetchComplete(const net::URLFetcher* source) {
  delete source;
}
