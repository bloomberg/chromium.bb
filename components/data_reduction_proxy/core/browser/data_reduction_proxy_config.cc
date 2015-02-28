// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/load_flags.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace {

// Values of the UMA DataReductionProxy.NetworkChangeEvents histograms.
// This enum must remain synchronized with the enum of the same
// name in metrics/histograms/histograms.xml.
enum DataReductionProxyNetworkChangeEvent {
  IP_CHANGED = 0,         // The client IP address changed.
  DISABLED_ON_VPN = 1,    // The proxy is disabled because a VPN is running.
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

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyConfig::DataReductionProxyConfig(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    net::NetLog* net_log,
    scoped_ptr<DataReductionProxyParams> params,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventStore* event_store,
    bool enable_quic)
    : restricted_by_carrier_(false),
      disabled_on_vpn_(false),
      unreachable_(false),
      enabled_by_user_(false),
      alternative_enabled_by_user_(false),
      params_(params.release()),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      net_log_(net_log),
      configurator_(configurator),
      event_store_(event_store) {
  DCHECK(io_task_runner);
  DCHECK(ui_task_runner);
  DCHECK(configurator);
  DCHECK(event_store);
  params_->EnableQuic(enable_quic);
  InitOnIOThread();
}

DataReductionProxyConfig::~DataReductionProxyConfig() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void DataReductionProxyConfig::SetDataReductionProxyService(
    base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service) {
  data_reduction_proxy_service_ = data_reduction_proxy_service;
}

void DataReductionProxyConfig::SetProxyPrefs(bool enabled,
                                             bool alternative_enabled,
                                             bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DataReductionProxyConfig::SetProxyConfigOnIOThread,
                            base::Unretained(this), enabled,
                            alternative_enabled, at_startup));
}

bool DataReductionProxyConfig::WasDataReductionProxyUsed(
    const net::URLRequest* request,
    DataReductionProxyTypeInfo* proxy_info) const {
  return params_->WasDataReductionProxyUsed(request, proxy_info);
}

bool DataReductionProxyConfig::IsDataReductionProxy(
    const net::HostPortPair& host_port_pair,
    DataReductionProxyTypeInfo* proxy_info) const {
  return params_->IsDataReductionProxy(host_port_pair, proxy_info);
}

bool DataReductionProxyConfig::IsBypassedByDataReductionProxyLocalRules(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config) const {
  return params_->IsBypassedByDataReductionProxyLocalRules(
      request, data_reduction_proxy_config);
}

bool DataReductionProxyConfig::AreDataReductionProxiesBypassed(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config,
    base::TimeDelta* min_retry_delay) const {
  return params_->AreDataReductionProxiesBypassed(
      request, data_reduction_proxy_config, min_retry_delay);
}

bool DataReductionProxyConfig::IsProxyBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyServer& proxy_server,
    base::TimeDelta* retry_delay) const {
  return params_->IsProxyBypassed(retry_map, proxy_server, retry_delay);
}

bool DataReductionProxyConfig::ContainsDataReductionProxy(
    const net::ProxyConfig::ProxyRules& proxy_rules) {
  // Data Reduction Proxy configurations are always TYPE_PROXY_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME)
    return false;

  const net::ProxyList* https_proxy_list =
      proxy_rules.MapUrlSchemeToProxyList("https");
  if (https_proxy_list && !https_proxy_list->IsEmpty() &&
      // Sufficient to check only the first proxy.
      params_->IsDataReductionProxy(https_proxy_list->Get().host_port_pair(),
                                    NULL)) {
    return true;
  }

  const net::ProxyList* http_proxy_list =
      proxy_rules.MapUrlSchemeToProxyList("http");
  if (http_proxy_list && !http_proxy_list->IsEmpty() &&
      // Sufficient to check only the first proxy.
      params_->IsDataReductionProxy(http_proxy_list->Get().host_port_pair(),
                                    NULL)) {
    return true;
  }

  return false;
}

