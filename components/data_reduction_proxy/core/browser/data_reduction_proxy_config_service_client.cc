// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_server.h"

namespace data_reduction_proxy {

namespace {

// This is the default backoff policy used to communicate with the Data
// Reduction Proxy configuration service.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
    0,               // num_errors_to_ignore
    1 * 1000,        // initial_delay_ms
    4,               // multiply_factor
    0.10,            // jitter_factor,
    30 * 60 * 1000,  // maximum_backoff_ms
    -1,              // entry_lifetime_ms
    true,            // always_use_initial_delay
};

// Extracts the list of Data Reduction Proxy servers to use for HTTP requests.
std::vector<net::ProxyServer> GetProxiesForHTTP(
    const data_reduction_proxy::ProxyConfig& proxy_config) {
  std::vector<net::ProxyServer> proxies;
  for (const auto& server : proxy_config.http_proxy_servers()) {
    if (server.scheme() != ProxyServer_ProxyScheme_UNSPECIFIED) {
      proxies.push_back(net::ProxyServer(
          config_parser::SchemeFromProxyScheme(server.scheme()),
          net::HostPortPair(server.host(), server.port())));
    }
  }

  return proxies;
}

// Calculate the next time at which the Data Reduction Proxy configuration
// should be retrieved, based on response success, configuration expiration,
// and the backoff delay. |backoff_delay| must be non-negative. Note that it is
// possible for |config_expiration| to be prior to |now|, but on a successful
// config refresh, |backoff_delay| will be returned.
base::TimeDelta CalculateNextConfigRefreshTime(
    bool fetch_succeeded,
    const base::Time& config_expiration,
    const base::Time& now,
    const base::TimeDelta& backoff_delay) {
  DCHECK(backoff_delay >= base::TimeDelta());
  if (fetch_succeeded) {
    base::TimeDelta success_delay = config_expiration - now;
    if (success_delay > backoff_delay)
      return success_delay;
  }

  return backoff_delay;
}

}  // namespace

const net::BackoffEntry::Policy& GetBackoffPolicy() {
  return kDefaultBackoffPolicy;
}

DataReductionProxyConfigServiceClient::DataReductionProxyConfigServiceClient(
    scoped_ptr<DataReductionProxyParams> params,
    const net::BackoffEntry::Policy& backoff_policy,
    DataReductionProxyRequestOptions* request_options,
    DataReductionProxyMutableConfigValues* config_values,
    DataReductionProxyConfig* config)
    : params_(params.Pass()),
      request_options_(request_options),
      config_values_(config_values),
      config_(config),
      backoff_entry_(&backoff_policy) {
  DCHECK(request_options);
  DCHECK(config_values);
  DCHECK(config);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfigServiceClient::
    ~DataReductionProxyConfigServiceClient() {
}

void DataReductionProxyConfigServiceClient::RetrieveConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string static_response = ConstructStaticResponse();
  ClientConfig config;
  bool succeeded = false;
  if (config_parser::ParseClientConfig(static_response, &config)) {
    if (config.has_proxy_config()) {
      net::ProxyServer origin;
      net::ProxyServer fallback_origin;
      std::vector<net::ProxyServer> proxies =
          GetProxiesForHTTP(config.proxy_config());
      if (proxies.size() > 0) {
        origin = proxies[0];
        if (proxies.size() > 1)
          fallback_origin = proxies[1];

        std::string session;
        std::string credentials;
        if (DataReductionProxyRequestOptions::ParseLocalSessionKey(
                config.session_key(), &session, &credentials)) {
          request_options_->SetCredentials(session, credentials);
          config_values_->UpdateValues(origin, fallback_origin);
          config_->ReloadConfig();
          succeeded = true;
        }
      }
    }
  }

  base::Time expiration_time;
  if (succeeded) {
    expiration_time = config_parser::TimestampToTime(config.expire_time());
  }

  GetBackoffEntry()->InformOfRequest(succeeded);
  base::TimeDelta next_config_refresh_time =
      CalculateNextConfigRefreshTime(succeeded, expiration_time, Now(),
                                     GetBackoffEntry()->GetTimeUntilRelease());
  SetConfigRefreshTimer(next_config_refresh_time);
}

net::BackoffEntry* DataReductionProxyConfigServiceClient::GetBackoffEntry() {
  return &backoff_entry_;
}

void DataReductionProxyConfigServiceClient::SetConfigRefreshTimer(
    const base::TimeDelta& delay) {
  DCHECK(delay >= base::TimeDelta());
  config_refresh_timer_.Stop();
  config_refresh_timer_.Start(
      FROM_HERE, delay, this,
      &DataReductionProxyConfigServiceClient::RetrieveConfig);
}

base::Time DataReductionProxyConfigServiceClient::Now() {
  return base::Time::Now();
}

std::string
DataReductionProxyConfigServiceClient::ConstructStaticResponse() const {
  std::string response;
  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue());
  params_->PopulateConfigResponse(values.get());
  request_options_->PopulateConfigResponse(values.get());
  base::JSONWriter::Write(values.get(), &response);

  return response;
}

}  // namespace data_reduction_proxy
