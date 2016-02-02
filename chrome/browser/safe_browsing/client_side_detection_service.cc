// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/client_model.pb.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "crypto/sha2.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

  // malware report type for UMA histogram counting.
  enum MalwareReportTypes {
    REPORT_SENT,
    REPORT_HIT_LIMIT,
    REPORT_FAILED_SERIALIZATION,

    // Always at the end
    REPORT_RESULT_MAX
  };

  void UpdateEnumUMAHistogram(MalwareReportTypes report_type) {
    DCHECK(report_type >= 0 && report_type < REPORT_RESULT_MAX);
    UMA_HISTOGRAM_ENUMERATION("SBClientMalware.SentReports",
                              report_type, REPORT_RESULT_MAX);
  }

}  // namespace

const int ClientSideDetectionService::kInitialClientModelFetchDelayMs = 10000;
const int ClientSideDetectionService::kReportsIntervalDays = 1;
const int ClientSideDetectionService::kMaxReportsPerInterval = 3;
const int ClientSideDetectionService::kNegativeCacheIntervalDays = 1;
const int ClientSideDetectionService::kPositiveCacheIntervalMinutes = 30;

const char ClientSideDetectionService::kClientReportPhishingUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/phishing";
const char ClientSideDetectionService::kClientReportMalwareUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/malware-check";

struct ClientSideDetectionService::ClientReportInfo {
  ClientReportPhishingRequestCallback callback;
  GURL phishing_url;
};

struct ClientSideDetectionService::ClientMalwareReportInfo {
  ClientReportMalwareRequestCallback callback;
  // This is the original landing url, may not be the malware url.
  GURL original_url;
};

ClientSideDetectionService::CacheState::CacheState(bool phish, base::Time time)
    : is_phishing(phish),
      timestamp(time) {}

ClientSideDetectionService::ClientSideDetectionService(
    net::URLRequestContextGetter* request_context_getter)
    : enabled_(false),
      request_context_getter_(request_context_getter),
      weak_factory_(this) {
  base::Closure update_renderers =
      base::Bind(&ClientSideDetectionService::SendModelToRenderers,
                 base::Unretained(this));
  model_loader_standard_.reset(
      new ModelLoader(update_renderers, request_context_getter, false));
  model_loader_extended_.reset(
      new ModelLoader(update_renderers, request_context_getter, true));

  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ClientSideDetectionService::~ClientSideDetectionService() {
  weak_factory_.InvalidateWeakPtrs();
  STLDeleteContainerPairPointers(client_phishing_reports_.begin(),
                                 client_phishing_reports_.end());
  client_phishing_reports_.clear();
  STLDeleteContainerPairPointers(client_malware_reports_.begin(),
                                 client_malware_reports_.end());
  client_malware_reports_.clear();
}

// static
ClientSideDetectionService* ClientSideDetectionService::Create(
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return new ClientSideDetectionService(request_context_getter);
}

void ClientSideDetectionService::SetEnabledAndRefreshState(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendModelToRenderers();  // always refresh the renderer state
  if (enabled == enabled_)
    return;
  enabled_ = enabled;
  if (enabled_) {
    // Refresh the models when the service is enabled.  This can happen when
    // either of the preferences are toggled, or early during startup if
    // safe browsing is already enabled. In a lot of cases the model will be
    // in the cache so it  won't actually be fetched from the network.
    // We delay the first model fetches to avoid slowing down browser startup.
    model_loader_standard_->ScheduleFetch(kInitialClientModelFetchDelayMs);
    model_loader_extended_->ScheduleFetch(kInitialClientModelFetchDelayMs);
  } else {
    // Cancel model loads in progress.
    model_loader_standard_->CancelFetcher();
    model_loader_extended_->CancelFetcher();
    // Invoke pending callbacks with a false verdict.
    for (std::map<const net::URLFetcher*, ClientReportInfo*>::iterator it =
             client_phishing_reports_.begin();
         it != client_phishing_reports_.end(); ++it) {
      ClientReportInfo* info = it->second;
      if (!info->callback.is_null())
        info->callback.Run(info->phishing_url, false);
    }
    STLDeleteContainerPairPointers(client_phishing_reports_.begin(),
                                   client_phishing_reports_.end());
    client_phishing_reports_.clear();
    for (std::map<const net::URLFetcher*, ClientMalwareReportInfo*>::iterator it
             = client_malware_reports_.begin();
         it != client_malware_reports_.end(); ++it) {
      ClientMalwareReportInfo* info = it->second;
      if (!info->callback.is_null())
        info->callback.Run(info->original_url, info->original_url, false);
    }
    STLDeleteContainerPairPointers(client_malware_reports_.begin(),
                                   client_malware_reports_.end());
    client_malware_reports_.clear();
    cache_.clear();
  }
}

void ClientSideDetectionService::SendClientReportPhishingRequest(
    ClientPhishingRequest* verdict,
    bool is_extended_reporting,
    const ClientReportPhishingRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClientSideDetectionService::StartClientReportPhishingRequest,
                 weak_factory_.GetWeakPtr(), verdict, is_extended_reporting,
                 callback));
}

