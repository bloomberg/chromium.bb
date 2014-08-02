// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"


using base::StringPrintf;

namespace {
// Values of the UMA DataReductionProxy.NetworkChangeEvents histograms.
// This enum must remain synchronized with the enum of the same
// name in metrics/histograms/histograms.xml.
enum DataReductionProxyNetworkChangeEvent {
  IP_CHANGED = 0, // The client IP address changed.
  DISABLED_ON_VPN = 1, // The proxy is disabled because a VPN is running.
  CHANGE_EVENT_COUNT = 2 // This must always be last.
};

// Key of the UMA DataReductionProxy.StartupState histogram.
const char kUMAProxyStartupStateHistogram[] =
    "DataReductionProxy.StartupState";

// Key of the UMA DataReductionProxy.ProbeURL histogram.
const char kUMAProxyProbeURL[] = "DataReductionProxy.ProbeURL";

// Key of the UMA DataReductionProxy.ProbeURLNetError histogram.
const char kUMAProxyProbeURLNetError[] = "DataReductionProxy.ProbeURLNetError";

// Record a network change event.
void RecordNetworkChangeEvent(DataReductionProxyNetworkChangeEvent event) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.NetworkChangeEvents",
                            event,
                            CHANGE_EVENT_COUNT);
}

int64 GetInt64PrefValue(const base::ListValue& list_value, size_t index) {
  int64 val = 0;
  std::string pref_value;
  bool rv = list_value.GetString(index, &pref_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(pref_value, &val);
    DCHECK(rv);
  }
  return val;
}

bool IsEnabledOnCommandLine() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxySettings::DataReductionProxySettings(
    DataReductionProxyParams* params)
    : restricted_by_carrier_(false),
      enabled_by_user_(false),
      disabled_on_vpn_(false),
      prefs_(NULL),
      local_state_prefs_(NULL),
      url_request_context_getter_(NULL) {
  DCHECK(params);
  params_.reset(params);
}

DataReductionProxySettings::~DataReductionProxySettings() {
  if (params_->allowed())
    spdy_proxy_auth_enabled_.Destroy();
}

void DataReductionProxySettings::InitPrefMembers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(&DataReductionProxySettings::OnProxyEnabledPrefChange,
                 base::Unretained(this)));
  data_reduction_proxy_alternative_enabled_.Init(
      prefs::kDataReductionProxyAltEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(
          &DataReductionProxySettings::OnProxyAlternativeEnabledPrefChange,
          base::Unretained(this)));
}

void DataReductionProxySettings::InitDataReductionProxySettings(
    PrefService* prefs,
    PrefService* local_state_prefs,
    net::URLRequestContextGetter* url_request_context_getter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs);
  DCHECK(local_state_prefs);
  DCHECK(url_request_context_getter);
  prefs_ = prefs;
  local_state_prefs_ = local_state_prefs;
  url_request_context_getter_ = url_request_context_getter;
  InitPrefMembers();
  RecordDataReductionInit();

  // Disable the proxy if it is not allowed to be used.
  if (!params_->allowed())
    return;

  AddDefaultProxyBypassRules();
  net::NetworkChangeNotifier::AddIPAddressObserver(this);

  // We set or reset the proxy pref at startup.
  MaybeActivateDataReductionProxy(true);
}

void DataReductionProxySettings::InitDataReductionProxySettings(
    PrefService* prefs,
    PrefService* local_state_prefs,
    net::URLRequestContextGetter* url_request_context_getter,
    scoped_ptr<DataReductionProxyConfigurator> configurator) {
  InitDataReductionProxySettings(prefs,
                                 local_state_prefs,
                                 url_request_context_getter);
  SetProxyConfigurator(configurator.Pass());
}

void DataReductionProxySettings::SetOnDataReductionEnabledCallback(
    const base::Callback<void(bool)>& on_data_reduction_proxy_enabled) {
  on_data_reduction_proxy_enabled_ = on_data_reduction_proxy_enabled;
  on_data_reduction_proxy_enabled_.Run(IsDataReductionProxyEnabled());
}

void DataReductionProxySettings::SetProxyConfigurator(
    scoped_ptr<DataReductionProxyConfigurator> configurator) {
  DCHECK(configurator);
  configurator_ = configurator.Pass();
}

bool DataReductionProxySettings::IsDataReductionProxyEnabled() {
  return spdy_proxy_auth_enabled_.GetValue() || IsEnabledOnCommandLine();
}

