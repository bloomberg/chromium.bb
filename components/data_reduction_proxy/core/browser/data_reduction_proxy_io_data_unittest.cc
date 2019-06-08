// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {
namespace {

std::string CreateEncodedConfig(
    const std::vector<DataReductionProxyServer> proxy_servers) {
  ClientConfig config;
  config.set_session_key("session");
  for (const auto& proxy_server : proxy_servers) {
    ProxyServer* config_proxy =
        config.mutable_proxy_config()->add_http_proxy_servers();
    net::HostPortPair host_port_pair =
        proxy_server.proxy_server().host_port_pair();
    config_proxy->set_scheme(ProxyServer_ProxyScheme_HTTP);
    config_proxy->set_host(host_port_pair.host());
    config_proxy->set_port(host_port_pair.port());
  }
  return EncodeConfig(config);
}
}  // namespace

class DataReductionProxyIODataTest : public testing::Test {
 public:
  DataReductionProxyIODataTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    RegisterSimpleProfilePrefs(prefs_.registry());
  }

  void RequestCallback(int err) {
  }

  PrefService* prefs() {
    return &prefs_;
  }

 protected:
  content::TestBrowserThreadBundle scoped_task_environment_;

 private:
  TestingPrefServiceSimple prefs_;
};

class TestCustomProxyConfigClient
    : public network::mojom::CustomProxyConfigClient {
 public:
  TestCustomProxyConfigClient(
      network::mojom::CustomProxyConfigClientRequest request)
      : binding_(this, std::move(request)) {}

  // network::mojom::CustomProxyConfigClient implementation:
  void OnCustomProxyConfigUpdated(
      network::mojom::CustomProxyConfigPtr proxy_config) override {
    config = std::move(proxy_config);
  }

  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override {}

  void ClearBadProxiesCache() override { num_clear_cache_calls++; }

  network::mojom::CustomProxyConfigPtr config;
  int num_clear_cache_calls = 0;

 private:
  mojo::Binding<network::mojom::CustomProxyConfigClient> binding_;
};

TEST_F(DataReductionProxyIODataTest, TestResetBadProxyListOnDisableDataSaver) {
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .SkipSettingsInitialization()
          .Build();

  drp_test_context->SetDataReductionProxyEnabled(true);
  drp_test_context->InitSettings();

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  drp_test_context->io_data()->SetCustomProxyConfigClient(
      std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  // Turn Data Saver off.
  drp_test_context->SetDataReductionProxyEnabled(false);
  base::RunLoop().RunUntilIdle();

  // Verify that the bad proxy cache was cleared.
  EXPECT_EQ(1, client.num_clear_cache_calls);
}

TEST_F(DataReductionProxyIODataTest, HoldbackConfiguresProxies) {
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .SkipSettingsInitialization()
          .Build();

  EXPECT_TRUE(drp_test_context->test_params()->proxies_for_http().size() > 0);
  EXPECT_FALSE(drp_test_context->test_params()
                   ->proxies_for_http()
                   .front()
                   .proxy_server()
                   .is_direct());
}

TEST_F(DataReductionProxyIODataTest, TestCustomProxyConfigClient) {
  auto proxy_server = net::ProxyServer::FromPacString("PROXY foo");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyHttpProxies, proxy_server.ToURI());

  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  drp_test_context->test_network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);
  DataReductionProxyIOData* io_data = drp_test_context->io_data();

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data->SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client.config->rules.proxies_for_http.Get(), proxy_server);
  EXPECT_TRUE(
      client.config->post_cache_headers.HasHeader(chrome_proxy_header()));
  EXPECT_TRUE(
      client.config->pre_cache_headers.HasHeader(chrome_proxy_ect_header()));
}

TEST_F(DataReductionProxyIODataTest, TestCustomProxyConfigUpdatedOnECTChange) {
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  drp_test_context->test_network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);
  DataReductionProxyIOData* io_data = drp_test_context->io_data();

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data->SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  std::string value;
  EXPECT_TRUE(client.config->pre_cache_headers.GetHeader(
      chrome_proxy_ect_header(), &value));
  EXPECT_EQ(value, "4G");

  drp_test_context->test_network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client.config->pre_cache_headers.GetHeader(
      chrome_proxy_ect_header(), &value));
  EXPECT_EQ(value, "2G");
}

