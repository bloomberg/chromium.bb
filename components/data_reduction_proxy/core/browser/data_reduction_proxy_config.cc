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
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
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

// Key of the UMA DataReductionProxy.SecureProxyCheck.Latency histogram.
const char kUMAProxySecureProxyCheckLatency[] =
    "DataReductionProxy.SecureProxyCheck.Latency";

// Record a network change event.
void RecordNetworkChangeEvent(DataReductionProxyNetworkChangeEvent event) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.NetworkChangeEvents", event,
                            CHANGE_EVENT_COUNT);
}

// Returns a descriptive name corresponding to |connection_type|.
const char* GetNameForConnectionType(
    net::NetworkChangeNotifier::ConnectionType connection_type) {
  switch (connection_type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return "Unknown";
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return "Ethernet";
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return "WiFi";
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return "2G";
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return "3G";
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return "4G";
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return "None";
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return "Bluetooth";
  }
  NOTREACHED();
  return "";
}

// Returns an enumerated histogram that should be used to record the given
// statistic. |max_limit| is the maximum value that can be stored in the
// histogram. Number of buckets in the enumerated histogram are one more than
// |max_limit|.
base::HistogramBase* GetEnumeratedHistogram(
    base::StringPiece prefix,
    net::NetworkChangeNotifier::ConnectionType type,
    int32_t max_limit) {
  DCHECK_GT(max_limit, 0);

  base::StringPiece name_for_connection_type(GetNameForConnectionType(type));
  std::string histogram_name;
  histogram_name.reserve(prefix.size() + name_for_connection_type.size());
  histogram_name.append(prefix.data(), prefix.size());
  histogram_name.append(name_for_connection_type.data(),
                        name_for_connection_type.size());

  return base::Histogram::FactoryGet(
      histogram_name, 0, max_limit, max_limit + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

// Following UMA is plotted to measure how frequently Lo-Fi state changes.
// Too frequent changes are undesirable. |connection_type| is the current
// connection type.
void RecordAutoLoFiRequestHeaderStateChange(
    net::NetworkChangeNotifier::ConnectionType connection_type,
    bool previous_header_low,
    bool current_header_low) {
  // Auto Lo-Fi request header state changes.
  // Possible Lo-Fi header directives are empty ("") and low ("q=low").
  // This enum must remain synchronized with the enum of the same name in
  // metrics/histograms/histograms.xml.
  enum AutoLoFiRequestHeaderState {
    AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_EMPTY = 0,
    AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_LOW = 1,
    AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_EMPTY = 2,
    AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_LOW = 3,
    AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY
  };

  AutoLoFiRequestHeaderState state;

  if (!previous_header_low) {
    if (current_header_low)
      state = AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_LOW;
    else
      state = AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_EMPTY;
  } else {
    if (current_header_low) {
      // Low to low in useful in checking how many consecutive page loads
      // are done with Lo-Fi enabled.
      state = AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_LOW;
    } else {
      state = AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_EMPTY;
    }
  }

  switch (connection_type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Unknown", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Ethernet", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.WiFi", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.2G", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.3G", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_4G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.4G", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.None", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Bluetooth", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    default:
      NOTREACHED();
      break;
  }
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

// Checks if the secure proxy is allowed by the carrier by sending a probe.
class SecureProxyChecker : public net::URLFetcherDelegate {
 public:
  SecureProxyChecker(const scoped_refptr<net::URLRequestContextGetter>&
                         url_request_context_getter)
      : url_request_context_getter_(url_request_context_getter) {}

  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK_EQ(source, fetcher_.get());
    net::URLRequestStatus status = source->GetStatus();

    std::string response;
    source->GetResponseAsString(&response);

    base::TimeDelta secure_proxy_check_latency =
        base::Time::Now() - secure_proxy_check_start_time_;
    if (secure_proxy_check_latency >= base::TimeDelta()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(kUMAProxySecureProxyCheckLatency,
                                 secure_proxy_check_latency);
    }

    fetcher_callback_.Run(response, status, source->GetResponseCode());
  }

  void CheckIfSecureProxyIsAllowed(const GURL& secure_proxy_check_url,
                                   FetcherResponseCallback fetcher_callback) {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation(
            "data_reduction_proxy_secure_proxy_check", R"(
            semantics {
              sender: "Data Reduction Proxy"
              description:
                "Sends a request to the Data Reduction Proxy server. Proceeds "
                "with using a secure connection to the proxy only if the "
                "response is not blocked or modified by an intermediary."
              trigger:
                "A request can be sent whenever the browser is determining how "
                "to configure its connection to the data reduction proxy. This "
                "happens on startup and network changes."
              data: "A specific URL, not related to user data."
              destination: GOOGLE_OWNED_SERVICE
            }
            policy {
              cookies_allowed: false
              setting:
                "Users can control Data Saver on Android via the 'Data Saver' "
                "setting. Data Saver is not available on iOS, and on desktop "
                "it is enabled by installing the Data Saver extension."
              policy_exception_justification: "Not implemented."
            })");
    fetcher_ = net::URLFetcher::Create(
        secure_proxy_check_url, net::URLFetcher::GET, this, traffic_annotation);
    data_use_measurement::DataUseUserData::AttachToFetcher(
        fetcher_.get(),
        data_use_measurement::DataUseUserData::DATA_REDUCTION_PROXY);
    fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_PROXY |
                           net::LOAD_DO_NOT_SEND_COOKIES |
                           net::LOAD_DO_NOT_SAVE_COOKIES);
    fetcher_->SetRequestContext(url_request_context_getter_.get());
    // Configure max retries to be at most kMaxRetries times for 5xx errors.
    static const int kMaxRetries = 5;
    fetcher_->SetMaxRetriesOn5xx(kMaxRetries);
    fetcher_->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
    // The secure proxy check should not be redirected. Since the secure proxy
    // check will inevitably fail if it gets redirected somewhere else (e.g. by
    // a captive portal), short circuit that by giving up on the secure proxy
    // check if it gets redirected.
    fetcher_->SetStopOnRedirect(true);

    fetcher_callback_ = fetcher_callback;

    secure_proxy_check_start_time_ = base::Time::Now();
    fetcher_->Start();
  }

  ~SecureProxyChecker() override {}

 private:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // The URLFetcher being used for the secure proxy check.
  std::unique_ptr<net::URLFetcher> fetcher_;
  FetcherResponseCallback fetcher_callback_;

  // Used to determine the latency in performing the Data Reduction Proxy secure
  // proxy check.
  base::Time secure_proxy_check_start_time_;

  DISALLOW_COPY_AND_ASSIGN(SecureProxyChecker);
};

// URLFetcherDelegate for fetching the warmup URL.
class WarmupURLFetcher : public net::URLFetcherDelegate {
 public:
  explicit WarmupURLFetcher(const scoped_refptr<net::URLRequestContextGetter>&
                                url_request_context_getter)
      : url_request_context_getter_(url_request_context_getter) {
    DCHECK(url_request_context_getter_);
  }

  ~WarmupURLFetcher() override {}

  // Creates and starts a URLFetcher that fetches the warmup URL.
  void FetchWarmupURL() {
    UMA_HISTOGRAM_EXACT_LINEAR("DataReductionProxy.WarmupURL.FetchInitiated", 1,
                               2);
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("data_reduction_proxy_warmup", R"(
          semantics {
            sender: "Data Reduction Proxy"
            description:
              "Sends a request to the Data Reduction Proxy server to warm up "
              "the connection to the proxy."
            trigger:
              "A network change while the data reduction proxy is enabled will "
              "trigger this request."
            data: "A specific URL, not related to user data."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: false
            setting:
              "Users can control Data Saver on Android via the 'Data Saver' "
              "setting. Data Saver is not available on iOS, and on desktop it "
              "is enabled by installing the Data Saver extension."
            policy_exception_justification: "Not implemented."
          })");
    fetcher_ = net::URLFetcher::Create(
        params::GetWarmupURL(), net::URLFetcher::GET, this, traffic_annotation);
    data_use_measurement::DataUseUserData::AttachToFetcher(
        fetcher_.get(),
        data_use_measurement::DataUseUserData::DATA_REDUCTION_PROXY);
    // Do not disable cookies. This allows the warmup connection to be reused
    // for fetching user initiated requests.
    fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE);
    fetcher_->SetRequestContext(url_request_context_getter_.get());
    // |fetcher| should not retry on 5xx errors. |fetcher_| should retry on
    // network changes since the network stack may receive the connection change
    // event later than |this|.
    static const int kMaxRetries = 5;
    fetcher_->SetAutomaticallyRetryOn5xx(false);
    fetcher_->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
    fetcher_->Start();
  }

 private:
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK_EQ(source, fetcher_.get());
    UMA_HISTOGRAM_BOOLEAN(
        "DataReductionProxy.WarmupURL.FetchSuccessful",
        source->GetStatus().status() == net::URLRequestStatus::SUCCESS);
  }

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // The URLFetcher being used for fetching the warmup URL.
  std::unique_ptr<net::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(WarmupURLFetcher);
};

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
      lofi_effective_connection_type_threshold_(
          net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      auto_lofi_hysteresis_(base::TimeDelta::Max()),
      network_prohibitively_slow_(false),
      connection_type_(net::NetworkChangeNotifier::GetConnectionType()),
      connection_type_changed_(false),
      lofi_off_(false),
      network_quality_at_last_query_(NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN),
      previous_state_lofi_on_(false),
      is_captive_portal_(false),
      weak_factory_(this) {
  DCHECK(io_task_runner_);
  DCHECK(configurator);
  DCHECK(event_creator);

  if (params::IsLoFiDisabledViaFlags())
    SetLoFiModeOff();
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfig::~DataReductionProxyConfig() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
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

  PopulateAutoLoFiParams();
  AddDefaultProxyBypassRules();
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);

  // Record accuracy at 3 different intervals. The values used here must remain
  // in sync with the suffixes specified in
  // tools/metrics/histograms/histograms.xml.
  lofi_accuracy_recording_intervals_.push_back(
      base::TimeDelta::FromSeconds(15));
  lofi_accuracy_recording_intervals_.push_back(
      base::TimeDelta::FromSeconds(30));
  lofi_accuracy_recording_intervals_.push_back(
      base::TimeDelta::FromSeconds(60));
}

