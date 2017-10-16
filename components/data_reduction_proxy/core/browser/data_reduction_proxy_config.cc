// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/warmup_url_fetcher.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/previews/core/previews_decider.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log_source_type.h"
#include "net/nqe/effective_connection_type.h"
#include "net/proxy/proxy_server.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif  // OS_ANDROID

using base::FieldTrialList;

namespace {

// Values of the UMA DataReductionProxy.Protocol.NotAcceptingTransform histogram
// defined in metrics/histograms/histograms.xml. This enum must remain
// synchronized with DataReductionProxyProtocolNotAcceptingTransformReason in
// tools/metrics/histograms/enums.xml.
enum NotAcceptingTransformReason {
  NOT_ACCEPTING_TRANSFORM_DISABLED = 0,
  NOT_ACCEPTING_TRANSFORM_BLACKLISTED = 1,
  NOT_ACCEPTING_TRANSFORM_CELLULAR_ONLY = 2,
  NOT_ACCEPTING_TRANSFORM_REASON_BOUNDARY
};

// Values of the UMA DataReductionProxy.NetworkChangeEvents histograms.
// This enum must remain synchronized with the enum of the same
// name in metrics/histograms/histograms.xml.
enum DataReductionProxyNetworkChangeEvent {
  IP_CHANGED = 0,         // The client IP address changed.
  DISABLED_ON_VPN = 1,    // [Deprecated] Proxy is disabled because a VPN is
                          // running.
  CHANGE_EVENT_COUNT = 2  // This must always be last.
};

// Key of the UMA DataReductionProxy.ProbeURL histogram.
const char kUMAProxyProbeURL[] = "DataReductionProxy.ProbeURL";

// Key of the UMA DataReductionProxy.ProbeURLNetError histogram.
const char kUMAProxyProbeURLNetError[] = "DataReductionProxy.ProbeURLNetError";

// Record a network change event.
void RecordNetworkChangeEvent(DataReductionProxyNetworkChangeEvent event) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.NetworkChangeEvents", event,
                            CHANGE_EVENT_COUNT);
}

//  Records UMA containing the result of requesting the secure proxy check.
void RecordSecureProxyCheckFetchResult(
    data_reduction_proxy::SecureProxyCheckFetchResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      kUMAProxyProbeURL, result,
      data_reduction_proxy::SECURE_PROXY_CHECK_FETCH_RESULT_COUNT);
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyConfig::DataReductionProxyConfig(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    net::NetLog* net_log,
    std::unique_ptr<DataReductionProxyConfigValues> config_values,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : secure_proxy_allowed_(true),
      unreachable_(false),
      enabled_by_user_(false),
      config_values_(std::move(config_values)),
      io_task_runner_(io_task_runner),
      net_log_(net_log),
      configurator_(configurator),
      event_creator_(event_creator),
      connection_type_(net::NetworkChangeNotifier::GetConnectionType()),
      is_captive_portal_(false),
      weak_factory_(this) {
  DCHECK(io_task_runner_);
  DCHECK(configurator);
  DCHECK(event_creator);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfig::~DataReductionProxyConfig() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void DataReductionProxyConfig::InitializeOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>&
        basic_url_request_context_getter,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  DCHECK(thread_checker_.CalledOnValidThread());

  secure_proxy_checker_.reset(
      new SecureProxyChecker(basic_url_request_context_getter));
  warmup_url_fetcher_.reset(new WarmupURLFetcher(url_request_context_getter));

  AddDefaultProxyBypassRules();
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

void DataReductionProxyConfig::ReloadConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(configurator_);

  if (enabled_by_user_ && !params::IsIncludedInHoldbackFieldTrial() &&
      !config_values_->proxies_for_http().empty()) {
    configurator_->Enable(!secure_proxy_allowed_ || is_captive_portal_,
                          config_values_->proxies_for_http());
  } else {
    configurator_->Disable();
  }
}

bool DataReductionProxyConfig::WasDataReductionProxyUsed(
    const net::URLRequest* request,
    DataReductionProxyTypeInfo* proxy_info) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request);
  return IsDataReductionProxy(request->proxy_server(), proxy_info);
}

