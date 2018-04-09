// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_source_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace data_reduction_proxy {

namespace {

// Key of the UMA DataReductionProxy.ConfigService.FetchResponseCode histogram.
const char kUMAConfigServiceFetchResponseCode[] =
    "DataReductionProxy.ConfigService.FetchResponseCode";

// Key of the UMA
// DataReductionProxy.ConfigService.FetchFailedAttemptsBeforeSuccess histogram.
const char kUMAConfigServiceFetchFailedAttemptsBeforeSuccess[] =
    "DataReductionProxy.ConfigService.FetchFailedAttemptsBeforeSuccess";

// Key of the UMA DataReductionProxy.ConfigService.FetchLatency histogram.
const char kUMAConfigServiceFetchLatency[] =
    "DataReductionProxy.ConfigService.FetchLatency";

// Key of the UMA DataReductionProxy.ConfigService.AuthExpired histogram.
const char kUMAConfigServiceAuthExpired[] =
    "DataReductionProxy.ConfigService.AuthExpired";

#if defined(OS_ANDROID)
// Maximum duration  to wait before fetching the config, while the application
// is in background.
const uint32_t kMaxBackgroundFetchIntervalSeconds = 6 * 60 * 60;  // 6 hours.
#endif

// This is the default backoff policy used to communicate with the Data
// Reduction Proxy configuration service.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
    0,                // num_errors_to_ignore
    30 * 1000,        // initial_delay_ms
    4,                // multiply_factor
    0.25,             // jitter_factor,
    128 * 60 * 1000,  // maximum_backoff_ms
    -1,               // entry_lifetime_ms
    true,             // always_use_initial_delay
};

// Extracts the list of Data Reduction Proxy servers to use for HTTP requests.
std::vector<DataReductionProxyServer> GetProxiesForHTTP(
    const data_reduction_proxy::ProxyConfig& proxy_config) {
  std::vector<DataReductionProxyServer> proxies;
  for (const auto& server : proxy_config.http_proxy_servers()) {
    if (server.scheme() != ProxyServer_ProxyScheme_UNSPECIFIED) {
      proxies.push_back(DataReductionProxyServer(
          net::ProxyServer(
              protobuf_parser::SchemeFromProxyScheme(server.scheme()),
              net::HostPortPair(server.host(), server.port()),
              /* HTTPS proxies are marked as trusted. */
              server.scheme() == ProxyServer_ProxyScheme_HTTPS),
          server.type()));
    }
  }

  return proxies;
}

void RecordAuthExpiredHistogram(bool auth_expired) {
  UMA_HISTOGRAM_BOOLEAN(kUMAConfigServiceAuthExpired, auth_expired);
}

// Records whether the session key used in the request matches the current
// sesssion key.
void RecordAuthExpiredSessionKey(bool matches) {
  // This enum must remain synchronized with the
  // DataReductionProxyConfigServiceAuthExpiredSessionKey enum in
  // metrics/histograms/histograms.xml.
  enum AuthExpiredSessionKey {
    AUTH_EXPIRED_SESSION_KEY_MISMATCH = 0,
    AUTH_EXPIRED_SESSION_KEY_MATCH = 1,
    AUTH_EXPIRED_SESSION_KEY_BOUNDARY = 2
  };

  AuthExpiredSessionKey state = matches ? AUTH_EXPIRED_SESSION_KEY_MATCH
                                        : AUTH_EXPIRED_SESSION_KEY_MISMATCH;

  UMA_HISTOGRAM_ENUMERATION(
      "DataReductionProxy.ConfigService.AuthExpiredSessionKey", state,
      AUTH_EXPIRED_SESSION_KEY_BOUNDARY);
}

}  // namespace

const net::BackoffEntry::Policy& GetBackoffPolicy() {
  return kDefaultBackoffPolicy;
}