bool
DataReductionProxySettings::IsDataReductionProxyAlternativeEnabled() const {
  return data_reduction_proxy_alternative_enabled_.GetValue();
}

bool DataReductionProxySettings::IsDataReductionProxyManaged() {
  return spdy_proxy_auth_enabled_.IsManaged();
}

void DataReductionProxySettings::SetDataReductionProxyEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!params_->allowed())
    return;

  if (spdy_proxy_auth_enabled_.GetValue() != enabled) {
    spdy_proxy_auth_enabled_.SetValue(enabled);
    OnProxyEnabledPrefChange();
  }
}

void DataReductionProxySettings::SetDataReductionProxyAlternativeEnabled(
    bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!params_->alternative_allowed())
    return;
  if (data_reduction_proxy_alternative_enabled_.GetValue() != enabled) {
    data_reduction_proxy_alternative_enabled_.SetValue(enabled);
    OnProxyAlternativeEnabledPrefChange();
  }
}

int64 DataReductionProxySettings::GetDataReductionLastUpdateTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* local_state = GetLocalStatePrefs();
  int64 last_update_internal =
      local_state->GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
  base::Time last_update = base::Time::FromInternalValue(last_update_internal);
  return static_cast<int64>(last_update.ToJsTime());
}

DataReductionProxySettings::ContentLengthList
DataReductionProxySettings::GetDailyOriginalContentLengths() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetDailyContentLengths(prefs::kDailyHttpOriginalContentLength);
}

bool DataReductionProxySettings::IsDataReductionProxyUnreachable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return usage_stats_ && usage_stats_->isDataReductionProxyUnreachable();
}

void DataReductionProxySettings::SetDataReductionProxyUsageStats(
    DataReductionProxyUsageStats* usage_stats) {
  usage_stats_ = usage_stats;
}

DataReductionProxySettings::ContentLengthList
DataReductionProxySettings::GetDailyReceivedContentLengths() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetDailyContentLengths(prefs::kDailyHttpReceivedContentLength);
}

void DataReductionProxySettings::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The purpose of sending a request for the warmup URL is to warm the
  // connection to the data_reduction_proxy. The result is ignored.
  if (source == warmup_fetcher_.get())
    return;

  DCHECK(source == fetcher_.get());
  net::URLRequestStatus status = source->GetStatus();
  if (status.status() == net::URLRequestStatus::FAILED) {
    if (status.error() == net::ERR_INTERNET_DISCONNECTED) {
      RecordProbeURLFetchResult(INTERNET_DISCONNECTED);
      return;
    }
    // TODO(bengr): Remove once we understand the reasons probes are failing.
    // Probe errors are either due to fetcher-level errors or modified
    // responses. This only tracks the former.
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        kUMAProxyProbeURLNetError, std::abs(status.error()));
  }

  std::string response;
  source->GetResponseAsString(&response);

  if ("OK" == response.substr(0, 2)) {
    DVLOG(1) << "The data reduction proxy is unrestricted.";

    if (enabled_by_user_) {
      if (restricted_by_carrier_) {
        // The user enabled the proxy, but sometime previously in the session,
        // the network operator had blocked the canary and restricted the user.
        // The current network doesn't block the canary, so don't restrict the
        // proxy configurations.
        SetProxyConfigs(true /* enabled */,
                        IsDataReductionProxyAlternativeEnabled(),
                        false /* restricted */,
                        false /* at_startup */);
        RecordProbeURLFetchResult(SUCCEEDED_PROXY_ENABLED);
      } else {
        RecordProbeURLFetchResult(SUCCEEDED_PROXY_ALREADY_ENABLED);
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
      SetProxyConfigs(true /* enabled */,
                      IsDataReductionProxyAlternativeEnabled(),
                      true /* restricted */,
                      false /* at_startup */);
      RecordProbeURLFetchResult(FAILED_PROXY_DISABLED);
    } else {
      RecordProbeURLFetchResult(FAILED_PROXY_ALREADY_DISABLED);
    }
  }
  restricted_by_carrier_ = true;
}

PrefService* DataReductionProxySettings::GetOriginalProfilePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return prefs_;
}

PrefService* DataReductionProxySettings::GetLocalStatePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return local_state_prefs_;
}

