// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ping_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/env_vars.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

// SafeBrowsingPingManager implementation ----------------------------------

// static
SafeBrowsingPingManager* SafeBrowsingPingManager::Create(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return new SafeBrowsingPingManager(request_context_getter, config);
}

SafeBrowsingPingManager::SafeBrowsingPingManager(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config)
    : client_name_(config.client_name),
      request_context_getter_(request_context_getter),
      url_prefix_(config.url_prefix) {
  DCHECK(!url_prefix_.empty());

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

// Sends a SafeBrowsing "hit" for UMA users.
void SafeBrowsingPingManager::ReportSafeBrowsingHit(
    const GURL& malicious_url,
    const GURL& page_url,
    const GURL& referrer_url,
    bool is_subresource,
    SBThreatType threat_type,
    const std::string& post_data) {
  GURL report_url = SafeBrowsingHitUrl(malicious_url, page_url,
                                       referrer_url, is_subresource,
                                       threat_type);
  net::URLFetcher* report = net::URLFetcher::Create(
      report_url,
      post_data.empty() ? net::URLFetcher::GET : net::URLFetcher::POST,
      this);
  report->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  report->SetRequestContext(request_context_getter_.get());
  if (!post_data.empty())
    report->SetUploadData("text/plain", post_data);
  safebrowsing_reports_.insert(report);
  report->Start();
}

// Sends malware details for users who opt-in.
void SafeBrowsingPingManager::ReportMalwareDetails(
    const std::string& report) {
  GURL report_url = MalwareDetailsUrl();
  net::URLFetcher* fetcher = net::URLFetcher::Create(
      report_url, net::URLFetcher::POST, this);
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->SetUploadData("application/octet-stream", report);
  // Don't try too hard to send reports on failures.
  fetcher->SetAutomaticallyRetryOn5xx(false);
  fetcher->Start();
  safebrowsing_reports_.insert(fetcher);
}

GURL SafeBrowsingPingManager::SafeBrowsingHitUrl(
    const GURL& malicious_url, const GURL& page_url,
    const GURL& referrer_url, bool is_subresource,
    SBThreatType threat_type) const {
  DCHECK(threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         threat_type == SB_THREAT_TYPE_BINARY_MALWARE_URL ||
         threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL ||
         threat_type == SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL);
  std::string url = SafeBrowsingProtocolManagerHelper::ComposeUrl(
      url_prefix_, "report", client_name_, version_, std::string());
  std::string threat_list = "none";
  switch (threat_type) {
    case SB_THREAT_TYPE_URL_MALWARE:
      threat_list = "malblhit";
      break;
    case SB_THREAT_TYPE_URL_PHISHING:
      threat_list = "phishblhit";
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
  return GURL(base::StringPrintf("%s&evts=%s&evtd=%s&evtr=%s&evhr=%s&evtb=%d",
      url.c_str(), threat_list.c_str(),
      net::EscapeQueryParamValue(malicious_url.spec(), true).c_str(),
      net::EscapeQueryParamValue(page_url.spec(), true).c_str(),
      net::EscapeQueryParamValue(referrer_url.spec(), true).c_str(),
      is_subresource));
}

GURL SafeBrowsingPingManager::MalwareDetailsUrl() const {
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
