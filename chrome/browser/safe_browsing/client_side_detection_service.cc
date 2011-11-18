// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/client_model.pb.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_fetcher.h"
#include "crypto/sha2.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

namespace safe_browsing {

const size_t ClientSideDetectionService::kMaxModelSizeBytes = 90 * 1024;
const int ClientSideDetectionService::kMaxReportsPerInterval = 3;
// TODO(noelutz): once we know this mechanism works as intended we should fetch
// the model much more frequently.  E.g., every 5 minutes or so.
const int ClientSideDetectionService::kClientModelFetchIntervalMs = 3600 * 1000;
const int ClientSideDetectionService::kInitialClientModelFetchDelayMs = 10000;

const base::TimeDelta ClientSideDetectionService::kReportsInterval =
    base::TimeDelta::FromDays(1);
const base::TimeDelta ClientSideDetectionService::kNegativeCacheInterval =
    base::TimeDelta::FromDays(1);
const base::TimeDelta ClientSideDetectionService::kPositiveCacheInterval =
    base::TimeDelta::FromMinutes(30);

const char ClientSideDetectionService::kClientReportPhishingUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/phishing";
const char ClientSideDetectionService::kClientModelUrl[] =
    "https://ssl.gstatic.com/safebrowsing/csd/client_model_v4.pb";

struct ClientSideDetectionService::ClientReportInfo {
  scoped_ptr<ClientReportPhishingRequestCallback> callback;
  GURL phishing_url;
};

ClientSideDetectionService::CacheState::CacheState(bool phish, base::Time time)
    : is_phishing(phish),
      timestamp(time) {}

ClientSideDetectionService::ClientSideDetectionService(
    net::URLRequestContextGetter* request_context_getter)
    : enabled_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      request_context_getter_(request_context_getter) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ClientSideDetectionService::~ClientSideDetectionService() {
  weak_factory_.InvalidateWeakPtrs();
  STLDeleteContainerPairPointers(client_phishing_reports_.begin(),
                                 client_phishing_reports_.end());
  client_phishing_reports_.clear();
}

// static
ClientSideDetectionService* ClientSideDetectionService::Create(
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ClientSideDetectionService> service(
      new ClientSideDetectionService(request_context_getter));
  if (!service->InitializePrivateNetworks()) {
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.InitPrivateNetworksFailed", 1);
    return NULL;
  }
  return service.release();
}

void ClientSideDetectionService::SetEnabledAndRefreshState(bool enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendModelToRenderers();  // always refresh the renderer state
  if (enabled == enabled_)
    return;
  enabled_ = enabled;
  if (enabled_) {
    // Refresh the model when the service is enabled.  This can happen when the
    // preference is toggled, or early during startup if the preference is
    // already enabled. In a lot of cases the model will be in the cache so it
    // won't actually be fetched from the network.
    // We delay the first model fetch to avoid slowing down browser startup.
    ScheduleFetchModel(kInitialClientModelFetchDelayMs);
  } else {
    // Cancel pending requests.
    model_fetcher_.reset();
    // Invoke pending callbacks with a false verdict.
    for (std::map<const content::URLFetcher*, ClientReportInfo*>::iterator it =
             client_phishing_reports_.begin();
         it != client_phishing_reports_.end(); ++it) {
      ClientReportInfo* info = it->second;
      if (info->callback.get())
        info->callback->Run(info->phishing_url, false);
    }
    STLDeleteContainerPairPointers(client_phishing_reports_.begin(),
                                   client_phishing_reports_.end());
    client_phishing_reports_.clear();
    cache_.clear();
  }
}

void ClientSideDetectionService::SendClientReportPhishingRequest(
    ClientPhishingRequest* verdict,
    ClientReportPhishingRequestCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ClientSideDetectionService::StartClientReportPhishingRequest,
                 weak_factory_.GetWeakPtr(), verdict, callback));
}

bool ClientSideDetectionService::IsPrivateIPAddress(
    const std::string& ip_address) const {
  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(ip_address, &ip_number)) {
    VLOG(2) << "Unable to parse IP address: '" << ip_address << "'";
    // Err on the side of safety and assume this might be private.
    return true;
  }

  for (std::vector<AddressRange>::const_iterator it =
           private_networks_.begin();
       it != private_networks_.end(); ++it) {
    if (net::IPNumberMatchesPrefix(ip_number, it->first, it->second)) {
      return true;
    }
  }
  return false;
}