void DataReductionProxySettings::AddDefaultProxyBypassRules() {
  // localhost
  DCHECK(configurator_);
  configurator_->AddHostPatternToBypass("<local>");
  // RFC1918 private addresses.
  configurator_->AddHostPatternToBypass("10.0.0.0/8");
  configurator_->AddHostPatternToBypass("172.16.0.0/12");
  configurator_->AddHostPatternToBypass("192.168.0.0/16");
  // RFC4193 private addresses.
  configurator_->AddHostPatternToBypass("fc00::/7");
  // IPV6 probe addresses.
  configurator_->AddHostPatternToBypass("*-ds.metric.gstatic.com");
  configurator_->AddHostPatternToBypass("*-v4.metric.gstatic.com");
}

void DataReductionProxySettings::LogProxyState(
    bool enabled, bool restricted, bool at_startup) {
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

  LOG(WARNING) << "SPDY proxy " << (enabled ? annotated_on : kOff)
               << " " << (at_startup ? kAtStartup : kByUser);
}

void DataReductionProxySettings::OnIPAddressChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (enabled_by_user_) {
    DCHECK(params_->allowed());
    RecordNetworkChangeEvent(IP_CHANGED);
    if (DisableIfVPN())
      return;
    ProbeWhetherDataReductionProxyIsAvailable();
    WarmProxyConnection();
  }
}

void DataReductionProxySettings::OnProxyEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!on_data_reduction_proxy_enabled_.is_null())
    on_data_reduction_proxy_enabled_.Run(IsDataReductionProxyEnabled());
  if (!params_->allowed())
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettings::OnProxyAlternativeEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!params_->alternative_allowed())
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettings::ResetDataReductionStatistics() {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* prefs = GetLocalStatePrefs();
  if (!prefs)
    return;
  ListPrefUpdate original_update(prefs, prefs::kDailyHttpOriginalContentLength);
  ListPrefUpdate received_update(prefs, prefs::kDailyHttpReceivedContentLength);
  original_update->Clear();
  received_update->Clear();
  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    original_update->AppendString(base::Int64ToString(0));
    received_update->AppendString(base::Int64ToString(0));
  }
}

void DataReductionProxySettings::MaybeActivateDataReductionProxy(
    bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* prefs = GetOriginalProfilePrefs();
  // TODO(marq): Consider moving this so stats are wiped the first time the
  // proxy settings are actually (not maybe) turned on.
  if (spdy_proxy_auth_enabled_.GetValue() &&
      !prefs->GetBoolean(prefs::kDataReductionProxyWasEnabledBefore)) {
    prefs->SetBoolean(prefs::kDataReductionProxyWasEnabledBefore, true);
    ResetDataReductionStatistics();
  }
  // Configure use of the data reduction proxy if it is enabled.
  enabled_by_user_= IsDataReductionProxyEnabled();
  SetProxyConfigs(enabled_by_user_ && !disabled_on_vpn_,
                  IsDataReductionProxyAlternativeEnabled(),
                  restricted_by_carrier_,
                  at_startup);

  // Check if the proxy has been restricted explicitly by the carrier.
  if (enabled_by_user_ && !disabled_on_vpn_) {
    ProbeWhetherDataReductionProxyIsAvailable();
    WarmProxyConnection();
  }
}

void DataReductionProxySettings::SetProxyConfigs(bool enabled,
                                                 bool alternative_enabled,
                                                 bool restricted,
                                                 bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(configurator_);

  LogProxyState(enabled, restricted, at_startup);
  // The alternative is only configured if the standard configuration is
  // is enabled.
  if (enabled & !params_->holdback()) {
    if (alternative_enabled) {
      configurator_->Enable(restricted,
                            !params_->fallback_allowed(),
                            params_->alt_origin().spec(),
                            params_->alt_fallback_origin().spec(),
                            params_->ssl_origin().spec());
    } else {
      configurator_->Enable(restricted,
                            !params_->fallback_allowed(),
                            params_->origin().spec(),
                            params_->fallback_origin().spec(),
                            std::string());
    }
  } else {
    configurator_->Disable();
  }
}

// Metrics methods
void DataReductionProxySettings::RecordDataReductionInit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProxyStartupState state = PROXY_NOT_AVAILABLE;
  if (params_->allowed()) {
    if (IsDataReductionProxyEnabled())
      state = PROXY_ENABLED;
    else
      state = PROXY_DISABLED;
  }

  RecordStartupState(state);
}