void DataReductionProxyConfig::ReloadConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(configurator_);

  if (enabled_by_user_ && !config_values_->holdback() &&
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

bool DataReductionProxyConfig::IsNetworkQualityProhibitivelySlow(
    const net::NetworkQualityEstimator* network_quality_estimator) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(params::IsIncludedInLoFiEnabledFieldTrial() ||
         params::IsIncludedInLoFiControlFieldTrial() ||
         params::IsLoFiSlowConnectionsOnlyViaFlags());

  if (!network_quality_estimator)
    return false;

  const net::EffectiveConnectionType effective_connection_type =
      network_quality_estimator->GetEffectiveConnectionType();

  const bool is_network_quality_available =
      effective_connection_type != net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  // True only if the network is currently estimated to be slower than the
  // defined thresholds.
  const bool is_network_currently_slow =
      is_network_quality_available &&
      IsEffectiveConnectionTypeSlowerThanThreshold(effective_connection_type);

  if (is_network_quality_available) {
    network_quality_at_last_query_ =
        is_network_currently_slow ? NETWORK_QUALITY_AT_LAST_QUERY_SLOW
                                  : NETWORK_QUALITY_AT_LAST_QUERY_NOT_SLOW;

    if ((params::IsIncludedInLoFiEnabledFieldTrial() ||
         params::IsIncludedInLoFiControlFieldTrial()) &&
        !params::IsLoFiSlowConnectionsOnlyViaFlags()) {
      // Post tasks to record accuracy of network quality prediction at
      // different intervals.
      for (const base::TimeDelta& measuring_delay :
           GetLofiAccuracyRecordingIntervals()) {
        io_task_runner_->PostDelayedTask(
            FROM_HERE,
            base::Bind(&DataReductionProxyConfig::RecordAutoLoFiAccuracyRate,
                       weak_factory_.GetWeakPtr(), network_quality_estimator,
                       measuring_delay),
            measuring_delay);
      }
    }
  }

  // Return the cached entry if the last update was within the hysteresis
  // duration and if the connection type has not changed.
  if (!connection_type_changed_ && !network_quality_last_checked_.is_null() &&
      GetTicksNow() - network_quality_last_checked_ <= auto_lofi_hysteresis_) {
    return network_prohibitively_slow_;
  }

  network_quality_last_checked_ = GetTicksNow();
  connection_type_changed_ = false;

  if (!is_network_quality_available)
    return false;

  network_prohibitively_slow_ = is_network_currently_slow;
  return network_prohibitively_slow_;
}