bool ClientSideDetectionService::IsBadIpAddress(
    const std::string& ip_address) const {
  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(ip_address, &ip_number)) {
    VLOG(2) << "Unable to parse IP address: '" << ip_address << "'";
    return false;
  }
  if (ip_number.size() == net::kIPv4AddressSize) {
    ip_number = net::ConvertIPv4NumberToIPv6Number(ip_number);
  }
  if (ip_number.size() != net::kIPv6AddressSize) {
    VLOG(2) << "Unable to convert IPv4 address to IPv6: '" << ip_address << "'";
    return false;  // better safe than sorry.
  }
  for (BadSubnetMap::const_iterator it = bad_subnets_.begin();
       it != bad_subnets_.end(); ++it) {
    const std::string& mask = it->first;
    DCHECK_EQ(mask.size(), ip_number.size());
    std::string subnet(net::kIPv6AddressSize, '.');
    for (size_t i = 0; i < net::kIPv6AddressSize; ++i) {
      subnet[i] = ip_number[i] & mask[i];
    }
    if (it->second.count(crypto::SHA256HashString(subnet)) > 0) {
      return true;
    }
  }
  return false;
}

void ClientSideDetectionService::OnURLFetchComplete(
    const content::URLFetcher* source) {
  std::string data;
  source->GetResponseAsString(&data);
  if (source == model_fetcher_.get()) {
    HandleModelResponse(
        source, source->GetURL(), source->GetStatus(),
        source->GetResponseCode(), source->GetCookies(), data);
  } else if (client_phishing_reports_.find(source) !=
             client_phishing_reports_.end()) {
    HandlePhishingVerdict(
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  if (!model_.get()) {
    // Model might not be ready or maybe there was an error.
    return;
  }
  SendModelToProcess(
      content::Source<content::RenderProcessHost>(source).ptr());
}

void ClientSideDetectionService::SendModelToProcess(
    content::RenderProcessHost* process) {
  // The ClientSideDetectionService is enabled if _any_ active profile has
  // SafeBrowsing turned on.  Here we check the profile for each renderer
  // process and only send the model to those that have SafeBrowsing enabled.
  Profile* profile = Profile::FromBrowserContext(process->GetBrowserContext());
  std::string model;
  if (profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    VLOG(2) << "Sending phishing model to RenderProcessHost @" << process;
    model = model_str_;
  } else {
    VLOG(2) << "Disabling client-side phishing detection for "
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

void ClientSideDetectionService::ScheduleFetchModel(int64 delay_ms) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ClientSideDetectionService::StartFetchModel,
                 weak_factory_.GetWeakPtr()),
      delay_ms);
}

void ClientSideDetectionService::StartFetchModel() {
  if (enabled_) {
    // Start fetching the model either from the cache or possibly from the
    // network if the model isn't in the cache.
    model_fetcher_.reset(content::URLFetcher::Create(
        0 /* ID used for testing */, GURL(kClientModelUrl),
        content::URLFetcher::GET, this));
    model_fetcher_->SetRequestContext(request_context_getter_.get());
    model_fetcher_->Start();
  }
}

void ClientSideDetectionService::EndFetchModel(ClientModelStatus status) {
  UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.ClientModelStatus",
                            status,
                            MODEL_STATUS_MAX);
  if (status == MODEL_SUCCESS) {
    SetBadSubnets(*model_, &bad_subnets_);
    SendModelToRenderers();
  }
  int delay_ms = kClientModelFetchIntervalMs;
  // If the most recently fetched model had a valid max-age and the model was
  // valid we're scheduling the next model update for after the max-age expired.
  if (model_max_age_.get() &&
      (status == MODEL_SUCCESS || status == MODEL_NOT_CHANGED)) {
    // We're adding 60s of additional delay to make sure we're past
    // the model's age.
    *model_max_age_ += base::TimeDelta::FromMinutes(1);
    delay_ms = model_max_age_->InMilliseconds();
  }
  model_max_age_.reset();

  // Schedule the next model reload.
  ScheduleFetchModel(delay_ms);
}

