// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/url_request/url_request_context_builder.h"

namespace cronet {

namespace {

// TODO(xunjieli): Refactor constants in io_thread.cc.
const char kQuicFieldTrialName[] = "QUIC";
const char kQuicConnectionOptions[] = "connection_options";
const char kQuicStoreServerConfigsInProperties[] =
    "store_server_configs_in_properties";
const char kQuicDelayTcpRace[] = "delay_tcp_race";
const char kQuicMaxNumberOfLossyConnections[] =
    "max_number_of_lossy_connections";
const char kQuicPacketLossThreshold[] = "packet_loss_threshold";

// AsyncDNS experiment dictionary name.
const char kAsyncDnsFieldTrialName[] = "AsyncDNS";
// Name of boolean to enable AsyncDNS experiment.
const char kAsyncDnsEnable[] = "enable";

void ParseAndSetExperimentalOptions(
    const std::string& experimental_options,
    net::URLRequestContextBuilder* context_builder,
    net::NetLog* net_log) {
  if (experimental_options.empty())
    return;

  DVLOG(1) << "Experimental Options:" << experimental_options;
  scoped_ptr<base::Value> options =
      base::JSONReader::Read(experimental_options);

  if (!options) {
    DCHECK(false) << "Parsing experimental options failed: "
                  << experimental_options;
    return;
  }

  scoped_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(options.Pass());

  if (!dict) {
    DCHECK(false) << "Experimental options string is not a dictionary: "
                  << experimental_options;
    return;
  }

  const base::DictionaryValue* quic_args = nullptr;
  if (dict->GetDictionary(kQuicFieldTrialName, &quic_args)) {
    std::string quic_connection_options;
    if (quic_args->GetString(kQuicConnectionOptions,
                             &quic_connection_options)) {
      context_builder->set_quic_connection_options(
          net::QuicUtils::ParseQuicConnectionOptions(quic_connection_options));
    }

    bool quic_store_server_configs_in_properties = false;
    if (quic_args->GetBoolean(kQuicStoreServerConfigsInProperties,
                              &quic_store_server_configs_in_properties)) {
      context_builder->set_quic_store_server_configs_in_properties(
          quic_store_server_configs_in_properties);
    }

    bool quic_delay_tcp_race = false;
    if (quic_args->GetBoolean(kQuicDelayTcpRace, &quic_delay_tcp_race)) {
      context_builder->set_quic_delay_tcp_race(quic_delay_tcp_race);
    }

    int quic_max_number_of_lossy_connections = 0;
    if (quic_args->GetInteger(kQuicMaxNumberOfLossyConnections,
                              &quic_max_number_of_lossy_connections)) {
      context_builder->set_quic_max_number_of_lossy_connections(
          quic_max_number_of_lossy_connections);
    }

    double quic_packet_loss_threshold = 0.0;
    if (quic_args->GetDouble(kQuicPacketLossThreshold,
                             &quic_packet_loss_threshold)) {
      context_builder->set_quic_packet_loss_threshold(
          quic_packet_loss_threshold);
    }
  }

  const base::DictionaryValue* async_dns_args = nullptr;
  if (dict->GetDictionary(kAsyncDnsFieldTrialName, &async_dns_args)) {
    bool async_dns_enable = false;
    if (async_dns_args->GetBoolean(kAsyncDnsEnable, &async_dns_enable) &&
        async_dns_enable) {
      if (net_log == nullptr) {
        DCHECK(false) << "AsyncDNS experiment requires NetLog.";
      } else {
        scoped_ptr<net::HostResolver> host_resolver(
            net::HostResolver::CreateDefaultResolver(net_log));
        host_resolver->SetDnsClientEnabled(true);
        context_builder->set_host_resolver(std::move(host_resolver));
      }
    }
  }
}

}  // namespace

URLRequestContextConfig::QuicHint::QuicHint(const std::string& host,
                                            int port,
                                            int alternate_port)
    : host(host), port(port), alternate_port(alternate_port) {}

URLRequestContextConfig::QuicHint::~QuicHint() {}

URLRequestContextConfig::Pkp::Pkp(const std::string& host,
                                  bool include_subdomains,
                                  const base::Time& expiration_date)
    : host(host),
      include_subdomains(include_subdomains),
      expiration_date(expiration_date) {}

URLRequestContextConfig::Pkp::~Pkp() {}

URLRequestContextConfig::URLRequestContextConfig(
    bool enable_quic,
    bool enable_spdy,
    bool enable_sdch,
    HttpCacheType http_cache,
    int http_cache_max_size,
    bool load_disable_cache,
    const std::string& storage_path,
    const std::string& user_agent,
    const std::string& experimental_options,
    const std::string& data_reduction_proxy_key,
    const std::string& data_reduction_primary_proxy,
    const std::string& data_reduction_fallback_proxy,
    const std::string& data_reduction_secure_proxy_check_url,
    scoped_ptr<net::CertVerifier> mock_cert_verifier)
    : enable_quic(enable_quic),
      enable_spdy(enable_spdy),
      enable_sdch(enable_sdch),
      http_cache(http_cache),
      http_cache_max_size(http_cache_max_size),
      load_disable_cache(load_disable_cache),
      storage_path(storage_path),
      user_agent(user_agent),
      experimental_options(experimental_options),
      data_reduction_proxy_key(data_reduction_proxy_key),
      data_reduction_primary_proxy(data_reduction_primary_proxy),
      data_reduction_fallback_proxy(data_reduction_fallback_proxy),
      data_reduction_secure_proxy_check_url(
          data_reduction_secure_proxy_check_url),
      mock_cert_verifier(std::move(mock_cert_verifier)) {}

URLRequestContextConfig::~URLRequestContextConfig() {}

void URLRequestContextConfig::ConfigureURLRequestContextBuilder(
    net::URLRequestContextBuilder* context_builder,
    net::NetLog* net_log) {
  std::string config_cache;
  if (http_cache != DISABLED) {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    if (http_cache == DISK && !storage_path.empty()) {
      cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
      cache_params.path = base::FilePath(storage_path);
    } else {
      cache_params.type =
          net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    }
    cache_params.max_size = http_cache_max_size;
    context_builder->EnableHttpCache(cache_params);
  } else {
    context_builder->DisableHttpCache();
  }
  context_builder->set_user_agent(user_agent);
  context_builder->SetSpdyAndQuicEnabled(enable_spdy, enable_quic);
  context_builder->set_sdch_enabled(enable_sdch);

  ParseAndSetExperimentalOptions(experimental_options, context_builder,
                                 net_log);

  if (mock_cert_verifier)
    context_builder->SetCertVerifier(mock_cert_verifier.Pass());
  // TODO(mef): Use |config| to set cookies.
}

}  // namespace cronet