void DataReductionProxySettings::RecordProbeURLFetchResult(
    ProbeURLFetchResult result) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyProbeURL,
                            result,
                            PROBE_URL_FETCH_RESULT_COUNT);
}

void DataReductionProxySettings::RecordStartupState(ProxyStartupState state) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyStartupStateHistogram,
                            state,
                            PROXY_STARTUP_STATE_COUNT);
}

void DataReductionProxySettings::GetNetworkList(
    net::NetworkInterfaceList* interfaces,
    int policy) {
  net::GetNetworkList(interfaces, policy);
}

void DataReductionProxySettings::ResetParamsForTest(
    DataReductionProxyParams* params) {
  params_.reset(params);
}

DataReductionProxySettings::ContentLengthList
DataReductionProxySettings::GetDailyContentLengths(const char* pref_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DataReductionProxySettings::ContentLengthList content_lengths;
  const base::ListValue* list_value = GetLocalStatePrefs()->GetList(pref_name);
  if (list_value->GetSize() == kNumDaysInHistory) {
    for (size_t i = 0; i < kNumDaysInHistory; ++i) {
      content_lengths.push_back(GetInt64PrefValue(*list_value, i));
    }
  }
  return content_lengths;
}

void DataReductionProxySettings::GetContentLengths(
    unsigned int days,
    int64* original_content_length,
    int64* received_content_length,
    int64* last_update_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(days, kNumDaysInHistory);
  PrefService* local_state = GetLocalStatePrefs();
  if (!local_state) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  const base::ListValue* original_list =
      local_state->GetList(prefs::kDailyHttpOriginalContentLength);
  const base::ListValue* received_list =
      local_state->GetList(prefs::kDailyHttpReceivedContentLength);

  if (original_list->GetSize() != kNumDaysInHistory ||
      received_list->GetSize() != kNumDaysInHistory) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  int64 orig = 0L;
  int64 recv = 0L;
  // Include days from the end of the list going backwards.
  for (size_t i = kNumDaysInHistory - days;
       i < kNumDaysInHistory; ++i) {
    orig += GetInt64PrefValue(*original_list, i);
    recv += GetInt64PrefValue(*received_list, i);
  }
  *original_content_length = orig;
  *received_content_length = recv;
  *last_update_time =
      local_state->GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
}

net::URLFetcher* DataReductionProxySettings::GetBaseURLFetcher(
    const GURL& gurl,
    int load_flags) {

  net::URLFetcher* fetcher = net::URLFetcher::Create(gurl,
                                                     net::URLFetcher::GET,
                                                     this);
  fetcher->SetLoadFlags(load_flags);
  DCHECK(url_request_context_getter_);
  fetcher->SetRequestContext(url_request_context_getter_);
  // Configure max retries to be at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher->SetMaxRetriesOn5xx(kMaxRetries);
  fetcher->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
  return fetcher;
}


net::URLFetcher*
DataReductionProxySettings::GetURLFetcherForAvailabilityCheck() {
  return GetBaseURLFetcher(params_->probe_url(),
                           net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_PROXY);
}


void DataReductionProxySettings::ProbeWhetherDataReductionProxyIsAvailable() {
  net::URLFetcher* fetcher = GetURLFetcherForAvailabilityCheck();
  if (!fetcher)
    return;
  fetcher_.reset(fetcher);
  fetcher_->Start();
}

net::URLFetcher* DataReductionProxySettings::GetURLFetcherForWarmup() {
  return GetBaseURLFetcher(params_->warmup_url(), net::LOAD_DISABLE_CACHE);
}

void DataReductionProxySettings::WarmProxyConnection() {
  net::URLFetcher* fetcher = GetURLFetcherForWarmup();
  if (!fetcher)
    return;
  warmup_fetcher_.reset(fetcher);
  warmup_fetcher_->Start();
}

bool DataReductionProxySettings::DisableIfVPN() {
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
      SetProxyConfigs(false,
                      IsDataReductionProxyAlternativeEnabled(),
                      false,
                      false);
      disabled_on_vpn_ = true;
      RecordNetworkChangeEvent(DISABLED_ON_VPN);
      return true;
    }
  }
  if (disabled_on_vpn_) {
    SetProxyConfigs(enabled_by_user_,
                    IsDataReductionProxyAlternativeEnabled(),
                    restricted_by_carrier_,
                    false);
  }
  disabled_on_vpn_ = false;
  return false;
}

}  // namespace data_reduction_proxy
