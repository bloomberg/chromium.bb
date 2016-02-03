// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ping_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/certificate_reporting/error_reporter.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/certificate_report_sender.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {
// URLs to upload invalid certificate chain reports. The HTTP URL is
// preferred since a client seeing an invalid cert might not be able to
// make an HTTPS connection to report it.
const char kExtendedReportingUploadUrlInsecure[] =
    "http://safebrowsing.googleusercontent.com/safebrowsing/clientreport/"
    "chrome-certs";
const char kExtendedReportingUploadUrlSecure[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/chrome-certs";
}  // namespace

namespace safe_browsing {

// SafeBrowsingPingManager implementation ----------------------------------

// static
SafeBrowsingPingManager* SafeBrowsingPingManager::Create(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return new SafeBrowsingPingManager(request_context_getter, config);
}

SafeBrowsingPingManager::SafeBrowsingPingManager(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config)
    : client_name_(config.client_name),
      request_context_getter_(request_context_getter),
      url_prefix_(config.url_prefix) {
  DCHECK(!url_prefix_.empty());

  if (request_context_getter) {
    // Set the upload URL and whether or not to send cookies with
    // certificate reports sent to Safe Browsing servers.
    bool use_insecure_certificate_upload_url =
        certificate_reporting::ErrorReporter::IsHttpUploadUrlSupported();

    net::CertificateReportSender::CookiesPreference cookies_preference;
    GURL certificate_upload_url;
    if (use_insecure_certificate_upload_url) {
      cookies_preference = net::CertificateReportSender::DO_NOT_SEND_COOKIES;
      certificate_upload_url = GURL(kExtendedReportingUploadUrlInsecure);
    } else {
      cookies_preference = net::CertificateReportSender::SEND_COOKIES;
      certificate_upload_url = GURL(kExtendedReportingUploadUrlSecure);
    }

    certificate_error_reporter_.reset(new certificate_reporting::ErrorReporter(
        request_context_getter->GetURLRequestContext(), certificate_upload_url,
        cookies_preference));
  }

  version_ = SafeBrowsingProtocolManagerHelper::Version();
}

SafeBrowsingPingManager::~SafeBrowsingPingManager() {
  // Delete in-progress safebrowsing reports (hits and details).
  STLDeleteContainerPointers(safebrowsing_reports_.begin(),
                             safebrowsing_reports_.end());
}

// net::URLFetcherDelegate implementation ----------------------------------

// All SafeBrowsing request responses are handled here.
void SafeBrowsingPingManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  Reports::iterator sit = safebrowsing_reports_.find(source);
  DCHECK(sit != safebrowsing_reports_.end());
  delete *sit;
  safebrowsing_reports_.erase(sit);
}

// Sends a SafeBrowsing "hit" report.
void SafeBrowsingPingManager::ReportSafeBrowsingHit(
    const safe_browsing::HitReport& hit_report) {
  GURL report_url = SafeBrowsingHitUrl(hit_report);
  net::URLFetcher* report =
      net::URLFetcher::Create(report_url, hit_report.post_data.empty()
                                              ? net::URLFetcher::GET
                                              : net::URLFetcher::POST,
                              this)
          .release();
  report->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  report->SetRequestContext(request_context_getter_.get());
  if (!hit_report.post_data.empty())
    report->SetUploadData("text/plain", hit_report.post_data);
  safebrowsing_reports_.insert(report);
  report->Start();
}

// Sends threat details for users who opt-in.
void SafeBrowsingPingManager::ReportThreatDetails(const std::string& report) {
  GURL report_url = ThreatDetailsUrl();
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(report_url, net::URLFetcher::POST, this)
          .release();
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->SetUploadData("application/octet-stream", report);
  // Don't try too hard to send reports on failures.
  fetcher->SetAutomaticallyRetryOn5xx(false);
  fetcher->Start();
  safebrowsing_reports_.insert(fetcher);
}

void SafeBrowsingPingManager::ReportInvalidCertificateChain(
    const std::string& serialized_report) {
  DCHECK(certificate_error_reporter_);
  certificate_error_reporter_->SendExtendedReportingReport(serialized_report);
}

void SafeBrowsingPingManager::SetCertificateErrorReporterForTesting(
    scoped_ptr<certificate_reporting::ErrorReporter>
        certificate_error_reporter) {
  certificate_error_reporter_ = std::move(certificate_error_reporter);
}

GURL SafeBrowsingPingManager::SafeBrowsingHitUrl(
    const safe_browsing::HitReport& hit_report) const {
  DCHECK(hit_report.threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         hit_report.threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         hit_report.threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
         hit_report.threat_type == SB_THREAT_TYPE_BINARY_MALWARE_URL ||
         hit_report.threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL ||
         hit_report.threat_type == SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL);
  std::string url = SafeBrowsingProtocolManagerHelper::ComposeUrl(
      url_prefix_, "report", client_name_, version_, std::string(),
      hit_report.is_extended_reporting);

  std::string threat_list = "none";
  switch (hit_report.threat_type) {
    case SB_THREAT_TYPE_URL_MALWARE:
      threat_list = "malblhit";
      break;
    case SB_THREAT_TYPE_URL_PHISHING:
      threat_list = "phishblhit";
      break;
    case SB_THREAT_TYPE_URL_UNWANTED:
      threat_list = "uwsblhit";
      break;
    case SB_THREAT_TYPE_BINARY_MALWARE_URL:
      threat_list = "binurlhit";
      break;
    case SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL:
      threat_list = "phishcsdhit";
      break;
    case SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL:
      threat_list = "malcsdhit";
      break;
    default:
      NOTREACHED();
  }

  std::string threat_source = "none";
  switch (hit_report.threat_source) {
    case safe_browsing::ThreatSource::DATA_SAVER:
      threat_source = "ds";
      break;
    case safe_browsing::ThreatSource::REMOTE:
      threat_source = "rem";
      break;
    case safe_browsing::ThreatSource::LOCAL_PVER3:
      threat_source = "l3";
      break;
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      threat_source = "l4";
      break;
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
      threat_source = "csd";
      break;
    case safe_browsing::ThreatSource::UNKNOWN:
      NOTREACHED();
  }

  return GURL(base::StringPrintf(
      "%s&evts=%s&evtd=%s&evtr=%s&evhr=%s&evtb=%d&src=%s&m=%d", url.c_str(),
      threat_list.c_str(),
      net::EscapeQueryParamValue(hit_report.malicious_url.spec(), true).c_str(),
      net::EscapeQueryParamValue(hit_report.page_url.spec(), true).c_str(),
      net::EscapeQueryParamValue(hit_report.referrer_url.spec(), true).c_str(),
      hit_report.is_subresource, threat_source.c_str(),
      hit_report.is_metrics_reporting_active));
}

GURL SafeBrowsingPingManager::ThreatDetailsUrl() const {
  std::string url = base::StringPrintf(
          "%s/clientreport/malware?client=%s&appver=%s&pver=1.0",
          url_prefix_.c_str(),
          client_name_.c_str(),
          version_.c_str());
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        net::EscapeQueryParamValue(api_key, true).c_str());
  }
  return GURL(url);
}

}  // namespace safe_browsing
