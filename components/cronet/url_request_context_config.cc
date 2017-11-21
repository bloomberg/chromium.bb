// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/cronet/stale_host_resolver.h"
#include "net/base/address_family.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/quic/chromium/quic_utils_chromium.h"
#include "net/quic/core/quic_packets.h"
#include "net/socket/ssl_client_socket.h"
#include "net/url_request/url_request_context_builder.h"

namespace cronet {

namespace {

// Name of disk cache directory.
const char kDiskCacheDirectoryName[] = "disk_cache";
// TODO(xunjieli): Refactor constants in io_thread.cc.
const char kQuicFieldTrialName[] = "QUIC";
const char kQuicConnectionOptions[] = "connection_options";
const char kQuicStoreServerConfigsInProperties[] =
    "store_server_configs_in_properties";
const char kQuicMaxServerConfigsStoredInProperties[] =
    "max_server_configs_stored_in_properties";
const char kQuicIdleConnectionTimeoutSeconds[] =
    "idle_connection_timeout_seconds";
const char kQuicMaxTimeBeforeCryptoHandshakeSeconds[] =
    "max_time_before_crypto_handshake_seconds";
const char kQuicMaxIdleTimeBeforeCryptoHandshakeSeconds[] =
    "max_idle_time_before_crypto_handshake_seconds";
const char kQuicMigrateSessionsOnNetworkChange[] =
    "migrate_sessions_on_network_change";
const char kQuicMigrateSessionsOnNetworkChangeV2[] =
    "migrate_sessions_on_network_change_v2";
const char kQuicUserAgentId[] = "user_agent_id";
const char kQuicMigrateSessionsEarly[] = "migrate_sessions_early";
const char kQuicDisableBidirectionalStreams[] =
    "quic_disable_bidirectional_streams";
const char kQuicRaceCertVerification[] = "race_cert_verification";
const char kQuicHostWhitelist[] = "host_whitelist";

// AsyncDNS experiment dictionary name.
const char kAsyncDnsFieldTrialName[] = "AsyncDNS";
// Name of boolean to enable AsyncDNS experiment.
const char kAsyncDnsEnable[] = "enable";

// Stale DNS (StaleHostResolver) experiment dictionary name.
const char kStaleDnsFieldTrialName[] = "StaleDNS";
// Name of boolean to enable stale DNS experiment.
const char kStaleDnsEnable[] = "enable";
// Name of integer delay in milliseconds before a stale DNS result will be
// used.
const char kStaleDnsDelayMs[] = "delay_ms";
// Name of integer maximum age (past expiration) in milliseconds of a stale DNS
// result that will be used, or 0 for no limit.
const char kStaleDnsMaxExpiredTimeMs[] = "max_expired_time_ms";
// Name of integer maximum times each stale DNS result can be used, or 0 for no
// limit.
const char kStaleDnsMaxStaleUses[] = "max_stale_uses";
// Name of boolean to allow stale DNS results from other networks to be used on
// the current network.
const char kStaleDnsAllowOtherNetwork[] = "allow_other_network";
// Name of boolean to enable persisting the DNS cache to disk.
const char kStaleDnsPersist[] = "persist_to_disk";
// Name of integer minimum time in milliseconds between writes to disk for DNS
// cache persistence. Default value is one minute. Only relevant if
// "persist_to_disk" is true.
const char kStaleDnsPersistTimer[] = "persist_delay_ms";

// Rules to override DNS resolution. Intended for testing.
// See explanation of format in net/dns/mapped_host_resolver.h.
const char kHostResolverRulesFieldTrialName[] = "HostResolverRules";
const char kHostResolverRules[] = "host_resolver_rules";

// NetworkQualityEstimator (NQE) experiment dictionary name.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

// Disable IPv6 when on WiFi. This is a workaround for a known issue on certain
// Android phones, and should not be necessary when not on one of those devices.
// See https://crbug.com/696569 for details.
const char kDisableIPv6OnWifi[] = "disable_ipv6_on_wifi";

const char kSSLKeyLogFile[] = "ssl_key_log_file";

// A CTPolicyEnforcer that accepts all certificates.
class DoNothingCTPolicyEnforcer : public net::CTPolicyEnforcer {
 public:
  DoNothingCTPolicyEnforcer() = default;
  ~DoNothingCTPolicyEnforcer() override = default;