void ClientSideDetectionService::SendClientReportMalwareRequest(
    ClientMalwareRequest* verdict,
    const ClientReportMalwareRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClientSideDetectionService::StartClientReportMalwareRequest,
                 weak_factory_.GetWeakPtr(), verdict, callback));
}

bool ClientSideDetectionService::IsPrivateIPAddress(
    const std::string& ip_address) const {
  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(ip_address, &ip_number)) {
    DVLOG(2) << "Unable to parse IP address: '" << ip_address << "'";
    // Err on the side of safety and assume this might be private.
    return true;
  }

  return net::IsIPAddressReserved(ip_number);
}

void ClientSideDetectionService::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::string data;
  source->GetResponseAsString(&data);

  if (client_phishing_reports_.find(source) != client_phishing_reports_.end()) {
    HandlePhishingVerdict(
        source, source->GetURL(), source->GetStatus(),
        source->GetResponseCode(), source->GetCookies(), data);
  } else if (client_malware_reports_.find(source) !=
             client_malware_reports_.end()) {
    HandleMalwareVerdict(
        source, source->GetURL(), source->GetStatus(),
        source->GetResponseCode(), source->GetCookies(), data);
  } else {
    NOTREACHED();
  }
}

void ClientSideDetectionService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  SendModelToProcess(
      content::Source<content::RenderProcessHost>(source).ptr());
}

void ClientSideDetectionService::SendModelToProcess(
    content::RenderProcessHost* process) {
  // The ClientSideDetectionService is enabled if _any_ active profile has
  // SafeBrowsing turned on.  Here we check the profile for each renderer
  // process and only send the model to those that have SafeBrowsing enabled,
  // and we select the model based on the extended reporting setting.
  Profile* profile = Profile::FromBrowserContext(process->GetBrowserContext());
  std::string model;
  if (profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    if (profile->GetPrefs()->GetBoolean(
            prefs::kSafeBrowsingExtendedReportingEnabled)) {
      DVLOG(2) << "Sending phishing model " << model_loader_extended_->name()
               << " to RenderProcessHost @" << process;
      model = model_loader_extended_->model_str();
    } else {
      DVLOG(2) << "Sending phishing model " << model_loader_standard_->name()
               << " to RenderProcessHost @" << process;
      model = model_loader_standard_->model_str();
    }
  } else {
    DVLOG(2) << "Disabling client-side phishing detection for "
             << "RenderProcessHost @" << process;
  }
  process->Send(new SafeBrowsingMsg_SetPhishingModel(model));
}

void ClientSideDetectionService::SendModelToRenderers() {
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    SendModelToProcess(i.GetCurrentValue());
  }
}