TEST_F(DataReductionProxyIODataTest,
       TestCustomProxyConfigUpdatedOnHeaderChange) {
  DataReductionProxyIOData io_data(
      Client::UNKNOWN, prefs(),
      network::TestNetworkConnectionTracker::GetInstance(),
      scoped_task_environment_.GetMainThreadTaskRunner(),
      scoped_task_environment_.GetMainThreadTaskRunner(), false /* enabled */,
      std::string() /* user_agent */, std::string() /* channel */);

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data.SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  std::string value;
  EXPECT_TRUE(client.config->post_cache_headers.GetHeader(chrome_proxy_header(),
                                                          &value));

  io_data.request_options()->SetSecureSession("session_value");
  base::RunLoop().RunUntilIdle();
  std::string changed_value;
  EXPECT_TRUE(client.config->post_cache_headers.GetHeader(chrome_proxy_header(),
                                                          &changed_value));
  EXPECT_NE(value, changed_value);
}

TEST_F(DataReductionProxyIODataTest,
       TestCustomProxyConfigUpdatedOnProxyChange) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetch);
  DataReductionProxyIOData io_data(
      Client::UNKNOWN, prefs(),
      network::TestNetworkConnectionTracker::GetInstance(),
      scoped_task_environment_.GetMainThreadTaskRunner(),
      scoped_task_environment_.GetMainThreadTaskRunner(), false /* enabled */,
      std::string() /* user_agent */, std::string() /* channel */);
  NetworkPropertiesManager network_properties_manager(
      base::DefaultClock::GetInstance(), prefs(),
      scoped_task_environment_.GetMainThreadTaskRunner());
  io_data.config()->SetNetworkPropertiesManagerForTesting(
      &network_properties_manager);
  io_data.config()->UpdateConfigForTesting(true, true, true);

  auto proxy_server1 = net::ProxyServer::FromPacString("PROXY foo");
  io_data.config_client()->ApplySerializedConfig(
      CreateEncodedConfig({DataReductionProxyServer(proxy_server1)}));

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data.SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client.config->rules.proxies_for_http.Get(), proxy_server1);

  auto proxy_server2 = net::ProxyServer::FromPacString("PROXY bar");
  io_data.config_client()->SetRemoteConfigAppliedForTesting(false);
  io_data.config_client()->ApplySerializedConfig(
      CreateEncodedConfig({DataReductionProxyServer(proxy_server2)}));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client.config->rules.proxies_for_http.Get(), proxy_server2);
}

TEST_F(DataReductionProxyIODataTest,
       TestCustomProxyConfigHasAlternateProxyListOfCoreProxies) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetch);
  DataReductionProxyIOData io_data(
      Client::UNKNOWN, prefs(),
      network::TestNetworkConnectionTracker::GetInstance(),
      scoped_task_environment_.GetMainThreadTaskRunner(),
      scoped_task_environment_.GetMainThreadTaskRunner(), false /* enabled */,
      std::string() /* user_agent */, std::string() /* channel */);
  NetworkPropertiesManager network_properties_manager(
      base::DefaultClock::GetInstance(), prefs(),
      scoped_task_environment_.GetMainThreadTaskRunner());
  io_data.config()->SetNetworkPropertiesManagerForTesting(
      &network_properties_manager);
  io_data.config()->UpdateConfigForTesting(true, true, true);

  auto core_proxy_server = net::ProxyServer::FromPacString("PROXY foo");
  auto second_proxy_server = net::ProxyServer::FromPacString("PROXY bar");
  io_data.config_client()->ApplySerializedConfig(
      CreateEncodedConfig({DataReductionProxyServer(core_proxy_server),
                           DataReductionProxyServer(second_proxy_server)}));

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data.SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  net::ProxyConfig::ProxyRules expected_rules;
  expected_rules.type =
      net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
  expected_rules.proxies_for_http.AddProxyServer(core_proxy_server);
  expected_rules.proxies_for_http.AddProxyServer(second_proxy_server);
  expected_rules.proxies_for_http.AddProxyServer(net::ProxyServer::Direct());
  EXPECT_TRUE(client.config->rules.Equals(expected_rules));
}

TEST_F(DataReductionProxyIODataTest, TestCustomProxyConfigProperties) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetch);
  DataReductionProxyIOData io_data(
      Client::UNKNOWN, prefs(),
      network::TestNetworkConnectionTracker::GetInstance(),
      scoped_task_environment_.GetMainThreadTaskRunner(),
      scoped_task_environment_.GetMainThreadTaskRunner(), false /* enabled */,
      std::string() /* user_agent */, std::string() /* channel */);
  NetworkPropertiesManager network_properties_manager(
      base::DefaultClock::GetInstance(), prefs(),
      scoped_task_environment_.GetMainThreadTaskRunner());
  io_data.config()->SetNetworkPropertiesManagerForTesting(
      &network_properties_manager);
  io_data.config()->UpdateConfigForTesting(true, true, true);

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data.SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.config->assume_https_proxies_support_quic);
  EXPECT_FALSE(client.config->can_use_proxy_on_http_url_redirect_cycles);
}

}  // namespace data_reduction_proxy