DataReductionProxyConfigServiceClient::DataReductionProxyConfigServiceClient(
    std::unique_ptr<DataReductionProxyParams> params,
    const net::BackoffEntry::Policy& backoff_policy,
    DataReductionProxyRequestOptions* request_options,
    DataReductionProxyMutableConfigValues* config_values,
    DataReductionProxyConfig* config,
    DataReductionProxyEventCreator* event_creator,
    DataReductionProxyIOData* io_data,
    net::NetLog* net_log,
    ConfigStorer config_storer)
    : params_(std::move(params)),
      request_options_(request_options),
      config_values_(config_values),
      config_(config),
      event_creator_(event_creator),
      io_data_(io_data),
      net_log_(net_log),
      config_storer_(config_storer),
      backoff_entry_(&backoff_policy),
      config_service_url_(util::AddApiKeyToUrl(params::GetConfigServiceURL())),
      enabled_(false),
      remote_config_applied_(false),
      url_request_context_getter_(nullptr),
#if defined(OS_ANDROID)
      foreground_fetch_pending_(false),
#endif
      previous_request_failed_authentication_(false),
      failed_attempts_before_success_(0),
      fetch_in_progress_(false) {
  DCHECK(request_options);
  DCHECK(config_values);
  DCHECK(config);
  DCHECK(event_creator);
  DCHECK(io_data);
  DCHECK(net_log);
  DCHECK(config_service_url_.is_valid());
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfigServiceClient::
    ~DataReductionProxyConfigServiceClient() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

base::TimeDelta
DataReductionProxyConfigServiceClient::CalculateNextConfigRefreshTime(
    bool fetch_succeeded,
    const base::TimeDelta& config_expiration_delta,
    const base::TimeDelta& backoff_delay) {
  DCHECK(backoff_delay >= base::TimeDelta());

#if defined(OS_ANDROID)
  foreground_fetch_pending_ = false;
  if (!fetch_succeeded && IsApplicationStateBackground()) {
    // If Chromium is in background, then fetch the config when Chromium comes
    // to foreground or after max of |kMaxBackgroundFetchIntervalSeconds| and
    // |backoff_delay|.
    foreground_fetch_pending_ = true;
    return std::max(backoff_delay, base::TimeDelta::FromSeconds(
                                       kMaxBackgroundFetchIntervalSeconds));
  }
#endif

  if (fetch_succeeded) {
    return std::max(backoff_delay, config_expiration_delta);
  }

  return backoff_delay;
}

void DataReductionProxyConfigServiceClient::InitializeOnIOThread(
    net::URLRequestContextGetter* url_request_context_getter) {
  DCHECK(url_request_context_getter);
#if defined(OS_ANDROID)
  // It is okay to use Unretained here because |app_status_listener| would be
  // destroyed before |this|.
  app_status_listener_.reset(
      new base::android::ApplicationStatusListener(base::Bind(
          &DataReductionProxyConfigServiceClient::OnApplicationStateChange,
          base::Unretained(this))));
#endif
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  url_request_context_getter_ = url_request_context_getter;
}

void DataReductionProxyConfigServiceClient::SetEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_ = enabled;
}

void DataReductionProxyConfigServiceClient::RetrieveConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_)
    return;

  net_log_with_source_ = net::NetLogWithSource::Make(
      net_log_, net::NetLogSourceType::DATA_REDUCTION_PROXY);
  // Strip off query string parameters
  GURL::Replacements replacements;
  replacements.ClearQuery();
  GURL base_config_service_url =
      config_service_url_.ReplaceComponents(replacements);
  event_creator_->BeginConfigRequest(net_log_with_source_,
                                     base_config_service_url);
  config_fetch_start_time_ = base::TimeTicks::Now();

  RetrieveRemoteConfig();
}

bool DataReductionProxyConfigServiceClient::RemoteConfigApplied() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return remote_config_applied_;
}

void DataReductionProxyConfigServiceClient::ApplySerializedConfig(
    const std::string& config_value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (RemoteConfigApplied())
    return;

  std::string decoded_config;
  if (base::Base64Decode(config_value, &decoded_config)) {
    ClientConfig config;
    if (config.ParseFromString(decoded_config))
      ParseAndApplyProxyConfig(config);
  }
}