void ClientSideDetectionService::StartClientReportPhishingRequest(
    ClientPhishingRequest* verdict,
    bool is_extended_reporting,
    const ClientReportPhishingRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_ptr<ClientPhishingRequest> request(verdict);

  if (!enabled_) {
    if (!callback.is_null())
      callback.Run(GURL(request->url()), false);
    return;
  }

  // Fill in metadata about which model we used.
  if (is_extended_reporting) {
    request->set_model_filename(model_loader_extended_->name());
    request->mutable_population()->set_user_population(
        ChromeUserPopulation::EXTENDED_REPORTING);
  } else {
    request->set_model_filename(model_loader_standard_->name());
    request->mutable_population()->set_user_population(
        ChromeUserPopulation::SAFE_BROWSING);
  }
  DVLOG(2) << "Starting report for hit on model " << request->model_filename();

  std::string request_data;
  if (!request->SerializeToString(&request_data)) {
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.RequestNotSerialized", 1);
    DVLOG(1) << "Unable to serialize the CSD request. Proto file changed?";
    if (!callback.is_null())
      callback.Run(GURL(request->url()), false);
    return;
  }

  net::URLFetcher* fetcher =
      net::URLFetcher::Create(0 /* ID used for testing */,
                              GetClientReportUrl(kClientReportPhishingUrl),
                              net::URLFetcher::POST, this).release();

  // Remember which callback and URL correspond to the current fetcher object.
  ClientReportInfo* info = new ClientReportInfo;
  info->callback = callback;
  info->phishing_url = GURL(request->url());
  client_phishing_reports_[fetcher] = info;

  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->SetUploadData("application/octet-stream", request_data);
  fetcher->Start();

  // Record that we made a request
  phishing_report_times_.push(base::Time::Now());
}

void ClientSideDetectionService::StartClientReportMalwareRequest(
    ClientMalwareRequest* verdict,
    const ClientReportMalwareRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_ptr<ClientMalwareRequest> request(verdict);

  if (!enabled_) {
    if (!callback.is_null())
      callback.Run(GURL(request->url()), GURL(request->url()), false);
    return;
  }

  std::string request_data;
  if (!request->SerializeToString(&request_data)) {
    UpdateEnumUMAHistogram(REPORT_FAILED_SERIALIZATION);
    DVLOG(1) << "Unable to serialize the CSD request. Proto file changed?";
    if (!callback.is_null())
      callback.Run(GURL(request->url()), GURL(request->url()), false);
    return;
  }

  net::URLFetcher* fetcher =
      net::URLFetcher::Create(0 /* ID used for testing */,
                              GetClientReportUrl(kClientReportMalwareUrl),
                              net::URLFetcher::POST, this).release();

  // Remember which callback and URL correspond to the current fetcher object.
  ClientMalwareReportInfo* info = new ClientMalwareReportInfo;
  info->callback = callback;
  info->original_url = GURL(request->url());
  client_malware_reports_[fetcher] = info;

  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->SetUploadData("application/octet-stream", request_data);
  fetcher->Start();

  UMA_HISTOGRAM_ENUMERATION("SBClientMalware.SentReports",
                            REPORT_SENT, REPORT_RESULT_MAX);

  UMA_HISTOGRAM_COUNTS("SBClientMalware.IPBlacklistRequestPayloadSize",
                       request_data.size());

  // Record that we made a malware request
  malware_report_times_.push(base::Time::Now());
}


void ClientSideDetectionService::HandlePhishingVerdict(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  ClientPhishingResponse response;
  scoped_ptr<ClientReportInfo> info(client_phishing_reports_[source]);
  bool is_phishing = false;
  if (status.is_success() && net::HTTP_OK == response_code &&
      response.ParseFromString(data)) {
    // Cache response, possibly flushing an old one.
    cache_[info->phishing_url] =
        make_linked_ptr(new CacheState(response.phishy(), base::Time::Now()));
    is_phishing = response.phishy();
  } else {
    DLOG(ERROR) << "Unable to get the server verdict for URL: "
                << info->phishing_url << " status: " << status.status() << " "
                << "response_code:" << response_code;
  }
  if (!info->callback.is_null())
    info->callback.Run(info->phishing_url, is_phishing);
  client_phishing_reports_.erase(source);
  delete source;
}

