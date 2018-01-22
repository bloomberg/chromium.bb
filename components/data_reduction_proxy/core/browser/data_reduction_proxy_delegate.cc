// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"

#include <cmath>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_info.h"

namespace data_reduction_proxy {

namespace {

static const char kDataReductionCoreProxy[] = "proxy.googlezip.net";

// Adds data reduction proxies to |result|, where applicable, if result
// otherwise uses a direct connection for |url|, and the data reduction proxy is
// not bypassed. Also, configures |result| to proceed directly to the origin if
// |result|'s current proxy is the data reduction proxy.
void OnResolveProxyHandler(
    const GURL& url,
    const std::string& method,
    const net::ProxyConfig& proxy_config,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    const DataReductionProxyConfig& data_reduction_proxy_config,
    DataReductionProxyIOData* io_data,
    net::ProxyInfo* result) {
  DCHECK(result->is_empty() || result->is_direct() ||
         !data_reduction_proxy_config.IsDataReductionProxy(
             result->proxy_server(), nullptr));

  if (!util::EligibleForDataReductionProxy(*result, url, method))
    return;

  net::ProxyInfo data_reduction_proxy_info;
  bool data_saver_proxy_used = util::ApplyProxyConfigToProxyInfo(
      proxy_config, proxy_retry_info, url, &data_reduction_proxy_info);
  if (data_saver_proxy_used)
    result->OverrideProxyList(data_reduction_proxy_info.proxy_list());

  if (io_data && io_data->resource_type_provider()) {
    ResourceTypeProvider::ContentType content_type =
        io_data->resource_type_provider()->GetContentType(url);
    DCHECK_GT(ResourceTypeProvider::CONTENT_TYPE_MAX, content_type);
    UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.ResourceContentType",
                              content_type,
                              ResourceTypeProvider::CONTENT_TYPE_MAX);
  }

  // The |proxy_config| must be valid otherwise the proxy cannot be used.
  DCHECK(proxy_config.is_valid() || !data_saver_proxy_used);

  if (data_reduction_proxy_config.enabled_by_user_and_reachable() &&
      url.SchemeIs(url::kHttpScheme) && !net::IsLocalhost(url) &&
      !params::IsIncludedInHoldbackFieldTrial()) {
    UMA_HISTOGRAM_BOOLEAN(
        "DataReductionProxy.ConfigService.HTTPRequests",
        !data_reduction_proxy_config.GetProxiesForHttp().empty());
  }
}

}  // namespace

DataReductionProxyDelegate::DataReductionProxyDelegate(
    DataReductionProxyConfig* config,
    const DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator,
    DataReductionProxyBypassStats* bypass_stats,
    net::NetLog* net_log)
    : config_(config),
      configurator_(configurator),
      event_creator_(event_creator),
      bypass_stats_(bypass_stats),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      first_data_saver_request_recorded_(false),
      io_data_(nullptr),
      net_log_(net_log) {
  DCHECK(config_);
  DCHECK(configurator_);
  DCHECK(event_creator_);
  DCHECK(bypass_stats_);
  DCHECK(net_log_);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyDelegate::~DataReductionProxyDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void DataReductionProxyDelegate::InitializeOnIOThread(
    DataReductionProxyIOData* io_data) {
  DCHECK(io_data);
  DCHECK(thread_checker_.CalledOnValidThread());
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  io_data_ = io_data;
}

void DataReductionProxyDelegate::OnResolveProxy(
    const GURL& url,
    const std::string& method,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyInfo* result) {
  DCHECK(result);
  DCHECK(thread_checker_.CalledOnValidThread());

  ResourceTypeProvider::ContentType content_type =
      ResourceTypeProvider::CONTENT_TYPE_UNKNOWN;

  if (io_data_ && io_data_->resource_type_provider()) {
    content_type = io_data_->resource_type_provider()->GetContentType(url);
  }

  std::vector<DataReductionProxyServer> proxies_for_http =
      params::IsIncludedInHoldbackFieldTrial()
          ? std::vector<DataReductionProxyServer>()
          : config_->GetProxiesForHttp();

  // Remove the proxies that are unsupported for this request.
  proxies_for_http.erase(
      std::remove_if(proxies_for_http.begin(), proxies_for_http.end(),
                     [content_type](const DataReductionProxyServer& proxy) {
                       return !proxy.SupportsResourceType(content_type);
                     }),
      proxies_for_http.end());

  base::Optional<std::pair<bool /* is_secure_proxy */, bool /*is_core_proxy */>>
      warmup_proxy = config_->GetInFlightWarmupProxyDetails();

  bool is_warmup_url = warmup_proxy &&
                       url.host() == params::GetWarmupURL().host() &&
                       url.path() == params::GetWarmupURL().path();

  if (is_warmup_url) {
    // This is a request to fetch the warmup (aka probe) URL.
    // |is_secure_proxy| and |is_core_proxy| indicate the properties of the
    // proxy that is being currently probed.
    bool is_secure_proxy = warmup_proxy->first;
    bool is_core_proxy = warmup_proxy->second;
    // Remove the proxies with properties that do not match the properties of
    // the proxy that is being probed.
    proxies_for_http.erase(
        std::remove_if(proxies_for_http.begin(), proxies_for_http.end(),
                       [is_secure_proxy,
                        is_core_proxy](const DataReductionProxyServer& proxy) {
                         return proxy.IsSecureProxy() != is_secure_proxy ||
                                proxy.IsCoreProxy() != is_core_proxy;
                       }),
        proxies_for_http.end());
  }

  // If the proxy is disabled due to warmup URL fetch failing in the past,
  // then enable it temporarily. This ensures that |configurator_| includes
  // this proxy type when generating the |proxy_config|.
  net::ProxyConfig proxy_config = configurator_->CreateProxyConfig(
      is_warmup_url, config_->GetNetworkPropertiesManager(), proxies_for_http);

  OnResolveProxyHandler(url, method, proxy_config, proxy_retry_info, *config_,
                        io_data_, result);

  if (!result->is_empty()) {
    net::ProxyServer alternative_proxy_server;
    if (GetAlternativeProxy(url, result->proxy_server(), proxy_retry_info,
                            &alternative_proxy_server)) {
      result->SetAlternativeProxy(alternative_proxy_server);
    }
  }

  if (!first_data_saver_request_recorded_ && !result->is_empty() &&
      config_->IsDataReductionProxy(result->proxy_server(), nullptr)) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "DataReductionProxy.TimeToFirstDataSaverRequest",
        tick_clock_->NowTicks() - last_network_change_time_);
    first_data_saver_request_recorded_ = true;
  }
}