bool DataReductionProxyConfigServiceClient::ShouldRetryDueToAuthFailure(
    const net::HttpRequestHeaders& request_headers,
    const net::HttpResponseHeaders* response_headers,
    const net::ProxyServer& proxy_server,
    const net::LoadTimingInfo& load_timing_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(response_headers);
  if (config_->IsDataReductionProxy(proxy_server, nullptr)) {
    if (response_headers->response_code() ==
        net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
      std::string session_key =
          request_options_->GetSessionKeyFromRequestHeaders(request_headers);

      std::string current_session_key = request_options_->GetSecureSession();

      // If the session key used in the request is different from the current
      // session key, then the current session key does not need to be
      // invalidated.
      if (session_key != current_session_key) {
        RecordAuthExpiredSessionKey(false);
        return true;
      }
      RecordAuthExpiredSessionKey(true);

      // The default backoff logic is to increment the failure count (and
      // increase the backoff time) with each response failure to the remote
      // config service, and to decrement the failure count (and decrease the
      // backoff time) with each response success. In the case where the
      // config service returns a success response (decrementing the failure
      // count) but the session key is continually invalid (as a response from
      // the Data Reduction Proxy and not the config service), the previous
      // response should be considered a failure in order to ensure the backoff
      // time continues to increase.
      if (previous_request_failed_authentication_)
        GetBackoffEntry()->InformOfRequest(false);

      // Record that a request resulted in an authentication failure.
      RecordAuthExpiredHistogram(true);
      previous_request_failed_authentication_ = true;
      InvalidateConfig();
      DCHECK(!config_->IsDataReductionProxy(proxy_server, nullptr));

      if (fetch_in_progress_) {
        // If a client config fetch is already in progress, then do not start
        // another fetch since starting a new fetch will cause extra data
        // usage, and also cancel the ongoing fetch.
        return true;
      }

      RetrieveConfig();

      if (!load_timing_info.send_start.is_null() &&
          !load_timing_info.request_start.is_null() &&
          net::NetworkChangeNotifier::GetConnectionType() !=
              net::NetworkChangeNotifier::CONNECTION_NONE &&
          last_ip_address_change_ < load_timing_info.request_start) {
        // Record only if there was no change in the IP address since the
        // request started.
        UMA_HISTOGRAM_TIMES(
            "DataReductionProxy.ConfigService.AuthFailure.LatencyPenalty",
            base::TimeTicks::Now() - load_timing_info.request_start);
      }

      return true;
    }

    previous_request_failed_authentication_ = false;
  }

  return false;
}

net::BackoffEntry* DataReductionProxyConfigServiceClient::GetBackoffEntry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return &backoff_entry_;
}

void DataReductionProxyConfigServiceClient::SetConfigRefreshTimer(
    const base::TimeDelta& delay) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(delay >= base::TimeDelta());
  config_refresh_timer_.Stop();
  config_refresh_timer_.Start(
      FROM_HERE, delay, this,
      &DataReductionProxyConfigServiceClient::RetrieveConfig);
}

base::Time DataReductionProxyConfigServiceClient::Now() {
  return base::Time::Now();
}

void DataReductionProxyConfigServiceClient::OnIPAddressChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetBackoffEntry()->Reset();
  last_ip_address_change_ = base::TimeTicks::Now();
  failed_attempts_before_success_ = 0;
  RetrieveConfig();
}

void DataReductionProxyConfigServiceClient::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(source == fetcher_.get());
  fetch_in_progress_ = false;
  net::URLRequestStatus status = source->GetStatus();
  std::string response;
  source->GetResponseAsString(&response);
  HandleResponse(response, status, source->GetResponseCode());
}

void DataReductionProxyConfigServiceClient::RetrieveRemoteConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateClientConfigRequest request;
  std::string serialized_request;