void DataReductionProxyConfig::PopulateAutoLoFiParams() {
  std::string field_trial = params::GetLoFiFieldTrialName();

    // Default parameters to use.
  const net::EffectiveConnectionType
      default_effective_connection_type_threshold =
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
  const base::TimeDelta default_hysterisis = base::TimeDelta::FromSeconds(60);

  if (params::IsLoFiSlowConnectionsOnlyViaFlags()) {
    // Use the default parameters.
    lofi_effective_connection_type_threshold_ =
        default_effective_connection_type_threshold;
    auto_lofi_hysteresis_ = default_hysterisis;
    field_trial = params::GetLoFiFlagFieldTrialName();
  }

  if (!params::IsIncludedInLoFiControlFieldTrial() &&
      !params::IsIncludedInLoFiEnabledFieldTrial() &&
      !params::IsLoFiSlowConnectionsOnlyViaFlags()) {
    return;
  }

  std::string variation_value = variations::GetVariationParamValue(
      field_trial, "effective_connection_type");
  if (!variation_value.empty()) {
    bool effective_connection_type_available =
        net::GetEffectiveConnectionTypeForName(
            variation_value, &lofi_effective_connection_type_threshold_);
    DCHECK(effective_connection_type_available);

    // Silence unused variable warning in release builds.
    (void)effective_connection_type_available;
  } else {
    // Use the default parameters.
    lofi_effective_connection_type_threshold_ =
        default_effective_connection_type_threshold;
  }

  uint32_t auto_lofi_hysteresis_period_seconds;
  variation_value = variations::GetVariationParamValue(
      field_trial, "hysteresis_period_seconds");
  if (!variation_value.empty() &&
      base::StringToUint(variation_value,
                         &auto_lofi_hysteresis_period_seconds)) {
    auto_lofi_hysteresis_ =
        base::TimeDelta::FromSeconds(auto_lofi_hysteresis_period_seconds);
  } else {
    // Use the default parameters.
    auto_lofi_hysteresis_ = default_hysterisis;
  }
  DCHECK_GE(auto_lofi_hysteresis_, base::TimeDelta());
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

// Returns true if the Data Reduction Proxy promo may be shown. This is not
// tied to whether the Data Reduction Proxy is enabled.
bool DataReductionProxyConfig::promo_allowed() const {
  return config_values_->promo_allowed();
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
        config_values_->secure_proxy_check_url(),
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

void DataReductionProxyConfig::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  connection_type_changed_ = true;
  connection_type_ = type;
  FetchWarmupURL();
}

void DataReductionProxyConfig::OnIPAddressChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (enabled_by_user_) {
    RecordNetworkChangeEvent(IP_CHANGED);

    // Reset |network_quality_at_last_query_| to prevent recording of network
    // quality prediction accuracy if there was a change in the IP address.
    network_quality_at_last_query_ = NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN;

    HandleCaptivePortal();
    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives
    // |secure_proxy_checker_|.
    SecureProxyCheck(
        config_values_->secure_proxy_check_url(),
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
    const GURL& secure_proxy_check_url,
    FetcherResponseCallback fetcher_callback) {
  net_log_with_source_ = net::NetLogWithSource::Make(
      net_log_, net::NetLogSourceType::DATA_REDUCTION_PROXY);
  if (event_creator_) {
    event_creator_->BeginSecureProxyCheck(
        net_log_with_source_, config_values_->secure_proxy_check_url());
  }

  secure_proxy_checker_->CheckIfSecureProxyIsAllowed(secure_proxy_check_url,
                                                     fetcher_callback);
}

void DataReductionProxyConfig::FetchWarmupURL() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!enabled_by_user_ || !params::FetchWarmupURLEnabled())
    return;

  if (connection_type_ == net::NetworkChangeNotifier::CONNECTION_NONE)
    return;

  warmup_url_fetcher_->FetchWarmupURL();
}