  net::ct::CertPolicyCompliance DoesConformToCertPolicy(
      net::X509Certificate* cert,
      const net::SCTList& verified_scts,
      const net::NetLogWithSource& net_log) override {
    return net::ct::CertPolicyCompliance::CERT_POLICY_COMPLIES_VIA_SCTS;
  }
};

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
    const std::string& quic_user_agent_id,
    bool enable_spdy,
    bool enable_brotli,
    HttpCacheType http_cache,
    int http_cache_max_size,
    bool load_disable_cache,
    const std::string& storage_path,
    const std::string& user_agent,
    const std::string& experimental_options,
    std::unique_ptr<net::CertVerifier> mock_cert_verifier,
    bool enable_network_quality_estimator,
    bool bypass_public_key_pinning_for_local_trust_anchors,
    const std::string& cert_verifier_data)
    : enable_quic(enable_quic),
      quic_user_agent_id(quic_user_agent_id),
      enable_spdy(enable_spdy),
      enable_brotli(enable_brotli),
      http_cache(http_cache),
      http_cache_max_size(http_cache_max_size),
      load_disable_cache(load_disable_cache),
      storage_path(storage_path),
      user_agent(user_agent),
      mock_cert_verifier(std::move(mock_cert_verifier)),
      enable_network_quality_estimator(enable_network_quality_estimator),
      bypass_public_key_pinning_for_local_trust_anchors(
          bypass_public_key_pinning_for_local_trust_anchors),
      cert_verifier_data(cert_verifier_data),
      experimental_options(experimental_options) {}

URLRequestContextConfig::~URLRequestContextConfig() {}

void URLRequestContextConfig::ParseAndSetExperimentalOptions(
    net::URLRequestContextBuilder* context_builder,
    net::HttpNetworkSession::Params* session_params,
    net::NetLog* net_log) {
  if (experimental_options.empty())
    return;

  DCHECK(net_log);

  DVLOG(1) << "Experimental Options:" << experimental_options;
  std::unique_ptr<base::Value> options =
      base::JSONReader::Read(experimental_options);

  if (!options) {
    DCHECK(false) << "Parsing experimental options failed: "
                  << experimental_options;
    return;
  }

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(options));

  if (!dict) {
    DCHECK(false) << "Experimental options string is not a dictionary: "
                  << experimental_options;
    return;
  }

  bool async_dns_enable = false;
  bool stale_dns_enable = false;
  bool host_resolver_rules_enable = false;
  bool disable_ipv6_on_wifi = false;

  effective_experimental_options = dict->CreateDeepCopy();
  StaleHostResolver::StaleOptions stale_dns_options;
  std::string host_resolver_rules_string;

