// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/net_metrics_log_uploader.h"

#include "base/base64.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/metrics/metrics_log_uploader.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "third_party/metrics_proto/reporting_info.pb.h"
#include "url/gurl.h"

namespace {

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    const metrics::MetricsLogUploader::MetricServiceType& service_type) {
  // The code in this function should remain so that we won't need a default
  // case that does not have meaningful annotation.
  if (service_type == metrics::MetricsLogUploader::UMA) {
    return net::DefineNetworkTrafficAnnotation("metrics_report_uma", R"(
        semantics {
          sender: "Metrics UMA Log Uploader"
          description:
            "Report of usage statistics and crash-related data about Chromium. "
            "Usage statistics contain information such as preferences, button "
            "clicks, and memory usage and do not include web page URLs or "
            "personal information. See more at "
            "https://www.google.com/chrome/browser/privacy/ under 'Usage "
            "statistics and crash reports'. Usage statistics are tied to a "
            "pseudonymous machine identifier and not to your email address."
          trigger:
            "Reports are automatically generated on startup and at intervals "
            "while Chromium is running."
          data:
            "A protocol buffer with usage statistics and crash related data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by disabling "
            "'Automatically send usage statistics and crash reports to Google' "
            "in Chromium's settings under Advanced Settings, Privacy. The "
            "feature is enabled by default."
          chrome_policy {
            MetricsReportingEnabled {
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
        })");
  } else {
    DCHECK_EQ(service_type, metrics::MetricsLogUploader::UKM);
    return net::DefineNetworkTrafficAnnotation("metrics_report_ukm", R"(
        semantics {
          sender: "Metrics UKM Log Uploader"
          description:
            "Report of usage statistics that are keyed by URLs to Chromium, "
            "sent only if the profile has History Sync. This includes "
            "information about the web pages you visit and your usage of them, "
            "such as page load speed. This will also include URLs and "
            "statistics related to downloaded files. If Extension Sync is "
            "enabled, these statistics will also include information about "
            "the extensions that have been installed from Chrome Web Store. "
            "Google only stores usage statistics associated with published "
            "extensions, and URLs that are known by Google’s search index. "
            "Usage statistics are tied to a pseudonymous machine identifier "
            "and not to your email address."
          trigger:
            "Reports are automatically generated on startup and at intervals "
            "while Chromium is running with Sync enabled."
          data:
            "A protocol buffer with usage statistics and associated URLs."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by disabling "
            "'Automatically send usage statistics and crash reports to Google' "
            "in Chromium's settings under Advanced Settings, Privacy. This is "
            "only enabled if all active profiles have History/Extension Sync "
            "enabled without a Sync passphrase."
          chrome_policy {
            MetricsReportingEnabled {
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
        })");
  }
}

std::string SerializeReportingInfo(
    const metrics::ReportingInfo& reporting_info) {
  std::string result;
  std::string bytes;
  bool success = reporting_info.SerializeToString(&bytes);
  DCHECK(success);
  base::Base64Encode(bytes, &result);
  return result;
}

}  // namespace

namespace metrics {

NetMetricsLogUploader::NetMetricsLogUploader(
    net::URLRequestContextGetter* request_context_getter,
    base::StringPiece server_url,
    base::StringPiece mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : request_context_getter_(request_context_getter),
      server_url_(server_url),
      mime_type_(mime_type.data(), mime_type.size()),
      service_type_(service_type),
      on_upload_complete_(on_upload_complete) {}

NetMetricsLogUploader::~NetMetricsLogUploader() {
}

void NetMetricsLogUploader::UploadLog(const std::string& compressed_log_data,
                                      const std::string& log_hash,
                                      const ReportingInfo& reporting_info) {
  UploadLogToURL(compressed_log_data, log_hash, reporting_info,
                 GURL(server_url_));
}

void NetMetricsLogUploader::UploadLogToURL(
    const std::string& compressed_log_data,
    const std::string& log_hash,
    const ReportingInfo& reporting_info,
    const GURL& url) {
  current_fetch_ =
      net::URLFetcher::Create(url, net::URLFetcher::POST, this,
                              GetNetworkTrafficAnnotation(service_type_));

  auto service = data_use_measurement::DataUseUserData::UMA;

  switch (service_type_) {
    case MetricsLogUploader::UMA:
      service = data_use_measurement::DataUseUserData::UMA;
      break;
    case MetricsLogUploader::UKM:
      service = data_use_measurement::DataUseUserData::UKM;
      break;
  }
  data_use_measurement::DataUseUserData::AttachToFetcher(current_fetch_.get(),
                                                         service);
  current_fetch_->SetRequestContext(request_context_getter_);
  current_fetch_->SetUploadData(mime_type_, compressed_log_data);

  // Tell the server that we're uploading gzipped protobufs.
  current_fetch_->SetExtraRequestHeaders("content-encoding: gzip");

  DCHECK(!log_hash.empty());
  current_fetch_->AddExtraRequestHeader("X-Chrome-UMA-Log-SHA1: " + log_hash);

  std::string reporting_info_string = SerializeReportingInfo(reporting_info);
  current_fetch_->AddExtraRequestHeader("X-Chrome-UMA-ReportingInfo:" +
                                        reporting_info_string);

  // Drop cookies and auth data.
  current_fetch_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                               net::LOAD_DO_NOT_SEND_AUTH_DATA |
                               net::LOAD_DO_NOT_SEND_COOKIES);
  current_fetch_->Start();
}

void NetMetricsLogUploader::OnURLFetchComplete(const net::URLFetcher* source) {
  // We're not allowed to re-use the existing |URLFetcher|s, so free them here.
  // Note however that |source| is aliased to the fetcher, so we should be
  // careful not to delete it too early.
  DCHECK_EQ(current_fetch_.get(), source);

  int response_code = source->GetResponseCode();
  if (response_code == net::URLFetcher::RESPONSE_CODE_INVALID)
    response_code = -1;
  int error_code = 0;
  const net::URLRequestStatus& request_status = source->GetStatus();
  if (request_status.status() != net::URLRequestStatus::SUCCESS) {
    error_code = request_status.error();
  }
  current_fetch_.reset();
  on_upload_complete_.Run(response_code, error_code);
}

}  // namespace metrics