void ClientSideDetectionService::StartClientReportPhishingRequest(
    ClientPhishingRequest* verdict,
    ClientReportPhishingRequestCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ClientPhishingRequest> request(verdict);
  scoped_ptr<ClientReportPhishingRequestCallback> cb(callback);

  if (!enabled_) {
    if (cb.get())
      cb->Run(GURL(request->url()), false);
    return;
  }

  std::string request_data;
  if (!request->SerializeToString(&request_data)) {
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.RequestNotSerialized", 1);
    VLOG(1) << "Unable to serialize the CSD request. Proto file changed?";
    if (cb.get()) {
      cb->Run(GURL(request->url()), false);
    }
    return;
  }

  content::URLFetcher* fetcher = content::URLFetcher::Create(
      0 /* ID used for testing */, GURL(kClientReportPhishingUrl),
      content::URLFetcher::POST, this);

  // Remember which callback and URL correspond to the current fetcher object.
  ClientReportInfo* info = new ClientReportInfo;
  info->callback.swap(cb);  // takes ownership of the callback.
  info->phishing_url = GURL(request->url());
  client_phishing_reports_[fetcher] = info;

  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->SetUploadData("application/octet-stream", request_data);
  fetcher->Start();

  // Record that we made a request
  phishing_report_times_.push(base::Time::Now());
}

void ClientSideDetectionService::HandleModelResponse(
    const content::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  base::TimeDelta max_age;
  if (status.is_success() && RC_REQUEST_OK == response_code &&
      source->GetResponseHeaders() &&
      source->GetResponseHeaders()->GetMaxAgeValue(&max_age)) {
    model_max_age_.reset(new base::TimeDelta(max_age));
  }
  scoped_ptr<ClientSideModel> model(new ClientSideModel());
  ClientModelStatus model_status;
  if (!status.is_success() || RC_REQUEST_OK != response_code) {
    model_status = MODEL_FETCH_FAILED;
  } else if (data.empty()) {
    model_status = MODEL_EMPTY;
  } else if (data.size() > kMaxModelSizeBytes) {
    model_status = MODEL_TOO_LARGE;
  } else if (!model->ParseFromString(data)) {
    model_status = MODEL_PARSE_ERROR;
  } else if (!model->IsInitialized() || !model->has_version()) {
    model_status = MODEL_MISSING_FIELDS;
  } else if (!ModelHasValidHashIds(*model)) {
    model_status = MODEL_BAD_HASH_IDS;
  } else if (model->version() < 0 ||
             (model_.get() && model->version() < model_->version())) {
    model_status = MODEL_INVALID_VERSION_NUMBER;
  } else if (model_.get() && model->version() == model_->version()) {
    model_status = MODEL_NOT_CHANGED;
  } else {
    // The model is valid => replace the existing model with the new one.
    model_str_.assign(data);
    model_.swap(model);
    model_status = MODEL_SUCCESS;
  }
  EndFetchModel(model_status);
}