  for (base::DictionaryValue::Iterator it(*dict.get()); !it.IsAtEnd();
       it.Advance()) {
    if (it.key() == kQuicFieldTrialName) {
      const base::DictionaryValue* quic_args = nullptr;
      if (!it.value().GetAsDictionary(&quic_args)) {
        LOG(ERROR) << "Quic config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->Remove(it.key(), nullptr);
        continue;
      }
      std::string quic_connection_options;
      if (quic_args->GetString(kQuicConnectionOptions,
                               &quic_connection_options)) {
        session_params->quic_connection_options =
            net::ParseQuicConnectionOptions(quic_connection_options);
      }

      // TODO(rtenneti): Delete this option after apps stop using it.
      // Added this for backward compatibility.
      bool quic_store_server_configs_in_properties = false;
      if (quic_args->GetBoolean(kQuicStoreServerConfigsInProperties,
                                &quic_store_server_configs_in_properties)) {
        session_params->quic_max_server_configs_stored_in_properties =
            net::kMaxQuicServersToPersist;
      }

      int quic_max_server_configs_stored_in_properties = 0;
      if (quic_args->GetInteger(
              kQuicMaxServerConfigsStoredInProperties,
              &quic_max_server_configs_stored_in_properties)) {
        session_params->quic_max_server_configs_stored_in_properties =
            static_cast<size_t>(quic_max_server_configs_stored_in_properties);
      }

      int quic_idle_connection_timeout_seconds = 0;
      if (quic_args->GetInteger(kQuicIdleConnectionTimeoutSeconds,
                                &quic_idle_connection_timeout_seconds)) {
        session_params->quic_idle_connection_timeout_seconds =
            quic_idle_connection_timeout_seconds;
      }

      int quic_max_time_before_crypto_handshake_seconds = 0;
      if (quic_args->GetInteger(
              kQuicMaxTimeBeforeCryptoHandshakeSeconds,
              &quic_max_time_before_crypto_handshake_seconds)) {
        session_params->quic_max_time_before_crypto_handshake_seconds =
            quic_max_time_before_crypto_handshake_seconds;
      }

      int quic_max_idle_time_before_crypto_handshake_seconds = 0;
      if (quic_args->GetInteger(
              kQuicMaxIdleTimeBeforeCryptoHandshakeSeconds,
              &quic_max_idle_time_before_crypto_handshake_seconds)) {
        session_params->quic_max_idle_time_before_crypto_handshake_seconds =
            quic_max_idle_time_before_crypto_handshake_seconds;
      }

      bool quic_migrate_sessions_on_network_change = false;
      if (quic_args->GetBoolean(kQuicMigrateSessionsOnNetworkChange,
                                &quic_migrate_sessions_on_network_change)) {
        session_params->quic_migrate_sessions_on_network_change =
            quic_migrate_sessions_on_network_change;
      }

      bool quic_migrate_sessions_on_network_change_v2 = false;
      if (quic_args->GetBoolean(kQuicMigrateSessionsOnNetworkChangeV2,
                                &quic_migrate_sessions_on_network_change_v2)) {
        session_params->quic_migrate_sessions_on_network_change_v2 =
            quic_migrate_sessions_on_network_change_v2;
      }

      std::string quic_user_agent_id;
      if (quic_args->GetString(kQuicUserAgentId, &quic_user_agent_id)) {
        session_params->quic_user_agent_id = quic_user_agent_id;
      }

      bool quic_migrate_sessions_early = false;
      if (quic_args->GetBoolean(kQuicMigrateSessionsEarly,
                                &quic_migrate_sessions_early)) {
        session_params->quic_migrate_sessions_early =
            quic_migrate_sessions_early;
      }

      bool quic_disable_bidirectional_streams = false;
      if (quic_args->GetBoolean(kQuicDisableBidirectionalStreams,
                                &quic_disable_bidirectional_streams)) {
        session_params->quic_disable_bidirectional_streams =
            quic_disable_bidirectional_streams;
      }

      bool quic_race_cert_verification = false;
      if (quic_args->GetBoolean(kQuicRaceCertVerification,
                                &quic_race_cert_verification)) {
        session_params->quic_race_cert_verification =
            quic_race_cert_verification;
      }

      std::string quic_host_whitelist;
      if (quic_args->GetString(kQuicHostWhitelist, &quic_host_whitelist)) {
        std::vector<std::string> host_vector =
            base::SplitString(quic_host_whitelist, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL);
        session_params->quic_host_whitelist.clear();
        for (const std::string& host : host_vector) {
          session_params->quic_host_whitelist.insert(host);
        }
      }

    } else if (it.key() == kAsyncDnsFieldTrialName) {
      const base::DictionaryValue* async_dns_args = nullptr;
      if (!it.value().GetAsDictionary(&async_dns_args)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->Remove(it.key(), nullptr);
        continue;
      }
      async_dns_args->GetBoolean(kAsyncDnsEnable, &async_dns_enable);
    } else if (it.key() == kStaleDnsFieldTrialName) {
      const base::DictionaryValue* stale_dns_args = nullptr;
      if (!it.value().GetAsDictionary(&stale_dns_args)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->Remove(it.key(), nullptr);
        continue;
      }
      if (stale_dns_args->GetBoolean(kStaleDnsEnable, &stale_dns_enable) &&
          stale_dns_enable) {
        int delay;
        if (stale_dns_args->GetInteger(kStaleDnsDelayMs, &delay))
          stale_dns_options.delay = base::TimeDelta::FromMilliseconds(delay);
        int max_expired_time_ms;
        if (stale_dns_args->GetInteger(kStaleDnsMaxExpiredTimeMs,
                                       &max_expired_time_ms)) {
          stale_dns_options.max_expired_time =
              base::TimeDelta::FromMilliseconds(max_expired_time_ms);
        }
        int max_stale_uses;
        if (stale_dns_args->GetInteger(kStaleDnsMaxStaleUses, &max_stale_uses))
          stale_dns_options.max_stale_uses = max_stale_uses;
        bool allow_other_network;
        if (stale_dns_args->GetBoolean(kStaleDnsAllowOtherNetwork,
                                       &allow_other_network)) {
          stale_dns_options.allow_other_network = allow_other_network;
        }
        bool persist;
        if (stale_dns_args->GetBoolean(kStaleDnsPersist, &persist))
          enable_host_cache_persistence = persist;
        int persist_timer;
        if (stale_dns_args->GetInteger(kStaleDnsPersistTimer, &persist_timer))
          host_cache_persistence_delay_ms = persist_timer;
      }
    } else if (it.key() == kHostResolverRulesFieldTrialName) {
      const base::DictionaryValue* host_resolver_rules_args = nullptr;
      if (!it.value().GetAsDictionary(&host_resolver_rules_args)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->Remove(it.key(), nullptr);
        continue;
      }
      host_resolver_rules_enable = host_resolver_rules_args->GetString(
          kHostResolverRules, &host_resolver_rules_string);
    } else if (it.key() == kDisableIPv6OnWifi) {
      if (!it.value().GetAsBoolean(&disable_ipv6_on_wifi)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a bool";
        effective_experimental_options->Remove(it.key(), nullptr);
        continue;
      }
    } else if (it.key() == kSSLKeyLogFile) {
      std::string ssl_key_log_file_string;
      if (it.value().GetAsString(&ssl_key_log_file_string)) {
        base::FilePath ssl_key_log_file(ssl_key_log_file_string);
        if (!ssl_key_log_file.empty()) {
          // SetSSLKeyLogFile is only safe to call before any SSLClientSockets
          // are created. This should not be used if there are multiple
          // CronetEngine.
          // TODO(xunjieli): Expose this as a stable API after crbug.com/458365
          // is resolved.
          net::SSLClientSocket::SetSSLKeyLogFile(ssl_key_log_file);
        }
      }
    } else if (it.key() == kNetworkQualityEstimatorFieldTrialName) {
      const base::DictionaryValue* nqe_args = nullptr;
      if (!it.value().GetAsDictionary(&nqe_args)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->Remove(it.key(), nullptr);
        continue;
      }

      std::string nqe_option;
      if (nqe_args->GetString(net::kForceEffectiveConnectionType,
                              &nqe_option)) {
        nqe_forced_effective_connection_type =
            net::GetEffectiveConnectionTypeForName(nqe_option);
        if (!nqe_option.empty() && !nqe_forced_effective_connection_type) {
          LOG(ERROR) << "\"" << nqe_option
                     << "\" is not a valid effective connection type value";
        }
      }

    } else {
      LOG(WARNING) << "Unrecognized Cronet experimental option \"" << it.key()
                   << "\" with params \"" << it.value();
      effective_experimental_options->Remove(it.key(), nullptr);
    }
  }