bool DataReductionProxyConfig::UsingHTTPTunnel(
    const net::HostPortPair& proxy_server) const {
  return params_->ssl_origin().is_valid() &&
         params_->ssl_origin().host_port_pair().Equals(proxy_server);
}

const net::ProxyServer& DataReductionProxyConfig::Origin() {
  return params_->origin();
}

// Returns true if the Data Reduction Proxy configuration may be used.
bool DataReductionProxyConfig::allowed() {
  return params_->allowed();
}

// Returns true if the alternative Data Reduction Proxy configuration may be
// used.
bool DataReductionProxyConfig::alternative_allowed() {
  return params_->alternative_allowed();
}

// Returns true if the Data Reduction Proxy promo may be shown. This is not
// tied to whether the Data Reduction Proxy is enabled.
bool DataReductionProxyConfig::promo_allowed() {
  return params_->promo_allowed();
}

void DataReductionProxyConfig::SetProxyConfigOnIOThread(
    bool enabled, bool alternative_enabled, bool at_startup) {
  enabled_by_user_ = enabled;
  alternative_enabled_by_user_ = alternative_enabled;
  UpdateConfigurator(enabled_by_user_, alternative_enabled_by_user_,
                     restricted_by_carrier_, at_startup);

  // Check if the proxy has been restricted explicitly by the carrier.
  if (enabled &&
      !(alternative_enabled && !params_->alternative_fallback_allowed())) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DataReductionProxyConfig::StartSecureProxyCheck,
                              base::Unretained(this)));
  }
}

void DataReductionProxyConfig::UpdateConfigurator(bool enabled,
                                                  bool alternative_enabled,
                                                  bool restricted,
                                                  bool at_startup) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(configurator_);
  LogProxyState(enabled, restricted, at_startup);
  // The alternative is only configured if the standard configuration is
  // is enabled.
  if (enabled & !params_->holdback()) {
    if (alternative_enabled) {
      configurator_->Enable(restricted,
                            !params_->alternative_fallback_allowed(),
                            params_->alt_origin().ToURI(), std::string(),
                            params_->ssl_origin().ToURI());
    } else {
      configurator_->Enable(restricted, !params_->fallback_allowed(),
                            params_->origin().ToURI(),
                            params_->fallback_origin().ToURI(), std::string());
    }
  } else {
    configurator_->Disable();
  }
}

void DataReductionProxyConfig::LogProxyState(bool enabled,
                                             bool restricted,
                                             bool at_startup) {
  // This must stay a LOG(WARNING); the output is used in processing customer
  // feedback.
  const char kAtStartup[] = "at startup";
  const char kByUser[] = "by user action";
  const char kOn[] = "ON";
  const char kOff[] = "OFF";
  const char kRestricted[] = "(Restricted)";
  const char kUnrestricted[] = "(Unrestricted)";

  std::string annotated_on =
      kOn + std::string(" ") + (restricted ? kRestricted : kUnrestricted);

  LOG(WARNING) << "SPDY proxy " << (enabled ? annotated_on : kOff) << " "
               << (at_startup ? kAtStartup : kByUser);
}

void DataReductionProxyConfig::HandleSecureProxyCheckResponse(
    const std::string& response, const net::URLRequestStatus& status) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &DataReductionProxyConfig::HandleSecureProxyCheckResponseOnIOThread,
          base::Unretained(this), response, status));
}

