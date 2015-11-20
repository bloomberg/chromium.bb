// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/cert/cert_verifier.h"
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

// Using a reference to scoped_ptr is unavoidable because of the semantics of
// RegisterCustomField.
// TODO(xunjieli): Remove this once crbug.com/544976 is fixed.
bool GetMockCertVerifierFromString(
    const base::StringPiece& mock_cert_verifier_string,
    scoped_ptr<net::CertVerifier>* result) {
  int64 val;
  bool success = base::StringToInt64(mock_cert_verifier_string, &val);
  *result = make_scoped_ptr(reinterpret_cast<net::CertVerifier*>(val));
  return success;
}

void ParseAndSetExperimentalOptions(
    const std::string& experimental_options,
    net::URLRequestContextBuilder* context_builder) {
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
}

}  // namespace

#define DEFINE_CONTEXT_CONFIG(x) const char REQUEST_CONTEXT_CONFIG_##x[] = #x;
#include "components/cronet/url_request_context_config_list.h"
#undef DEFINE_CONTEXT_CONFIG

URLRequestContextConfig::QuicHint::QuicHint() {
}

URLRequestContextConfig::QuicHint::~QuicHint() {
}

// static
void URLRequestContextConfig::QuicHint::RegisterJSONConverter(
    base::JSONValueConverter<URLRequestContextConfig::QuicHint>* converter) {
  converter->RegisterStringField(REQUEST_CONTEXT_CONFIG_QUIC_HINT_HOST,
                                 &URLRequestContextConfig::QuicHint::host);
  converter->RegisterIntField(
      REQUEST_CONTEXT_CONFIG_QUIC_HINT_PORT,
      &URLRequestContextConfig::QuicHint::port);
  converter->RegisterIntField(
      REQUEST_CONTEXT_CONFIG_QUIC_HINT_ALT_PORT,
      &URLRequestContextConfig::QuicHint::alternate_port);
}

URLRequestContextConfig::URLRequestContextConfig() {}

URLRequestContextConfig::~URLRequestContextConfig() {}

bool URLRequestContextConfig::LoadFromJSON(const std::string& config_string) {
  scoped_ptr<base::Value> config_value = base::JSONReader::Read(config_string);
  if (!config_value || !config_value->IsType(base::Value::TYPE_DICTIONARY)) {
    DLOG(ERROR) << "Bad JSON: " << config_string;
    return false;
  }

  base::JSONValueConverter<URLRequestContextConfig> converter;
  if (!converter.Convert(*config_value, this)) {
    DLOG(ERROR) << "Bad Config: " << config_value;
    return false;
  }
  return true;
}

void URLRequestContextConfig::ConfigureURLRequestContextBuilder(
    net::URLRequestContextBuilder* context_builder) {
  std::string config_cache;
  if (http_cache != REQUEST_CONTEXT_CONFIG_HTTP_CACHE_DISABLED) {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    if (http_cache == REQUEST_CONTEXT_CONFIG_HTTP_CACHE_DISK &&
        !storage_path.empty()) {
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

  ParseAndSetExperimentalOptions(experimental_options, context_builder);

  if (mock_cert_verifier)
    context_builder->SetCertVerifier(mock_cert_verifier.Pass());
  // TODO(mef): Use |config| to set cookies.
}

// static
void URLRequestContextConfig::RegisterJSONConverter(
    base::JSONValueConverter<URLRequestContextConfig>* converter) {
  converter->RegisterStringField(REQUEST_CONTEXT_CONFIG_USER_AGENT,
                                 &URLRequestContextConfig::user_agent);
  converter->RegisterStringField(REQUEST_CONTEXT_CONFIG_STORAGE_PATH,
                                 &URLRequestContextConfig::storage_path);
  converter->RegisterBoolField(REQUEST_CONTEXT_CONFIG_ENABLE_QUIC,
                               &URLRequestContextConfig::enable_quic);
  converter->RegisterBoolField(REQUEST_CONTEXT_CONFIG_ENABLE_SPDY,
                               &URLRequestContextConfig::enable_spdy);
  converter->RegisterBoolField(REQUEST_CONTEXT_CONFIG_ENABLE_SDCH,
                               &URLRequestContextConfig::enable_sdch);
  converter->RegisterStringField(REQUEST_CONTEXT_CONFIG_HTTP_CACHE,
                                 &URLRequestContextConfig::http_cache);
  converter->RegisterBoolField(REQUEST_CONTEXT_CONFIG_LOAD_DISABLE_CACHE,
                               &URLRequestContextConfig::load_disable_cache);
  converter->RegisterIntField(REQUEST_CONTEXT_CONFIG_HTTP_CACHE_MAX_SIZE,
                              &URLRequestContextConfig::http_cache_max_size);
  converter->RegisterRepeatedMessage(REQUEST_CONTEXT_CONFIG_QUIC_HINTS,
                                     &URLRequestContextConfig::quic_hints);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_EXPERIMENTAL_OPTIONS,
      &URLRequestContextConfig::experimental_options);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_DATA_REDUCTION_PRIMARY_PROXY,
      &URLRequestContextConfig::data_reduction_primary_proxy);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_DATA_REDUCTION_FALLBACK_PROXY,
      &URLRequestContextConfig::data_reduction_fallback_proxy);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_DATA_REDUCTION_SECURE_PROXY_CHECK_URL,
      &URLRequestContextConfig::data_reduction_secure_proxy_check_url);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_DATA_REDUCTION_PROXY_KEY,
      &URLRequestContextConfig::data_reduction_proxy_key);

  // For Testing.
  converter->RegisterCustomField<scoped_ptr<net::CertVerifier>>(
      REQUEST_CONTEXT_CONFIG_MOCK_CERT_VERIFIER,
      &URLRequestContextConfig::mock_cert_verifier,
      &GetMockCertVerifierFromString);
}

}  // namespace cronet