#if defined(OS_ANDROID)
  request.set_telephony_network_operator(
      net::android::GetTelephonyNetworkOperator());
#endif

  data_reduction_proxy::ConfigDeviceInfo* device_info =
      request.mutable_device_info();
  device_info->set_total_device_memory_kb(
      base::SysInfo::AmountOfPhysicalMemory() / 1024);
  const std::string& session_key = request_options_->GetSecureSession();
  if (!session_key.empty())
    request.set_session_key(request_options_->GetSecureSession());
  request.set_dogfood_group(
      base::FeatureList::IsEnabled(features::kDogfood)
          ? CreateClientConfigRequest_DogfoodGroup_DOGFOOD
          : CreateClientConfigRequest_DogfoodGroup_NONDOGFOOD);
  data_reduction_proxy::VersionInfo* version_info =
      request.mutable_version_info();
  uint32_t build;
  uint32_t patch;
  util::GetChromiumBuildAndPatchAsInts(util::ChromiumVersion(), &build, &patch);
  version_info->set_client(util::GetStringForClient(io_data_->client()));
  version_info->set_build(build);
  version_info->set_patch(patch);
  version_info->set_channel(io_data_->channel());
  request.SerializeToString(&serialized_request);
  std::unique_ptr<net::URLFetcher> fetcher =
      GetURLFetcherForConfig(config_service_url_, serialized_request);
  if (!fetcher.get()) {
    HandleResponse(std::string(),
                   net::URLRequestStatus::FromError(net::ERR_ABORTED),
                   net::URLFetcher::RESPONSE_CODE_INVALID);
    return;
  }

  fetcher_ = std::move(fetcher);
  fetch_in_progress_ = true;

  // Attach variations headers.
  net::HttpRequestHeaders headers;
  variations::AppendVariationHeaders(config_service_url_,
                                     variations::InIncognito::kNo,
                                     variations::SignedIn::kNo, &headers);
  if (!headers.IsEmpty())
    fetcher_->SetExtraRequestHeaders(headers.ToString());
  UMA_HISTOGRAM_BOOLEAN("DataReductionProxy.ConfigService.SentVariationHeaders",
                        !headers.IsEmpty());
  fetcher_->Start();
}

void DataReductionProxyConfigServiceClient::InvalidateConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetBackoffEntry()->InformOfRequest(false);
  config_storer_.Run(std::string());
  request_options_->Invalidate();
  config_values_->Invalidate();
  io_data_->SetPingbackReportingFraction(0.0f);
  config_->OnNewClientConfigFetched();
}

std::unique_ptr<net::URLFetcher>
DataReductionProxyConfigServiceClient::GetURLFetcherForConfig(
    const GURL& secure_proxy_check_url,
    const std::string& request_body) {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("data_reduction_proxy_config", R"(
        semantics {
          sender: "Data Reduction Proxy"
          description:
            "Requests a configuration that specifies how to connect to the "
            "data reduction proxy."
          trigger:
            "Requested when Data Saver is enabled and the browser does not "
            "have a configuration that is not older than a threshold set by "
            "the server."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Data Saver on Android via 'Data Saver' setting. "
            "Data Saver is not available on iOS, and on desktop it is enabled "
            "by insalling the Data Saver extension."
          policy_exception_justification: "Not implemented."
        })");
  std::unique_ptr<net::URLFetcher> fetcher(net::URLFetcher::Create(
      secure_proxy_check_url, net::URLFetcher::POST, this, traffic_annotation));
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher.get(),
      data_use_measurement::DataUseUserData::DATA_REDUCTION_PROXY);
  fetcher->SetLoadFlags(net::LOAD_BYPASS_PROXY | net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher->SetUploadData("application/x-protobuf", request_body);
  DCHECK(url_request_context_getter_);
  fetcher->SetRequestContext(url_request_context_getter_);
  // |fetcher| should not retry on 5xx errors since the server may already be
  // overloaded. Spurious 5xx errors are still retried on exponential backoff.
  // |fetcher| should retry on network changes since the network stack may
  // receive the connection change event later than |this|.
  static const int kMaxRetries = 5;
  fetcher->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
  return fetcher;
}