bool DataReductionProxyConfig::IsDataReductionProxy(
    const net::ProxyServer& proxy_server,
    DataReductionProxyTypeInfo* proxy_info) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!proxy_server.is_valid() || proxy_server.is_direct())
    return false;

  // Only compare the host port pair of the |proxy_server| since the proxy
  // scheme of the stored data reduction proxy may be different than the proxy
  // scheme of |proxy_server|. This may happen even when the |proxy_server| is a
  // valid data reduction proxy. As an example, the stored data reduction proxy
  // may have a proxy scheme of HTTPS while |proxy_server| may have QUIC as the
  // proxy scheme.
  const net::HostPortPair& host_port_pair = proxy_server.host_port_pair();

  const std::vector<DataReductionProxyServer>& data_reduction_proxy_servers =
      config_values_->proxies_for_http();

  const auto proxy_it = std::find_if(
      data_reduction_proxy_servers.begin(), data_reduction_proxy_servers.end(),
      [&host_port_pair](const DataReductionProxyServer& proxy) {
        return proxy.proxy_server().is_valid() &&
               proxy.proxy_server().host_port_pair().Equals(host_port_pair);
      });

  if (proxy_it == data_reduction_proxy_servers.end())
    return false;

  if (!proxy_info)
    return true;

  proxy_info->proxy_servers =
      DataReductionProxyServer::ConvertToNetProxyServers(
          std::vector<DataReductionProxyServer>(
              proxy_it, data_reduction_proxy_servers.end()));
  proxy_info->proxy_index =
      static_cast<size_t>(proxy_it - data_reduction_proxy_servers.begin());
  return true;
}

bool DataReductionProxyConfig::IsBypassedByDataReductionProxyLocalRules(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request.context());
  DCHECK(request.context()->proxy_service());
  net::ProxyInfo result;
  data_reduction_proxy_config.proxy_rules().Apply(
      request.url(), &result);
  if (!result.proxy_server().is_valid())
    return true;
  if (result.proxy_server().is_direct())
    return true;
  return !IsDataReductionProxy(result.proxy_server(), NULL);
}

bool DataReductionProxyConfig::AreDataReductionProxiesBypassed(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config,
    base::TimeDelta* min_retry_delay) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (request.context() != NULL &&
      request.context()->proxy_service() != NULL) {
    return AreProxiesBypassed(
        request.context()->proxy_service()->proxy_retry_info(),
        data_reduction_proxy_config.proxy_rules(),
        request.url().SchemeIsCryptographic(), min_retry_delay);
  }

  return false;
}

bool DataReductionProxyConfig::AreProxiesBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyConfig::ProxyRules& proxy_rules,
    bool is_https,
    base::TimeDelta* min_retry_delay) const {
  // Data reduction proxy config is TYPE_PROXY_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME)
    return false;

  if (is_https)
    return false;

  const net::ProxyList* proxies =
      proxy_rules.MapUrlSchemeToProxyList(url::kHttpScheme);

  if (!proxies)
    return false;

  base::TimeDelta min_delay = base::TimeDelta::Max();
  bool bypassed = false;

  for (const net::ProxyServer& proxy : proxies->GetAll()) {
    if (!proxy.is_valid() || proxy.is_direct())
      continue;

    base::TimeDelta delay;
    if (IsDataReductionProxy(proxy, NULL)) {
      if (!IsProxyBypassed(retry_map, proxy, &delay))
        return false;
      if (delay < min_delay)
        min_delay = delay;
      bypassed = true;
    }
  }

  if (min_retry_delay && bypassed)
    *min_retry_delay = min_delay;

  return bypassed;
}

bool DataReductionProxyConfig::IsProxyBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyServer& proxy_server,
    base::TimeDelta* retry_delay) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::ProxyRetryInfoMap::const_iterator found =
      retry_map.find(proxy_server.ToURI());

  if (found == retry_map.end() || found->second.bad_until < GetTicksNow()) {
    return false;
  }

  if (retry_delay)
     *retry_delay = found->second.current_delay;

  return true;
}

bool DataReductionProxyConfig::ContainsDataReductionProxy(
    const net::ProxyConfig::ProxyRules& proxy_rules) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Data Reduction Proxy configurations are always TYPE_PROXY_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME)
    return false;

  const net::ProxyList* http_proxy_list =
      proxy_rules.MapUrlSchemeToProxyList("http");
  if (http_proxy_list && !http_proxy_list->IsEmpty() &&
      // Sufficient to check only the first proxy.
      IsDataReductionProxy(http_proxy_list->Get(), NULL)) {
    return true;
  }

  return false;
}

void DataReductionProxyConfig::SetProxyConfig(bool enabled, bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_by_user_ = enabled;
  ReloadConfig();

  if (enabled) {
    HandleCaptivePortal();
    FetchWarmupURL();

    // Check if the proxy has been restricted explicitly by the carrier.
    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives
    // |secure_proxy_checker_|.
    SecureProxyCheck(
        base::Bind(&DataReductionProxyConfig::HandleSecureProxyCheckResponse,
                   base::Unretained(this)));
  }
}