void DataReductionProxyConfig::SetLoFiModeOff() {
  DCHECK(thread_checker_.CalledOnValidThread());
  lofi_off_ = true;
}

void DataReductionProxyConfig::RecordAutoLoFiAccuracyRate(
    const net::NetworkQualityEstimator* network_quality_estimator,
    const base::TimeDelta& measuring_duration) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_quality_estimator);
  DCHECK((params::IsIncludedInLoFiEnabledFieldTrial() ||
          params::IsIncludedInLoFiControlFieldTrial()) &&
         !params::IsLoFiSlowConnectionsOnlyViaFlags());
  DCHECK_EQ(0, measuring_duration.InMilliseconds() % 1000);
  DCHECK(base::ContainsValue(GetLofiAccuracyRecordingIntervals(),
                             measuring_duration));

  if (network_quality_at_last_query_ == NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN)
    return;

  const base::TimeTicks now = GetTicksNow();

  // Return if the time since |last_query_| is less than |measuring_duration|.
  // This may happen if another main frame request started during last
  // |measuring_duration|.
  if (now - last_query_ < measuring_duration)
    return;

  // Return if the time since |last_query_| is off by a factor of 2.
  if (now - last_query_ > 2 * measuring_duration)
    return;

  const net::EffectiveConnectionType recent_effective_connection_type =
      network_quality_estimator->GetRecentEffectiveConnectionType(last_query_);
  if (recent_effective_connection_type ==
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    return;
  }

  // Values of Auto Lo-Fi accuracy.
  // This enum must remain synchronized with the enum of the same name in
  // metrics/histograms/histograms.xml.
  enum AutoLoFiAccuracy {
    AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_SLOW = 0,
    AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_NOT_SLOW = 1,
    AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_SLOW = 2,
    AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_NOT_SLOW = 3,
    AUTO_LOFI_ACCURACY_INDEX_BOUNDARY
  };

  const bool should_have_used_lofi =
      IsEffectiveConnectionTypeSlowerThanThreshold(
          recent_effective_connection_type);

  AutoLoFiAccuracy accuracy = AUTO_LOFI_ACCURACY_INDEX_BOUNDARY;

  if (should_have_used_lofi) {
    if (network_quality_at_last_query_ == NETWORK_QUALITY_AT_LAST_QUERY_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_SLOW;
    } else if (network_quality_at_last_query_ ==
               NETWORK_QUALITY_AT_LAST_QUERY_NOT_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_SLOW;
    } else {
      NOTREACHED();
    }
  } else {
    if (network_quality_at_last_query_ == NETWORK_QUALITY_AT_LAST_QUERY_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_NOT_SLOW;
    } else if (network_quality_at_last_query_ ==
               NETWORK_QUALITY_AT_LAST_QUERY_NOT_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_NOT_SLOW;
    } else {
      NOTREACHED();
    }
  }

  base::HistogramBase* accuracy_histogram = GetEnumeratedHistogram(
      base::StringPrintf("DataReductionProxy.LoFi.Accuracy.%d.",
                         static_cast<int>(measuring_duration.InSeconds())),
      connection_type_, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY - 1);

  accuracy_histogram->Add(accuracy);
}

