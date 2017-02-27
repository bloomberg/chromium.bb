// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/base_ping_manager.h"

#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/log/net_log_source_type.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {
// Returns a dictionary with "url"=|url-spec| and "data"=|payload| for
// netlogging the start phase of a ping.
std::unique_ptr<base::Value> NetLogPingStartCallback(
    const net::NetLogWithSource& net_log,
    const GURL& url,
    const std::string& payload,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> event_params(
      new base::DictionaryValue());
  event_params->SetString("url", url.spec());
  event_params->SetString("payload", payload);
  net_log.source().AddToEventParameters(event_params.get());
  return std::move(event_params);
}

// Returns a dictionary with "url"=|url-spec|, "status"=|status| and
// "error"=|error| for netlogging the end phase of a ping.
std::unique_ptr<base::Value> NetLogPingEndCallback(
    const net::NetLogWithSource& net_log,
    const net::URLRequestStatus& status,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> event_params(
      new base::DictionaryValue());
  event_params->SetInteger("status", status.status());
  event_params->SetInteger("error", status.error());
  net_log.source().AddToEventParameters(event_params.get());
  return std::move(event_params);
}

}  // namespace

namespace safe_browsing {

// SafeBrowsingPingManager implementation ----------------------------------

// static
std::unique_ptr<BasePingManager> BasePingManager::Create(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::WrapUnique(new BasePingManager(request_context_getter, config));
}

BasePingManager::BasePingManager(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config)
    : client_name_(config.client_name),
      request_context_getter_(request_context_getter),
      url_prefix_(config.url_prefix) {
  DCHECK(!url_prefix_.empty());

  if (request_context_getter) {
    net_log_ = net::NetLogWithSource::Make(
        request_context_getter->GetURLRequestContext()->net_log(),
        net::NetLogSourceType::SAFE_BROWSING);
  }

  version_ = ProtocolManagerHelper::Version();
}

BasePingManager::~BasePingManager() {}

// net::URLFetcherDelegate implementation ----------------------------------

// All SafeBrowsing request responses are handled here.
void BasePingManager::OnURLFetchComplete(const net::URLFetcher* source) {
  net_log_.EndEvent(
      net::NetLogEventType::SAFE_BROWSING_PING,
      base::Bind(&NetLogPingEndCallback, net_log_, source->GetStatus()));
  auto it =
      std::find_if(safebrowsing_reports_.begin(), safebrowsing_reports_.end(),
                   [source](const std::unique_ptr<net::URLFetcher>& ptr) {
                     return ptr.get() == source;
                   });
  DCHECK(it != safebrowsing_reports_.end());
  safebrowsing_reports_.erase(it);
}

// Sends a SafeBrowsing "hit" report.
void BasePingManager::ReportSafeBrowsingHit(
    const safe_browsing::HitReport& hit_report) {
  GURL report_url = SafeBrowsingHitUrl(hit_report);
  std::unique_ptr<net::URLFetcher> report_ptr = net::URLFetcher::Create(
      report_url,
      hit_report.post_data.empty() ? net::URLFetcher::GET
                                   : net::URLFetcher::POST,
      this);
  net::URLFetcher* report = report_ptr.get();
  data_use_measurement::DataUseUserData::AttachToFetcher(
      report, data_use_measurement::DataUseUserData::SAFE_BROWSING);
  report_ptr->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  report_ptr->SetRequestContext(request_context_getter_.get());
  std::string post_data_base64;
  if (!hit_report.post_data.empty()) {
    report_ptr->SetUploadData("text/plain", hit_report.post_data);
    base::Base64Encode(hit_report.post_data, &post_data_base64);
  }

  net_log_.BeginEvent(
      net::NetLogEventType::SAFE_BROWSING_PING,
      base::Bind(&NetLogPingStartCallback, net_log_,
                 report_ptr->GetOriginalURL(), post_data_base64));

  report->Start();
  safebrowsing_reports_.insert(std::move(report_ptr));
}

// Sends threat details for users who opt-in.
void BasePingManager::ReportThreatDetails(const std::string& report) {
  GURL report_url = ThreatDetailsUrl();
  std::unique_ptr<net::URLFetcher> fetcher =
      net::URLFetcher::Create(report_url, net::URLFetcher::POST, this);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher.get(), data_use_measurement::DataUseUserData::SAFE_BROWSING);
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->SetUploadData("application/octet-stream", report);
  // Don't try too hard to send reports on failures.
  fetcher->SetAutomaticallyRetryOn5xx(false);

  std::string report_base64;
  base::Base64Encode(report, &report_base64);
  net_log_.BeginEvent(net::NetLogEventType::SAFE_BROWSING_PING,
                      base::Bind(&NetLogPingStartCallback, net_log_,
                                 fetcher->GetOriginalURL(), report_base64));

  fetcher->Start();
  safebrowsing_reports_.insert(std::move(fetcher));
}

GURL BasePingManager::SafeBrowsingHitUrl(
    const safe_browsing::HitReport& hit_report) const {
  DCHECK(hit_report.threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         hit_report.threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         hit_report.threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
         hit_report.threat_type == SB_THREAT_TYPE_BINARY_MALWARE_URL ||
         hit_report.threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL ||
         hit_report.threat_type == SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL);
  std::string url = ProtocolManagerHelper::ComposeUrl(
      url_prefix_, "report", client_name_, version_, std::string(),
      hit_report.extended_reporting_level);

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

  // Add user_population component only if it's not empty.
  std::string user_population_comp;
  if (!hit_report.population_id.empty()) {
    // Population_id should be URL-safe, but escape it and size-limit it
    // anyway since it came from outside Chrome.
    std::string up_str =
        net::EscapeQueryParamValue(hit_report.population_id, true);
    if (up_str.size() > 512) {
      DCHECK(false) << "population_id is too long: " << up_str;
      up_str = "UP_STRING_TOO_LONG";
    }

    user_population_comp = "&up=" + up_str;
  }

  return GURL(base::StringPrintf(
      "%s&evts=%s&evtd=%s&evtr=%s&evhr=%s&evtb=%d&src=%s&m=%d%s", url.c_str(),
      threat_list.c_str(),
      net::EscapeQueryParamValue(hit_report.malicious_url.spec(), true).c_str(),
      net::EscapeQueryParamValue(hit_report.page_url.spec(), true).c_str(),
      net::EscapeQueryParamValue(hit_report.referrer_url.spec(), true).c_str(),
      hit_report.is_subresource, threat_source.c_str(),
      hit_report.is_metrics_reporting_active, user_population_comp.c_str()));
}

GURL BasePingManager::ThreatDetailsUrl() const {
  std::string url = base::StringPrintf(
      "%s/clientreport/malware?client=%s&appver=%s&pver=1.0",
      url_prefix_.c_str(), client_name_.c_str(), version_.c_str());
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        net::EscapeQueryParamValue(api_key, true).c_str());
  }
  return GURL(url);
}

}  // namespace safe_browsing