void DataReductionProxyConfig::HandleCaptivePortal() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool is_captive_portal = GetIsCaptivePortal();
  UMA_HISTOGRAM_BOOLEAN("DataReductionProxy.CaptivePortalDetected.Platform",
                        is_captive_portal);
  if (is_captive_portal == is_captive_portal_)
    return;
  is_captive_portal_ = is_captive_portal;
  ReloadConfig();
}

bool DataReductionProxyConfig::GetIsCaptivePortal() const {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(OS_ANDROID)
  return net::android::GetIsCaptivePortal();
#endif  // OS_ANDROID
  return false;
}

void DataReductionProxyConfig::UpdateConfigForTesting(
    bool enabled,
    bool secure_proxy_allowed) {
  enabled_by_user_ = enabled;
  secure_proxy_allowed_ = secure_proxy_allowed;
}

void DataReductionProxyConfig::HandleSecureProxyCheckResponse(
    const std::string& response,
    const net::URLRequestStatus& status,
    int http_response_code) {
  bool success_response =
      base::StartsWith(response, "OK", base::CompareCase::SENSITIVE);
  if (event_creator_) {
    event_creator_->EndSecureProxyCheck(net_log_with_source_, status.error(),
                                        http_response_code, success_response);
  }

  if (!status.is_success()) {
    if (status.error() == net::ERR_INTERNET_DISCONNECTED) {
      RecordSecureProxyCheckFetchResult(INTERNET_DISCONNECTED);
      return;
    }
    // TODO(bengr): Remove once we understand the reasons secure proxy checks
    // are failing. Secure proxy check errors are either due to fetcher-level
    // errors or modified responses. This only tracks the former.
    UMA_HISTOGRAM_SPARSE_SLOWLY(kUMAProxyProbeURLNetError,
                                std::abs(status.error()));
  }

  bool secure_proxy_allowed_past = secure_proxy_allowed_;
  secure_proxy_allowed_ = success_response;
  if (!enabled_by_user_)
    return;

  if (secure_proxy_allowed_ != secure_proxy_allowed_past)
    ReloadConfig();

  // Record the result.
  if (secure_proxy_allowed_past && secure_proxy_allowed_) {
    RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ALREADY_ENABLED);
  } else if (secure_proxy_allowed_past && !secure_proxy_allowed_) {
    RecordSecureProxyCheckFetchResult(FAILED_PROXY_DISABLED);
  } else if (!secure_proxy_allowed_past && secure_proxy_allowed_) {
    RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ENABLED);
  } else {
    DCHECK(!secure_proxy_allowed_past && !secure_proxy_allowed_);
    RecordSecureProxyCheckFetchResult(FAILED_PROXY_ALREADY_DISABLED);
  }
}

void DataReductionProxyConfig::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  connection_type_ = type;

  if (connection_type_ == net::NetworkChangeNotifier::CONNECTION_NONE)
    return;

  FetchWarmupURL();
}

void DataReductionProxyConfig::OnIPAddressChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (enabled_by_user_) {
    RecordNetworkChangeEvent(IP_CHANGED);

    HandleCaptivePortal();
    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives
    // |secure_proxy_checker_|.
    SecureProxyCheck(
        base::Bind(&DataReductionProxyConfig::HandleSecureProxyCheckResponse,
                   base::Unretained(this)));
  }
}

void DataReductionProxyConfig::AddDefaultProxyBypassRules() {
  // localhost
  DCHECK(configurator_);
  configurator_->AddHostPatternToBypass("<local>");
  // RFC6890 loopback addresses.
  // TODO(tbansal): Remove this once crbug/446705 is fixed.
  configurator_->AddHostPatternToBypass("127.0.0.0/8");

  // RFC6890 current network (only valid as source address).
  configurator_->AddHostPatternToBypass("0.0.0.0/8");

  // RFC1918 private addresses.
  configurator_->AddHostPatternToBypass("10.0.0.0/8");
  configurator_->AddHostPatternToBypass("172.16.0.0/12");
  configurator_->AddHostPatternToBypass("192.168.0.0/16");

  // RFC3513 unspecified address.
  configurator_->AddHostPatternToBypass("::/128");

  // RFC4193 private addresses.
  configurator_->AddHostPatternToBypass("fc00::/7");
  // IPV6 probe addresses.
  configurator_->AddHostPatternToBypass("*-ds.metric.gstatic.com");
  configurator_->AddHostPatternToBypass("*-v4.metric.gstatic.com");
}

