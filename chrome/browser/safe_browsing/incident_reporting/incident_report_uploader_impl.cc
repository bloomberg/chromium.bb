// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/incident_report_uploader_impl.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"

namespace safe_browsing {

namespace {

const char kSbIncidentReportUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/incident";

constexpr net::NetworkTrafficAnnotationTag
    kSafeBrowsingIncidentTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("safe_browsing_incident", R"(
    semantics {
      sender: "Safe Browsing Incident Reporting"
      description:
        "Following a security incident, Chrome can report system information "
        "and possible causes to Google to improve Safe Browsing experience."
      trigger:
        "An incident on the local machine affecting the user's experience with "
        "Chrome."
      data:
        "A description of the incident, possible causes and related system "
        "information. See ClientIncidentReport in 'https://cs.chromium.org/"
        "chromium/src/components/safe_browsing/csd.proto' for more details."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "Safe Browsing cookie store"
      setting:
        "Users can control this feature via the 'Automatically report details "
        "of possible security incidents to Google' setting under Privacy."
      chrome_policy {
        SafeBrowsingExtendedReportingOptInAllowed {
          policy_options {mode: MANDATORY}
          SafeBrowsingExtendedReportingOptInAllowed: false
        }
      }
    })");

}  // namespace

// This is initialized here rather than in the class definition due to an
// "extension" in MSVC that defies the standard.
// static
const int IncidentReportUploaderImpl::kTestUrlFetcherId = 47;

IncidentReportUploaderImpl::~IncidentReportUploaderImpl() {
}

// static
std::unique_ptr<IncidentReportUploader>
IncidentReportUploaderImpl::UploadReport(
    const OnResultCallback& callback,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const ClientIncidentReport& report) {
  std::string post_data;
  if (!report.SerializeToString(&post_data))
    return std::unique_ptr<IncidentReportUploader>();
  return std::unique_ptr<IncidentReportUploader>(new IncidentReportUploaderImpl(
      callback, request_context_getter, post_data));
}

IncidentReportUploaderImpl::IncidentReportUploaderImpl(
    const OnResultCallback& callback,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const std::string& post_data)
    : IncidentReportUploader(callback),
      url_fetcher_(
          net::URLFetcher::Create(kTestUrlFetcherId,
                                  GetIncidentReportUrl(),
                                  net::URLFetcher::POST,
                                  this,
                                  kSafeBrowsingIncidentTrafficAnnotation)),
      time_begin_(base::TimeTicks::Now()) {
  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher_.get(), data_use_measurement::DataUseUserData::SAFE_BROWSING);
  UMA_HISTOGRAM_COUNTS("SBIRS.ReportPayloadSize", post_data.size());
  url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SetRequestContext(request_context_getter.get());
  url_fetcher_->SetUploadData("application/octet-stream", post_data);
  url_fetcher_->Start();
}

// static
GURL IncidentReportUploaderImpl::GetIncidentReportUrl() {
  GURL url(kSbIncidentReportUrl);
  std::string api_key(google_apis::GetAPIKey());
  if (api_key.empty())
    return url;
  return url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));
}

void IncidentReportUploaderImpl::OnURLFetchComplete(
    const net::URLFetcher* source) {
  // Take ownership of the fetcher in this scope (source == url_fetcher_).
  std::unique_ptr<net::URLFetcher> url_fetcher(std::move(url_fetcher_));

  UMA_HISTOGRAM_TIMES("SBIRS.ReportUploadTime",
                      base::TimeTicks::Now() - time_begin_);

  Result result = UPLOAD_REQUEST_FAILED;
  std::unique_ptr<ClientIncidentResponse> response;

  if (source->GetStatus().is_success() &&
      source->GetResponseCode() == net::HTTP_OK) {
    std::string data;
    source->GetResponseAsString(&data);
    response.reset(new ClientIncidentResponse());
    if (!response->ParseFromString(data)) {
      response.reset();
      result = UPLOAD_INVALID_RESPONSE;
    } else {
      result = UPLOAD_SUCCESS;
    }
  }
  // Callbacks have a tendency to delete the uploader, so no touching anything
  // after this.
  callback_.Run(result, std::move(response));
}

}  // namespace safe_browsing