bool DataReductionProxyConfig::IsEffectiveConnectionTypeSlowerThanThreshold(
    net::EffectiveConnectionType effective_connection_type) const {
  return effective_connection_type >= net::EFFECTIVE_CONNECTION_TYPE_OFFLINE &&
         effective_connection_type <= lofi_effective_connection_type_threshold_;
}

bool DataReductionProxyConfig::ShouldEnableLoFi(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((request.load_flags() & net::LOAD_MAIN_FRAME_DEPRECATED) != 0);
  DCHECK(!request.url().SchemeIsCryptographic());

  if (base::FeatureList::IsEnabled(
          features::kDataReductionProxyDecidesTransform)) {
    return ShouldAcceptServerLoFi(request, previews_decider);
  }

  bool enable_lofi = ShouldEnableLoFiInternal(request, previews_decider);

  if (params::IsLoFiSlowConnectionsOnlyViaFlags() ||
      params::IsIncludedInLoFiEnabledFieldTrial()) {
    RecordAutoLoFiRequestHeaderStateChange(
        connection_type_, previous_state_lofi_on_, enable_lofi);
    previous_state_lofi_on_ = enable_lofi;
  }

  return enable_lofi;
}

bool DataReductionProxyConfig::ShouldEnableLitePages(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((request.load_flags() & net::LOAD_MAIN_FRAME_DEPRECATED) != 0);
  DCHECK(!request.url().SchemeIsCryptographic());

  if (base::FeatureList::IsEnabled(
          features::kDataReductionProxyDecidesTransform)) {
    return ShouldAcceptLitePages(request, previews_decider);
  }

  return ShouldEnableLitePagesInternal(request, previews_decider);
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
  if (params::IsBlackListEnabledForServerPreviews()) {
    // Pass in net::EFFECTIVE_CONNECTION_TYPE_4G as the thresold as network
    // speed is checked in IsNetworkQualityProhibitivelySlow().
    // TODO(ryansturm): Use the correct ECT value (or add new method to
    // just check blacklist). crbug.com/720102
    return !previews_decider.ShouldAllowPreviewAtECT(
        request, previews_type, net::EFFECTIVE_CONNECTION_TYPE_4G);
  } else {
    // If Lo-Fi has been turned off, its status can't change. This Lo-Fi bit
    // will be removed when Lo-Fi and Lite Pages are moved over to using the
    // PreviewsBlackList.
    return lofi_off_;
  }
}