void DataReductionProxyConfig::SecureProxyCheck(
    SecureProxyCheckerCallback fetcher_callback) {
  net_log_with_source_ = net::NetLogWithSource::Make(
      net_log_, net::NetLogSourceType::DATA_REDUCTION_PROXY);
  if (event_creator_) {
    event_creator_->BeginSecureProxyCheck(net_log_with_source_,
                                          params::GetSecureProxyCheckURL());
  }

  secure_proxy_checker_->CheckIfSecureProxyIsAllowed(fetcher_callback);
}

void DataReductionProxyConfig::FetchWarmupURL() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!enabled_by_user_ || !params::FetchWarmupURLEnabled())
    return;

  if (connection_type_ == net::NetworkChangeNotifier::CONNECTION_NONE)
    return;

  warmup_url_fetcher_->FetchWarmupURL();
}

bool DataReductionProxyConfig::ShouldEnableLoFi(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((request.load_flags() & net::LOAD_MAIN_FRAME_DEPRECATED) != 0);
  DCHECK(!request.url().SchemeIsCryptographic());

  return ShouldAcceptServerPreview(request, previews_decider);
}

bool DataReductionProxyConfig::ShouldEnableLitePages(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((request.load_flags() & net::LOAD_MAIN_FRAME_DEPRECATED) != 0);
  DCHECK(!request.url().SchemeIsCryptographic());

  return ShouldAcceptServerPreview(request, previews_decider);
}

bool DataReductionProxyConfig::enabled_by_user_and_reachable() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return enabled_by_user_ && !unreachable_;
}

bool DataReductionProxyConfig::IsBlackListedOrDisabled(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider,
    previews::PreviewsType previews_type) const {
  // Make sure request is not locally blacklisted.
  // Pass in net::EFFECTIVE_CONNECTION_TYPE_4G as the threshold since we
  // just want to check blacklisting here.
  // TODO(crbug.com/720102): Consider new method to just check blacklist.
  return !previews_decider.ShouldAllowPreviewAtECT(
      request, previews_type, net::EFFECTIVE_CONNECTION_TYPE_4G,
      std::vector<std::string>());
}

bool DataReductionProxyConfig::ShouldAcceptServerPreview(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!base::FeatureList::IsEnabled(
          features::kDataReductionProxyDecidesTransform)) {
    return false;
  }

  // For the transition to server-driven previews decisions, we will
  // use existing Lo-Fi flags for disabling and cellular-only mode.
  // TODO(dougarnett): Refactor flag names as part of bug 725645.
  if (params::IsLoFiDisabledViaFlags()) {
    UMA_HISTOGRAM_ENUMERATION(
        "DataReductionProxy.Protocol.NotAcceptingTransform",
        NOT_ACCEPTING_TRANSFORM_DISABLED,
        NOT_ACCEPTING_TRANSFORM_REASON_BOUNDARY);
    return false;
  }

  // AlwaysOn skips blacklist or disabled checks.
  if (params::IsLoFiAlwaysOnViaFlags())
    return true;

  if (IsBlackListedOrDisabled(request, previews_decider,
                              previews::PreviewsType::LITE_PAGE) ||
      IsBlackListedOrDisabled(request, previews_decider,
                              previews::PreviewsType::LOFI)) {
    UMA_HISTOGRAM_ENUMERATION(
        "DataReductionProxy.Protocol.NotAcceptingTransform",
        NOT_ACCEPTING_TRANSFORM_BLACKLISTED,
        NOT_ACCEPTING_TRANSFORM_REASON_BOUNDARY);
    return false;
  }

  if (params::IsLoFiCellularOnlyViaFlags() &&
      !net::NetworkChangeNotifier::IsConnectionCellular(connection_type_)) {
    UMA_HISTOGRAM_ENUMERATION(
        "DataReductionProxy.Protocol.NotAcceptingTransform",
        NOT_ACCEPTING_TRANSFORM_CELLULAR_ONLY,
        NOT_ACCEPTING_TRANSFORM_REASON_BOUNDARY);
    return false;
  }

  return true;
}

base::TimeTicks DataReductionProxyConfig::GetTicksNow() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::TimeTicks::Now();
}

net::ProxyConfig DataReductionProxyConfig::ProxyConfigIgnoringHoldback() const {
  if (!enabled_by_user_ || config_values_->proxies_for_http().empty())
    return net::ProxyConfig::CreateDirect();
  return configurator_->CreateProxyConfig(!secure_proxy_allowed_,
                                          config_values_->proxies_for_http());
}

bool DataReductionProxyConfig::secure_proxy_allowed() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return secure_proxy_allowed_;
}

std::vector<DataReductionProxyServer>
DataReductionProxyConfig::GetProxiesForHttp() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!enabled_by_user_)
    return std::vector<DataReductionProxyServer>();

  return config_values_->proxies_for_http();
}

}  // namespace data_reduction_proxy
