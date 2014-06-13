// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_report_uploader_impl.h"

#include "base/metrics/histogram.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"

namespace safe_browsing {

namespace {

const char kSbIncidentReportUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/incident";

}  // namespace

// This is initialized here rather than in the class definition due to an
// "extension" in MSVC that defies the standard.
// static
const int IncidentReportUploaderImpl::kTestUrlFetcherId = 47;

IncidentReportUploaderImpl::~IncidentReportUploaderImpl() {
}

// static
scoped_ptr<IncidentReportUploader> IncidentReportUploaderImpl::UploadReport(
    const OnResultCallback& callback,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const ClientIncidentReport& report) {
  std::string post_data;
  if (!report.SerializeToString(&post_data))
    return scoped_ptr<IncidentReportUploader>();
  return scoped_ptr<IncidentReportUploader>(new IncidentReportUploaderImpl(
      callback, request_context_getter, post_data));
}

IncidentReportUploaderImpl::IncidentReportUploaderImpl(
    const OnResultCallback& callback,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const std::string& post_data)
    : IncidentReportUploader(callback),
      url_fetcher_(net::URLFetcher::Create(kTestUrlFetcherId,
                                           GetIncidentReportUrl(),
                                           net::URLFetcher::POST,
                                           this)),
      time_begin_(base::TimeTicks::Now()) {
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
  scoped_ptr<net::URLFetcher> url_fetcher(url_fetcher_.Pass());

  UMA_HISTOGRAM_TIMES("SBIRS.ReportUploadTime",
                      base::TimeTicks::Now() - time_begin_);

  Result result = UPLOAD_REQUEST_FAILED;
  scoped_ptr<ClientIncidentResponse> response;

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
  callback_.Run(result, response.Pass());
}

}  // namespace safe_browsing