void DataReductionProxyConfig::HandleSecureProxyCheckResponseOnIOThread(
    const std::string& response, const net::URLRequestStatus& status) {
  if (event_store_) {
    event_store_->EndSecureProxyCheck(bound_net_log_, status.error());
  }

  if (status.status() == net::URLRequestStatus::FAILED) {
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

  if ("OK" == response.substr(0, 2)) {
    DVLOG(1) << "The data reduction proxy is unrestricted.";

    if (enabled_by_user_) {
      if (restricted_by_carrier_) {
        // The user enabled the proxy, but sometime previously in the session,
        // the network operator had blocked the secure proxy check and
        // restricted the user. The current network doesn't block the secure
        // proxy check, so don't restrict the proxy configurations.
        UpdateConfigurator(true /* enabled */, false /* alternative_enabled */,
                           false /* restricted */, false /* at_startup */);
        RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ENABLED);
      } else {
        RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ALREADY_ENABLED);
      }
    }
    restricted_by_carrier_ = false;
    return;
  }
  DVLOG(1) << "The data reduction proxy is restricted to the configured "
           << "fallback proxy.";
  if (enabled_by_user_) {
    if (!restricted_by_carrier_) {
      // Restrict the proxy.
      UpdateConfigurator(true /* enabled */, false /* alternative_enabled */,
                         true /* restricted */, false /* at_startup */);
      RecordSecureProxyCheckFetchResult(FAILED_PROXY_DISABLED);
    } else {
      RecordSecureProxyCheckFetchResult(FAILED_PROXY_ALREADY_DISABLED);
    }
  }
  restricted_by_carrier_ = true;
}

void DataReductionProxyConfig::OnIPAddressChanged() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (enabled_by_user_) {
    DCHECK(params_->allowed());
    RecordNetworkChangeEvent(IP_CHANGED);
    if (DisableIfVPN())
      return;
    if (alternative_enabled_by_user_ &&
        !params_->alternative_fallback_allowed()) {
      return;
    }

    ui_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DataReductionProxyConfig::StartSecureProxyCheck,
                              base::Unretained(this)));
  }
}

void DataReductionProxyConfig::InitOnIOThread() {
  if (!io_task_runner_->BelongsToCurrentThread()) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DataReductionProxyConfig::InitOnIOThread,
                              base::Unretained(this)));
    return;
  }

  if (!params_->allowed())
    return;

  AddDefaultProxyBypassRules();
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
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

void DataReductionProxyConfig::RecordSecureProxyCheckFetchResult(
    SecureProxyCheckFetchResult result) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyProbeURL, result,
                            SECURE_PROXY_CHECK_FETCH_RESULT_COUNT);
}

void DataReductionProxyConfig::StartSecureProxyCheck() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  bound_net_log_ = net::BoundNetLog::Make(
      net_log_, net::NetLog::SOURCE_DATA_REDUCTION_PROXY);
  if (data_reduction_proxy_service_) {
    if (event_store_) {
      event_store_->BeginSecureProxyCheck(bound_net_log_,
                                          params_->secure_proxy_check_url());
    }

    data_reduction_proxy_service_->SecureProxyCheck(
        params_->secure_proxy_check_url(),
        base::Bind(&DataReductionProxyConfig::HandleSecureProxyCheckResponse,
                   base::Unretained(this)));
  }
}

void DataReductionProxyConfig::GetNetworkList(
    net::NetworkInterfaceList* interfaces,
    int policy) {
  net::GetNetworkList(interfaces, policy);
}

bool DataReductionProxyConfig::DisableIfVPN() {
  net::NetworkInterfaceList network_interfaces;
  GetNetworkList(&network_interfaces, 0);
  // VPNs use a "tun" interface, so the presence of a "tun" interface indicates
  // a VPN is in use.
  // TODO(kundaji): Verify this works on Windows.
  const std::string vpn_interface_name_prefix = "tun";
  for (size_t i = 0; i < network_interfaces.size(); ++i) {
    std::string interface_name = network_interfaces[i].name;
    if (LowerCaseEqualsASCII(
            interface_name.begin(),
            interface_name.begin() + vpn_interface_name_prefix.size(),
            vpn_interface_name_prefix.c_str())) {
      UpdateConfigurator(false, alternative_enabled_by_user_, false, false);
      disabled_on_vpn_ = true;
      RecordNetworkChangeEvent(DISABLED_ON_VPN);
      return true;
    }
  }
  if (disabled_on_vpn_) {
    UpdateConfigurator(enabled_by_user_, alternative_enabled_by_user_,
                       restricted_by_carrier_, false);
  }
  disabled_on_vpn_ = false;
  return false;
}

}  // namespace data_reduction_proxy