void DataReductionProxyDelegate::OnFallback(const net::ProxyServer& bad_proxy,
                                            int net_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (bad_proxy.is_valid() &&
      config_->IsDataReductionProxy(bad_proxy, nullptr)) {
    event_creator_->AddProxyFallbackEvent(net_log_, bad_proxy.ToURI(),
                                          net_error);
  }

  if (bypass_stats_)
    bypass_stats_->OnProxyFallback(bad_proxy, net_error);
}

bool DataReductionProxyDelegate::IsTrustedSpdyProxy(
    const net::ProxyServer& proxy_server) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return proxy_server.is_valid() && proxy_server.is_https() && config_ &&
         config_->IsDataReductionProxy(proxy_server, nullptr);
}

void DataReductionProxyDelegate::SetTickClockForTesting(
    base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  // Update |last_network_change_time_| to the provided tick clock's current
  // time for testing.
  last_network_change_time_ = tick_clock_->NowTicks();
}

bool DataReductionProxyDelegate::GetAlternativeProxy(
    const GURL& url,
    const net::ProxyServer& resolved_proxy_server,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyServer* alternative_proxy_server) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      url.SchemeIsCryptographic()) {
    return false;
  }

  if (!params::IsIncludedInQuicFieldTrial()) {
    RecordQuicProxyStatus(QUIC_PROXY_DISABLED_VIA_FIELD_TRIAL);
    return false;
  }

  if (!resolved_proxy_server.is_valid() || !resolved_proxy_server.is_https())
    return false;

  if (!config_ ||
      !config_->IsDataReductionProxy(resolved_proxy_server, nullptr)) {
    return false;
  }

  if (!SupportsQUIC(resolved_proxy_server)) {
    RecordQuicProxyStatus(QUIC_PROXY_NOT_SUPPORTED);
    return false;
  }

  net::ProxyInfo alternative_proxy_info;
  alternative_proxy_info.UseProxyServer(net::ProxyServer(
      net::ProxyServer::SCHEME_QUIC, resolved_proxy_server.host_port_pair()));
  alternative_proxy_info.DeprioritizeBadProxies(proxy_retry_info);

  if (alternative_proxy_info.is_empty()) {
    RecordQuicProxyStatus(QUIC_PROXY_STATUS_MARKED_AS_BROKEN);
    return false;
  }

  RecordQuicProxyStatus(QUIC_PROXY_STATUS_AVAILABLE);
  *alternative_proxy_server = alternative_proxy_info.proxy_server();
  return true;
}

bool DataReductionProxyDelegate::SupportsQUIC(
    const net::ProxyServer& proxy_server) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Enable QUIC for whitelisted proxies.
  return params::IsQuicEnabledForNonCoreProxies() ||
         proxy_server ==
             net::ProxyServer(net::ProxyServer::SCHEME_HTTPS,
                              net::HostPortPair(kDataReductionCoreProxy, 443));
}

void DataReductionProxyDelegate::RecordQuicProxyStatus(
    QuicProxyStatus status) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.Quic.ProxyStatus", status,
                            QUIC_PROXY_STATUS_BOUNDARY);
}

void DataReductionProxyDelegate::OnIPAddressChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  first_data_saver_request_recorded_ = false;
  last_network_change_time_ = tick_clock_->NowTicks();
}

}  // namespace data_reduction_proxy