void ClientSideDetectionService::HandlePhishingVerdict(
    const content::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  ClientPhishingResponse response;
  scoped_ptr<ClientReportInfo> info(client_phishing_reports_[source]);
  bool is_phishing = false;
  if (status.is_success() && RC_REQUEST_OK == response_code &&
      response.ParseFromString(data)) {
    // Cache response, possibly flushing an old one.
    cache_[info->phishing_url] =
        make_linked_ptr(new CacheState(response.phishy(), base::Time::Now()));
    is_phishing = (response.phishy() &&
                   !IsFalsePositiveResponse(info->phishing_url, response));
  } else {
    DLOG(ERROR) << "Unable to get the server verdict for URL: "
                << info->phishing_url << " status: " << status.status() << " "
                << "response_code:" << response_code;
  }
  if (info->callback.get()) {
    info->callback->Run(info->phishing_url, is_phishing);
  }
  client_phishing_reports_.erase(source);
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
      cache_state.timestamp > base::Time::Now() - kPositiveCacheInterval :
      cache_state.timestamp > base::Time::Now() - kNegativeCacheInterval) {
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
      std::max(kPositiveCacheInterval, kReportsInterval);
  base::TimeDelta negative_cache_interval =
      std::max(kNegativeCacheInterval, kReportsInterval);

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

bool ClientSideDetectionService::OverReportLimit() {
  return GetNumReports() > kMaxReportsPerInterval;
}

int ClientSideDetectionService::GetNumReports() {
  base::Time cutoff = base::Time::Now() - kReportsInterval;

  // Erase items older than cutoff because we will never care about them again.
  while (!phishing_report_times_.empty() &&
         phishing_report_times_.front() < cutoff) {
    phishing_report_times_.pop();
  }

  // Return the number of elements that are above the cutoff.
  return phishing_report_times_.size();
}

bool ClientSideDetectionService::InitializePrivateNetworks() {
  static const char* const kPrivateNetworks[] = {
    "10.0.0.0/8",
    "127.0.0.0/8",
    "172.16.0.0/12",
    "192.168.0.0/16",
    // IPv6 address ranges
    "fc00::/7",
    "fec0::/10",
    "::1/128",
  };

  for (size_t i = 0; i < arraysize(kPrivateNetworks); ++i) {
    net::IPAddressNumber ip_number;
    size_t prefix_length;
    if (net::ParseCIDRBlock(kPrivateNetworks[i], &ip_number, &prefix_length)) {
      private_networks_.push_back(std::make_pair(ip_number, prefix_length));
    } else {
      DLOG(FATAL) << "Unable to parse IP address range: "
                  << kPrivateNetworks[i];
      return false;
    }
  }
  return true;
}

// static
void ClientSideDetectionService::SetBadSubnets(const ClientSideModel& model,
                                               BadSubnetMap* bad_subnets) {
  bad_subnets->clear();
  for (int i = 0; i < model.bad_subnet_size(); ++i) {
    int size = model.bad_subnet(i).size();
    if (size < 0 || size > static_cast<int>(net::kIPv6AddressSize) * 8) {
      DLOG(ERROR) << "Invalid bad subnet size: " << size;
      continue;
    }
    if (model.bad_subnet(i).prefix().size() != crypto::kSHA256Length) {
      DLOG(ERROR) << "Invalid bad subnet prefix length: "
                  << model.bad_subnet(i).prefix().size();
      continue;
    }
    // We precompute the mask for the given subnet size to speed up lookups.
    // Basically we need to create a 16B long string which has the highest
    // |size| bits sets to one.
    std::string mask(net::kIPv6AddressSize, '\x00');
    mask.replace(0, size / 8, size / 8, '\xFF');
    if (size % 8) {
      mask[size / 8] = 0xFF << (8 - (size % 8));
    }
    (*bad_subnets)[mask].insert(model.bad_subnet(i).prefix());
  }
}

// static
bool ClientSideDetectionService::ModelHasValidHashIds(
    const ClientSideModel& model) {
  const int max_index = model.hashes_size() - 1;
  for (int i = 0; i < model.rule_size(); ++i) {
    for (int j = 0; j < model.rule(i).feature_size(); ++j) {
      if (model.rule(i).feature(j) < 0 ||
          model.rule(i).feature(j) > max_index) {
        return false;
      }
    }
  }
  for (int i = 0; i < model.page_term_size(); ++i) {
    if (model.page_term(i) < 0 || model.page_term(i) > max_index) {
      return false;
    }
  }
  return true;
}

// static
bool ClientSideDetectionService::IsFalsePositiveResponse(
    const GURL& url,
    const ClientPhishingResponse& response) {
  if (!response.phishy() || response.whitelist_expression_size() == 0) {
    return false;
  }
  // This whitelist is special.  A particular URL gets whitelisted if it
  // matches any of the expressions on the whitelist or if any of the whitelist
  // entries matches the URL.

  std::string host, path, query;
  safe_browsing_util::CanonicalizeUrl(url, &host, &path, &query);
  std::string canonical_url_as_pattern = host + path + query;

  std::vector<std::string> url_patterns;
  safe_browsing_util::GeneratePatternsToCheck(url, &url_patterns);

  for (int i = 0; i < response.whitelist_expression_size(); ++i) {
    GURL whitelisted_url(std::string("http://") +
                         response.whitelist_expression(i));
    if (!whitelisted_url.is_valid()) {
      UMA_HISTOGRAM_COUNTS("SBClientPhishing.InvalidWhitelistExpression", 1);
      continue;  // Skip invalid whitelist expressions.
    }
    // First, we check whether the canonical URL matches any of the whitelisted
    // expressions.
    for (size_t j = 0; j < url_patterns.size(); ++j) {
      if (url_patterns[j] == response.whitelist_expression(i)) {
        return true;
      }
    }
    // Second, we consider the canonical URL as an expression and we check
    // whether any of the whitelist entries matches that expression.
    std::vector<std::string> whitelist_patterns;
    safe_browsing_util::GeneratePatternsToCheck(whitelisted_url,
                                                &whitelist_patterns);
    for (size_t j = 0; j < whitelist_patterns.size(); ++j) {
      if (whitelist_patterns[j] == canonical_url_as_pattern) {
        return true;
      }
    }
  }
  return false;
}
}  // namespace safe_browsing
