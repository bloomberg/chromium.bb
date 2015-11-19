// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include "net/http/http_network_session.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

TEST(URLRequestContextConfigTest, SetQuicExperimentalOptions) {
  URLRequestContextConfig config;

  std::string args =
      "{\"QUIC_HINTS\":[{\"QUIC_HINT_ALT_PORT\":6121,\"QUIC_HINT_PORT\":6121,"
      "\"QUIC_HINT_HOST\":\"test.example.com\"}],"
      "\"HTTP_CACHE\":\"HTTP_CACHE_DISK\",\"ENABLE_SDCH\":false,"
      "\"ENABLE_LEGACY_MODE\":false,\"HTTP_CACHE_MAX_SIZE\":1024000,"
      "\"NATIVE_LIBRARY_NAME\":\"cronet_tests\",\"USER_AGENT\":\"fake agent\","
      "\"STORAGE_PATH\":"
      "\"\\/data\\/data\\/org.chromium.net\\/app_cronet_test\\/test_storage\","
      "\"ENABLE_SPDY\":true,"
      "\"ENABLE_QUIC\":true,\"LOAD_DISABLE_CACHE\":true,"
      "\"EXPERIMENTAL_OPTIONS\":"
      "\"{\\\"QUIC\\\":{\\\"store_server_configs_in_properties\\\":true,"
      "\\\"delay_tcp_race\\\":true,"
      "\\\"max_number_of_lossy_connections\\\":10,"
      "\\\"packet_loss_threshold\\\":0.5,"
      "\\\"connection_options\\\":\\\"TIME,TBBR,REJ\\\"}}\"}";
  config.LoadFromJSON(args);
  net::URLRequestContextBuilder builder;
  config.ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(make_scoped_ptr(
      new net::ProxyConfigServiceFixed(net::ProxyConfig::CreateDirect())));
  scoped_ptr<net::URLRequestContext> context(builder.Build());
  const net::HttpNetworkSession::Params* params =
      context->GetNetworkSessionParams();
  // Check Quic Connection options.
  net::QuicTagVector quic_connection_options;
  quic_connection_options.push_back(net::kTIME);
  quic_connection_options.push_back(net::kTBBR);
  quic_connection_options.push_back(net::kREJ);
  EXPECT_EQ(quic_connection_options, params->quic_connection_options);

  // Check store_server_configs_in_properties.
  EXPECT_TRUE(params->quic_store_server_configs_in_properties);

  // Check delay_tcp_race.
  EXPECT_TRUE(params->quic_delay_tcp_race);

  // Check max_number_of_lossy_connections and packet_loss_threshold.
  EXPECT_EQ(10, params->quic_max_number_of_lossy_connections);
  EXPECT_FLOAT_EQ(0.5f, params->quic_packet_loss_threshold);
}

}  // namespace cronet
