// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "net/cert/cert_verifier.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

TEST(URLRequestContextConfigTest, TestExperimentalOptionParsing) {
  base::test::ScopedTaskEnvironment scoped_task_environment_(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);

  URLRequestContextConfig config(
      // Enable QUIC.
      true,
      // QUIC User Agent ID.
      "Default QUIC User Agent ID",
      // Enable SPDY.
      true,
      // Enable Brotli.
      false,
      // Type of http cache.
      URLRequestContextConfig::HttpCacheType::DISK,
      // Max size of http cache in bytes.
      1024000,
      // Disable caching for HTTP responses. Other information may be stored in
      // the cache.
      false,
      // Storage path for http cache and cookie storage.
      "/data/data/org.chromium.net/app_cronet_test/test_storage",
      // User-Agent request header field.
      "fake agent",
      // JSON encoded experimental options.
      "{\"QUIC\":{\"max_server_configs_stored_in_properties\":2,"
      "\"user_agent_id\":\"Custom QUIC UAID\","
      "\"idle_connection_timeout_seconds\":300,"
      "\"race_cert_verification\":true,"
      "\"connection_options\":\"TIME,TBBR,REJ\"},"
      "\"AsyncDNS\":{\"enable\":true},"
      "\"UnknownOption\":{\"foo\":true},"
      "\"HostResolverRules\":{\"host_resolver_rules\":"
      "\"MAP * 127.0.0.1\"},"
      // See http://crbug.com/696569.
      "\"disable_ipv6_on_wifi\":true}",
      // MockCertVerifier to use for testing purposes.
      std::unique_ptr<net::CertVerifier>(),
      // Enable network quality estimator.
      false,
      // Enable Public Key Pinning bypass for local trust anchors.
      true,
      // Certificate verifier cache data.
      "");

  net::URLRequestContextBuilder builder;
  net::NetLog net_log;
  config.ConfigureURLRequestContextBuilder(&builder, &net_log);
  EXPECT_FALSE(config.effective_experimental_options->HasKey("UnknownOption"));
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      base::MakeUnique<net::ProxyConfigServiceFixed>(
          net::ProxyConfig::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::HttpNetworkSession::Params* params =
      context->GetNetworkSessionParams();
  // Check Quic Connection options.
  net::QuicTagVector quic_connection_options;
  quic_connection_options.push_back(net::kTIME);
  quic_connection_options.push_back(net::kTBBR);
  quic_connection_options.push_back(net::kREJ);
  EXPECT_EQ(quic_connection_options, params->quic_connection_options);

  // Check Custom QUIC User Agent Id.
  EXPECT_EQ("Custom QUIC UAID", params->quic_user_agent_id);

  // Check max_server_configs_stored_in_properties.
  EXPECT_EQ(2u, params->quic_max_server_configs_stored_in_properties);

  // Check idle_connection_timeout_seconds.
  EXPECT_EQ(300, params->quic_idle_connection_timeout_seconds);

  EXPECT_FALSE(params->quic_migrate_sessions_on_network_change);
  EXPECT_FALSE(params->quic_migrate_sessions_on_network_change_v2);

  // Check race_cert_verification.
  EXPECT_TRUE(params->quic_race_cert_verification);

  // Check AsyncDNS resolver is enabled.
  EXPECT_TRUE(context->host_resolver()->GetDnsConfigAsValue());

  // Check IPv6 is disabled when on wifi.
  EXPECT_TRUE(context->host_resolver()->GetNoIPv6OnWifi());

  net::HostResolver::RequestInfo info(net::HostPortPair("abcde", 80));
  net::AddressList addresses;
  EXPECT_EQ(net::OK, context->host_resolver()->ResolveFromCache(
                         info, &addresses, net::NetLogWithSource()));
}

TEST(URLRequestContextConfigTest, SetQuicConnectionMigrationOptions) {
  base::test::ScopedTaskEnvironment scoped_task_environment_(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);

  URLRequestContextConfig config(
      // Enable QUIC.
      true,
      // QUIC User Agent ID.
      "Default QUIC User Agent ID",
      // Enable SPDY.
      true,
      // Enable Brotli.
      false,
      // Type of http cache.
      URLRequestContextConfig::HttpCacheType::DISK,
      // Max size of http cache in bytes.
      1024000,
      // Disable caching for HTTP responses. Other information may be stored in
      // the cache.
      false,
      // Storage path for http cache and cookie storage.
      "/data/data/org.chromium.net/app_cronet_test/test_storage",
      // User-Agent request header field.
      "fake agent",
      // JSON encoded experimental options.
      "{\"QUIC\":{\"migrate_sessions_on_network_change\":true,"
      "\"migrate_sessions_early\":true}}",
      // MockCertVerifier to use for testing purposes.
      std::unique_ptr<net::CertVerifier>(),
      // Enable network quality estimator.
      false,
      // Enable Public Key Pinning bypass for local trust anchors.
      true,
      // Certificate verifier cache data.
      "");

  net::URLRequestContextBuilder builder;
  net::NetLog net_log;
  config.ConfigureURLRequestContextBuilder(&builder, &net_log);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      base::MakeUnique<net::ProxyConfigServiceFixed>(
          net::ProxyConfig::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::HttpNetworkSession::Params* params =
      context->GetNetworkSessionParams();

  EXPECT_TRUE(params->quic_migrate_sessions_on_network_change);
  EXPECT_TRUE(params->quic_migrate_sessions_early);
}

TEST(URLRequestContextConfigTest, SetQuicConnectionMigrationV2Options) {
  base::test::ScopedTaskEnvironment scoped_task_environment_(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);

  URLRequestContextConfig config(
      // Enable QUIC.
      true,
      // QUIC User Agent ID.
      "Default QUIC User Agent ID",
      // Enable SPDY.
      true,
      // Enable Brotli.
      false,
      // Type of http cache.
      URLRequestContextConfig::HttpCacheType::DISK,
      // Max size of http cache in bytes.
      1024000,
      // Disable caching for HTTP responses. Other information may be stored in
      // the cache.
      false,
      // Storage path for http cache and cookie storage.
      "/data/data/org.chromium.net/app_cronet_test/test_storage",
      // User-Agent request header field.
      "fake agent",
      // JSON encoded experimental options.
      "{\"QUIC\":{\"migrate_sessions_on_network_change_v2\":true}}",
      // MockCertVerifier to use for testing purposes.
      std::unique_ptr<net::CertVerifier>(),
      // Enable network quality estimator.
      false,
      // Enable Public Key Pinning bypass for local trust anchors.
      true,
      // Certificate verifier cache data.
      "");

  net::URLRequestContextBuilder builder;
  net::NetLog net_log;
  config.ConfigureURLRequestContextBuilder(&builder, &net_log);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      base::MakeUnique<net::ProxyConfigServiceFixed>(
          net::ProxyConfig::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::HttpNetworkSession::Params* params =
      context->GetNetworkSessionParams();

  EXPECT_TRUE(params->quic_migrate_sessions_on_network_change_v2);
}

TEST(URLRequestContextConfigTest, SetQuicHostWhitelist) {
  base::test::ScopedTaskEnvironment scoped_task_environment_(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);

  URLRequestContextConfig config(
      // Enable QUIC.
      true,
      // QUIC User Agent ID.
      "Default QUIC User Agent ID",
      // Enable SPDY.
      true,
      // Enable Brotli.
      false,
      // Type of http cache.
      URLRequestContextConfig::HttpCacheType::DISK,
      // Max size of http cache in bytes.
      1024000,
      // Disable caching for HTTP responses. Other information may be stored in
      // the cache.
      false,
      // Storage path for http cache and cookie storage.
      "/data/data/org.chromium.net/app_cronet_test/test_storage",
      // User-Agent request header field.
      "fake agent",
      // JSON encoded experimental options.
      "{\"QUIC\":{\"host_whitelist\":\"www.example.com,www.example.org\"}}",
      // MockCertVerifier to use for testing purposes.
      std::unique_ptr<net::CertVerifier>(),
      // Enable network quality estimator.
      false,
      // Enable Public Key Pinning bypass for local trust anchors.
      true,
      // Certificate verifier cache data.
      "");

  net::URLRequestContextBuilder builder;
  net::NetLog net_log;
  config.ConfigureURLRequestContextBuilder(&builder, &net_log);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      base::MakeUnique<net::ProxyConfigServiceFixed>(
          net::ProxyConfig::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::HttpNetworkSession::Params* params =
      context->GetNetworkSessionParams();

  EXPECT_TRUE(
      base::ContainsKey(params->quic_host_whitelist, "www.example.com"));
  EXPECT_TRUE(
      base::ContainsKey(params->quic_host_whitelist, "www.example.org"));
}

// See stale_host_resolver_unittest.cc for test of StaleDNS options.

}  // namespace cronet