void ClientSideDetectionService::HandleMalwareVerdict(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  if (status.is_success()) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "SBClientMalware.IPBlacklistRequestResponseCode", response_code);
  }
  // status error is negative, so we put - in front of it.
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "SBClientMalware.IPBlacklistRequestNetError", -status.error());

  ClientMalwareResponse response;
  scoped_ptr<ClientMalwareReportInfo> info(client_malware_reports_[source]);
  bool should_blacklist = false;
  if (status.is_success() && net::HTTP_OK == response_code &&
      response.ParseFromString(data)) {
    should_blacklist = response.blacklist();
  } else {
    DLOG(ERROR) << "Unable to get the server verdict for URL: "
                << info->original_url << " status: " << status.status() << " "
                << "response_code:" << response_code;
  }

  if (!info->callback.is_null()) {
    if (response.has_bad_url())
      info->callback.Run(info->original_url, GURL(response.bad_url()),
                         should_blacklist);
    else
      info->callback.Run(info->original_url, info->original_url, false);
  }

  client_malware_reports_.erase(source);
  delete source;
}

bool ClientSideDetectionService::IsInCache(const GURL& url) {
  UpdateCache();

  return cache_.find(url) != cache_.end();
}

bool ClientSideDetectionService::GetValidCachedResult(const GURL& url,
                                                      bool* is_phishing) {
  UpdateCache();

  PhishingCache::iterator it = cache_.find(url);
  if (it == cache_.end()) {
    return false;
  }

  // We still need to check if the result is valid.
  const CacheState& cache_state = *it->second;
  if (cache_state.is_phishing ?
      cache_state.timestamp > base::Time::Now() -
          base::TimeDelta::FromMinutes(kPositiveCacheIntervalMinutes) :
      cache_state.timestamp > base::Time::Now() -
          base::TimeDelta::FromDays(kNegativeCacheIntervalDays)) {
    *is_phishing = cache_state.is_phishing;
    return true;
  }
  return false;
}

void ClientSideDetectionService::UpdateCache() {
  // Since we limit the number of requests but allow pass-through for cache
  // refreshes, we don't want to remove elements from the cache if they
  // could be used for this purpose even if we will not use the entry to
  // satisfy the request from the cache.
  base::TimeDelta positive_cache_interval =
      std::max(base::TimeDelta::FromMinutes(kPositiveCacheIntervalMinutes),
               base::TimeDelta::FromDays(kReportsIntervalDays));
  base::TimeDelta negative_cache_interval =
      std::max(base::TimeDelta::FromDays(kNegativeCacheIntervalDays),
               base::TimeDelta::FromDays(kReportsIntervalDays));

  // Remove elements from the cache that will no longer be used.
  for (PhishingCache::iterator it = cache_.begin(); it != cache_.end();) {
    const CacheState& cache_state = *it->second;
    if (cache_state.is_phishing ?
        cache_state.timestamp > base::Time::Now() - positive_cache_interval :
        cache_state.timestamp > base::Time::Now() - negative_cache_interval) {
      ++it;
    } else {
      cache_.erase(it++);
    }
  }
}

bool ClientSideDetectionService::OverMalwareReportLimit() {
  return GetMalwareNumReports() > kMaxReportsPerInterval;
}

bool ClientSideDetectionService::OverPhishingReportLimit() {
  return GetPhishingNumReports() > kMaxReportsPerInterval;
}

int ClientSideDetectionService::GetMalwareNumReports() {
  return GetNumReports(&malware_report_times_);
}

int ClientSideDetectionService::GetPhishingNumReports() {
  return GetNumReports(&phishing_report_times_);
}

int ClientSideDetectionService::GetNumReports(
    std::queue<base::Time>* report_times) {
  base::Time cutoff =
      base::Time::Now() - base::TimeDelta::FromDays(kReportsIntervalDays);

  // Erase items older than cutoff because we will never care about them again.
  while (!report_times->empty() &&
         report_times->front() < cutoff) {
    report_times->pop();
  }

  // Return the number of elements that are above the cutoff.
  return report_times->size();
}

// static
GURL ClientSideDetectionService::GetClientReportUrl(
    const std::string& report_url) {
  GURL url(report_url);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty())
    url = url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));

  return url;
}

}  // namespace safe_browsing