bool DataReductionProxyConfig::ShouldAcceptServerLoFi(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(base::FeatureList::IsEnabled(
      features::kDataReductionProxyDecidesTransform));

  if (IsBlackListedOrDisabled(request, previews_decider,
                              previews::PreviewsType::LOFI)) {
    return false;
  }

  if (params::IsLoFiAlwaysOnViaFlags()) {
    return true;
  }

  if (params::IsLoFiCellularOnlyViaFlags()) {
    return net::NetworkChangeNotifier::IsConnectionCellular(connection_type_);
  }

  if (params::IsLoFiSlowConnectionsOnlyViaFlags() ||
      params::IsIncludedInLoFiEnabledFieldTrial()) {
    // Accept Lo-Fi from the data reduction proxy (it will handle the effective
    // connection type check).
    return true;
  }

  return false;
}

bool DataReductionProxyConfig::ShouldAcceptLitePages(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(base::FeatureList::IsEnabled(
      features::kDataReductionProxyDecidesTransform));

  if (IsBlackListedOrDisabled(request, previews_decider,
                              previews::PreviewsType::LITE_PAGE)) {
    return false;
  }

  if (params::IsLoFiAlwaysOnViaFlags() &&
      params::AreLitePagesEnabledViaFlags()) {
    return true;
  }

  if (params::IsLoFiCellularOnlyViaFlags() &&
      params::AreLitePagesEnabledViaFlags()) {
    return net::NetworkChangeNotifier::IsConnectionCellular(connection_type_);
  }

  if ((params::IsLoFiSlowConnectionsOnlyViaFlags() &&
       params::AreLitePagesEnabledViaFlags()) ||
      params::IsIncludedInLitePageFieldTrial()) {
    // Accept LitePages from the data reduction proxy (it will handle the
    // effective connection type check).
    return true;
  }

  return false;
}