  if (async_dns_enable || stale_dns_enable || host_resolver_rules_enable ||
      disable_ipv6_on_wifi) {
    CHECK(net_log) << "All DNS-related experiments require NetLog.";
    std::unique_ptr<net::HostResolver> host_resolver;
    if (stale_dns_enable) {
      DCHECK(!disable_ipv6_on_wifi);
      host_resolver.reset(new StaleHostResolver(
          net::HostResolver::CreateDefaultResolverImpl(net_log),
          stale_dns_options));
    } else {
      host_resolver = net::HostResolver::CreateDefaultResolver(net_log);
    }
    if (disable_ipv6_on_wifi)
      host_resolver->SetNoIPv6OnWifi(true);
    if (async_dns_enable)
      host_resolver->SetDnsClientEnabled(true);
    if (host_resolver_rules_enable) {
      std::unique_ptr<net::MappedHostResolver> remapped_resolver(
          new net::MappedHostResolver(std::move(host_resolver)));
      remapped_resolver->SetRulesFromString(host_resolver_rules_string);
      host_resolver = std::move(remapped_resolver);
    }
    context_builder->set_host_resolver(std::move(host_resolver));
  }
}

void URLRequestContextConfig::ConfigureURLRequestContextBuilder(
    net::URLRequestContextBuilder* context_builder,
    net::NetLog* net_log) {
  DCHECK(net_log);

  std::string config_cache;
  if (http_cache != DISABLED) {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    if (http_cache == DISK && !storage_path.empty()) {
      cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
      cache_params.path =
          base::FilePath(storage_path)
              .Append(FILE_PATH_LITERAL(kDiskCacheDirectoryName));
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
  net::HttpNetworkSession::Params session_params;
  session_params.enable_http2 = enable_spdy;
  session_params.enable_quic = enable_quic;
  if (enable_quic)
    session_params.quic_user_agent_id = quic_user_agent_id;

  ParseAndSetExperimentalOptions(context_builder, &session_params, net_log);
  context_builder->set_http_network_session_params(session_params);

  std::unique_ptr<net::CertVerifier> cert_verifier;
  if (mock_cert_verifier) {
    // Because |context_builder| expects CachingCertVerifier, wrap
    // |mock_cert_verifier| into a CachingCertVerifier.
    cert_verifier = base::MakeUnique<net::CachingCertVerifier>(
        std::move(mock_cert_verifier));
  } else {
    // net::CertVerifier::CreateDefault() returns a CachingCertVerifier.
    cert_verifier = net::CertVerifier::CreateDefault();
  }
  context_builder->SetCertVerifier(std::move(cert_verifier));
  // Certificate Transparency is intentionally ignored in Cronet.
  // See //net/docs/certificate-transparency.md for more details.
  context_builder->set_ct_verifier(
      base::MakeUnique<net::DoNothingCTVerifier>());
  context_builder->set_ct_policy_enforcer(
      base::MakeUnique<DoNothingCTPolicyEnforcer>());
  // TODO(mef): Use |config| to set cookies.
}

URLRequestContextConfigBuilder::URLRequestContextConfigBuilder() {}
URLRequestContextConfigBuilder::~URLRequestContextConfigBuilder() {}

std::unique_ptr<URLRequestContextConfig>
URLRequestContextConfigBuilder::Build() {
  return base::MakeUnique<URLRequestContextConfig>(
      enable_quic, quic_user_agent_id, enable_spdy, enable_brotli, http_cache,
      http_cache_max_size, load_disable_cache, storage_path, user_agent,
      experimental_options, std::move(mock_cert_verifier),
      enable_network_quality_estimator,
      bypass_public_key_pinning_for_local_trust_anchors, cert_verifier_data);
}

}  // namespace cronet