void DataReductionProxyConfigServiceClient::HandleResponse(
    const std::string& config_data,
    const net::URLRequestStatus& status,
    int response_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ClientConfig config;
  bool succeeded = false;

  if (net::NetworkChangeNotifier::GetConnectionType() !=
      net::NetworkChangeNotifier::CONNECTION_NONE) {
    base::UmaHistogramSparse(kUMAConfigServiceFetchResponseCode, response_code);
  }

  if (status.status() == net::URLRequestStatus::SUCCESS &&
      response_code == net::HTTP_OK && config.ParseFromString(config_data)) {
    succeeded = ParseAndApplyProxyConfig(config);
  }

  // These are proxies listed in the config. The proxies that client eventually
  // ends up using depend on the field trials.
  std::vector<DataReductionProxyServer> proxies;
  base::TimeDelta refresh_duration;
  if (succeeded) {
    proxies = GetProxiesForHTTP(config.proxy_config());
    refresh_duration =
        protobuf_parser::DurationToTimeDelta(config.refresh_duration());

    DCHECK(!config_fetch_start_time_.is_null());
    base::TimeDelta configuration_fetch_latency =
        base::TimeTicks::Now() - config_fetch_start_time_;
    RecordAuthExpiredHistogram(false);
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMAConfigServiceFetchLatency,
                               configuration_fetch_latency);
    UMA_HISTOGRAM_COUNTS_100(kUMAConfigServiceFetchFailedAttemptsBeforeSuccess,
                             failed_attempts_before_success_);
    failed_attempts_before_success_ = 0;
    std::string encoded_config;
    base::Base64Encode(config_data, &encoded_config);
    config_storer_.Run(encoded_config);
  } else {
    ++failed_attempts_before_success_;
  }

  GetBackoffEntry()->InformOfRequest(succeeded);
  base::TimeDelta next_config_refresh_time = CalculateNextConfigRefreshTime(
      succeeded, refresh_duration, GetBackoffEntry()->GetTimeUntilRelease());

  SetConfigRefreshTimer(next_config_refresh_time);
  event_creator_->EndConfigRequest(
      net_log_with_source_, status.error(), response_code,
      GetBackoffEntry()->failure_count(),
      DataReductionProxyServer::ConvertToNetProxyServers(proxies),
      refresh_duration, next_config_refresh_time);
}

bool DataReductionProxyConfigServiceClient::ParseAndApplyProxyConfig(
    const ClientConfig& config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  float reporting_fraction = 0.0f;
  if (config.has_pageload_metrics_config() &&
      config.pageload_metrics_config().has_reporting_fraction()) {
    reporting_fraction = config.pageload_metrics_config().reporting_fraction();
  }
  DCHECK_LE(0.0f, reporting_fraction);
  DCHECK_GE(1.0f, reporting_fraction);
  io_data_->SetPingbackReportingFraction(reporting_fraction);

  if (!config.has_proxy_config())
    return false;

  std::vector<DataReductionProxyServer> proxies =
      GetProxiesForHTTP(config.proxy_config());

  if (proxies.empty())
    return false;

  request_options_->SetSecureSession(config.session_key());
  config_values_->UpdateValues(proxies);
  config_->OnNewClientConfigFetched();
  remote_config_applied_ = true;
  return true;
}

#if defined(OS_ANDROID)
bool DataReductionProxyConfigServiceClient::IsApplicationStateBackground()
    const {
  return base::android::ApplicationStatusListener::GetState() !=
         base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
}

void DataReductionProxyConfigServiceClient::OnApplicationStateChange(
    base::android::ApplicationState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (new_state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES &&
      foreground_fetch_pending_) {
    foreground_fetch_pending_ = false;
    RetrieveConfig();
  }
}
#endif

}  // namespace data_reduction_proxy