bool DataReductionProxyConfig::ShouldEnableLoFiInternal(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!base::FeatureList::IsEnabled(
      features::kDataReductionProxyDecidesTransform));

  last_query_ = GetTicksNow();
  network_quality_at_last_query_ = NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN;

  // LitePages overrides Server Lo-Fi. No fallback to Lo-Fi supported
  // on this code path (not using DataReductionProxyDecidesTransform feature).
  // TODO(dougarnett): Delete this surrounding method and related code once the
  // DataReductionProxyDecidesTransform feature is launched to stable [725645].
  if (ShouldEnableLitePages(request, previews_decider)) {
    return false;
  }

  if (IsBlackListedOrDisabled(request, previews_decider,
                              previews::PreviewsType::LOFI)) {
    return false;
  }

  if (params::IsLoFiAlwaysOnViaFlags())
    return true;

  if (params::IsLoFiCellularOnlyViaFlags()) {
    return net::NetworkChangeNotifier::IsConnectionCellular(connection_type_);
  }

  net::NetworkQualityEstimator* network_quality_estimator;
  network_quality_estimator =
      request.context() ? request.context()->network_quality_estimator()
                        : nullptr;

  if (params::IsLoFiSlowConnectionsOnlyViaFlags() ||
      params::IsIncludedInLoFiEnabledFieldTrial() ||
      params::IsIncludedInLoFiControlFieldTrial()) {
    return IsNetworkQualityProhibitivelySlow(network_quality_estimator);
  }

  return false;
}

bool DataReductionProxyConfig::ShouldEnableLitePagesInternal(
    const net::URLRequest& request,
    const previews::PreviewsDecider& previews_decider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!base::FeatureList::IsEnabled(
      features::kDataReductionProxyDecidesTransform));

  if (IsBlackListedOrDisabled(request, previews_decider,
                              previews::PreviewsType::LITE_PAGE)) {
    return false;
  }

  if (params::IsLoFiAlwaysOnViaFlags() && params::AreLitePagesEnabledViaFlags())
    return true;

  if (params::IsLoFiCellularOnlyViaFlags() &&
      params::AreLitePagesEnabledViaFlags()) {
    return net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType());
  }
  net::NetworkQualityEstimator* network_quality_estimator;
  network_quality_estimator =
      request.context() ? request.context()->network_quality_estimator()
                        : nullptr;

  if ((params::IsLoFiSlowConnectionsOnlyViaFlags() &&
       params::AreLitePagesEnabledViaFlags()) ||
      params::IsIncludedInLitePageFieldTrial() ||
      params::IsIncludedInLoFiControlFieldTrial()) {
    return IsNetworkQualityProhibitivelySlow(network_quality_estimator);
  }

  return false;
}

void DataReductionProxyConfig::GetNetworkList(
    net::NetworkInterfaceList* interfaces,
    int policy) {
  net::GetNetworkList(interfaces, policy);
}

const std::vector<base::TimeDelta>&
DataReductionProxyConfig::GetLofiAccuracyRecordingIntervals() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return lofi_accuracy_recording_intervals_;
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
