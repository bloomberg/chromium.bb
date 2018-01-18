// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/network/network_context.h"
#include "content/network/network_service_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_job_factory.h"
#include "services/network/public/interfaces/network_service.mojom.h"
#include "services/network/public/interfaces/proxy_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace content {

namespace {

network::mojom::NetworkContextParamsPtr CreateContextParams() {
  network::mojom::NetworkContextParamsPtr params =
      network::mojom::NetworkContextParams::New();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfig::CreateDirect();
  return params;
}

class NetworkContextTest : public testing::Test {
 public:
  NetworkContextTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        network_service_(NetworkServiceImpl::CreateForTesting()) {}
  ~NetworkContextTest() override {}

  std::unique_ptr<NetworkContext> CreateContextWithParams(
      network::mojom::NetworkContextParamsPtr context_params) {
    return std::make_unique<NetworkContext>(
        network_service_.get(), mojo::MakeRequest(&network_context_ptr_),
        std::move(context_params));
  }

  // Searches through |backend|'s stats to discover its type. Only supports
  // blockfile and simple caches.
  net::URLRequestContextBuilder::HttpCacheParams::Type GetBackendType(
      disk_cache::Backend* backend) {
    base::StringPairs stats;
    backend->GetStats(&stats);
    for (const auto& pair : stats) {
      if (pair.first != "Cache type")
        continue;

      if (pair.second == "Simple Cache")
        return net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE;
      if (pair.second == "Blockfile Cache")
        return net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE;
      break;
    }

    NOTREACHED();
    return net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
  }

  network::mojom::NetworkService* network_service() const {
    return network_service_.get();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<NetworkServiceImpl> network_service_;
  // Stores the NetworkContextPtr of the most recently created NetworkContext,
  // since destroying this before the NetworkContext itself triggers deletion of
  // the NetworkContext. These tests are probably fine anyways, since the
  // message loop must be spun for that to happen.
  network::mojom::NetworkContextPtr network_context_ptr_;
};

TEST_F(NetworkContextTest, DisableQuic) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableQuic);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  // By default, QUIC should be enabled for new NetworkContexts when the command
  // line indicates it should be.
  EXPECT_TRUE(network_context->url_request_context()
                  ->http_transaction_factory()
                  ->GetSession()
                  ->params()
                  .enable_quic);

  // Disabling QUIC should disable it on existing NetworkContexts.
  network_service()->DisableQuic();
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);

  // Disabling QUIC should disable it new NetworkContexts.
  std::unique_ptr<NetworkContext> network_context2 =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(network_context2->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);

  // Disabling QUIC again should be harmless.
  network_service()->DisableQuic();
  std::unique_ptr<NetworkContext> network_context3 =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(network_context3->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);
}

TEST_F(NetworkContextTest, EnableBrotli) {
  for (bool enable_brotli : {true, false}) {
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    context_params->enable_brotli = enable_brotli;
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));
    EXPECT_EQ(enable_brotli,
              network_context->url_request_context()->enable_brotli());
  }
}

TEST_F(NetworkContextTest, ContextName) {
  const char kContextName[] = "Jim";
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->context_name = std::string(kContextName);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_EQ(kContextName, network_context->url_request_context()->name());
}

TEST_F(NetworkContextTest, QuicUserAgentId) {
  const char kQuicUserAgentId[] = "007";
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->quic_user_agent_id = kQuicUserAgentId;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_EQ(kQuicUserAgentId, network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->params()
                                  .quic_user_agent_id);
}

TEST_F(NetworkContextTest, DisableDataUrlSupport) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->enable_data_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kDataScheme));
}

TEST_F(NetworkContextTest, EnableDataUrlSupport) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->enable_data_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kDataScheme));
}

TEST_F(NetworkContextTest, DisableFileUrlSupport) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->enable_file_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFileScheme));
}

#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
TEST_F(NetworkContextTest, EnableFileUrlSupport) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->enable_file_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFileScheme));
}
#endif  // !BUILDFLAG(DISABLE_FILE_SUPPORT)

TEST_F(NetworkContextTest, DisableFtpUrlSupport) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->enable_ftp_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFtpScheme));
}

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
TEST_F(NetworkContextTest, EnableFtpUrlSupport) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->enable_ftp_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFtpScheme));
}
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

TEST_F(NetworkContextTest, Http09Disabled) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->http_09_on_non_default_ports_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .http_09_on_non_default_ports_enabled);
}

TEST_F(NetworkContextTest, Http09Enabled) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->http_09_on_non_default_ports_enabled = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(network_context->url_request_context()
                  ->http_transaction_factory()
                  ->GetSession()
                  ->params()
                  .http_09_on_non_default_ports_enabled);
}

TEST_F(NetworkContextTest, DefaultHttpNetworkSessionParams) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  const net::HttpNetworkSession::Params& params =
      network_context->url_request_context()
          ->http_transaction_factory()
          ->GetSession()
          ->params();

  EXPECT_TRUE(params.enable_http2);
  EXPECT_FALSE(params.enable_quic);
  EXPECT_EQ(1350u, params.quic_max_packet_length);
  EXPECT_TRUE(params.origins_to_force_quic_on.empty());
  EXPECT_FALSE(params.enable_user_alternate_protocol_ports);
  EXPECT_FALSE(params.ignore_certificate_errors);
  EXPECT_EQ(0, params.testing_fixed_http_port);
  EXPECT_EQ(0, params.testing_fixed_https_port);
}

// Make sure that network_session_configurator is hooked up.
TEST_F(NetworkContextTest, FixedHttpPort) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestingFixedHttpPort, "800");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestingFixedHttpsPort, "801");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  const net::HttpNetworkSession::Params& params =
      network_context->url_request_context()
          ->http_transaction_factory()
          ->GetSession()
          ->params();

  EXPECT_EQ(800, params.testing_fixed_http_port);
  EXPECT_EQ(801, params.testing_fixed_https_port);
}

TEST_F(NetworkContextTest, NoCache) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->http_cache_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_transaction_factory()
                   ->GetCache());
}

TEST_F(NetworkContextTest, MemoryCache) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->http_cache_enabled = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  EXPECT_EQ(net::MEMORY_CACHE, backend->GetCacheType());
}

TEST_F(NetworkContextTest, DiskCache) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  EXPECT_EQ(net::DISK_CACHE, backend->GetCacheType());
  EXPECT_EQ(network_session_configurator::ChooseCacheType(
                *base::CommandLine::ForCurrentProcess()),
            GetBackendType(backend));
}

// This makes sure that network_session_configurator::ChooseCacheType is
// connected to NetworkContext.
TEST_F(NetworkContextTest, SimpleCache) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseSimpleCacheBackend, "on");
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  base::StringPairs stats;
  backend->GetStats(&stats);
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            GetBackendType(backend));
}

TEST_F(NetworkContextTest, HttpServerPropertiesToDisk) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo");
  EXPECT_FALSE(base::PathExists(file_path));

  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  // Create a context with on-disk storage of HTTP server properties.
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->http_server_properties_path = file_path;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk, and sanity check initial state.
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));

  // Set a property.
  network_context->url_request_context()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, true);
  // Deleting the context will cause it to flush state. Wait for the pref
  // service to flush to disk.
  network_context.reset();
  scoped_task_environment_.RunUntilIdle();

  // Create a new NetworkContext using the same path for HTTP server properties.
  context_params = network::mojom::NetworkContextParams::New();
  context_params->http_server_properties_path = file_path;
  network_context = CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(network_context->url_request_context()
                  ->http_server_properties()
                  ->GetSupportsSpdy(kSchemeHostPort));

  // Now check that ClearNetworkingHistorySince clears the data.
  base::RunLoop run_loop2;
  network_context->ClearNetworkingHistorySince(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));

  // Clear destroy the network context and let any pending writes complete
  // before destroying |temp_dir|, to avoid leaking any files.
  network_context.reset();
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_dir.Delete());
}

// Checks that ClearNetworkingHistorySince() works clears in-memory pref stores,
// and invokes the closure passed to it.
TEST_F(NetworkContextTest, ClearHttpServerPropertiesInMemory) {
  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(network::mojom::NetworkContextParams::New());

  EXPECT_FALSE(network_context->url_request_context()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));
  network_context->url_request_context()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, true);
  EXPECT_TRUE(network_context->url_request_context()
                  ->http_server_properties()
                  ->GetSupportsSpdy(kSchemeHostPort));

  base::RunLoop run_loop;
  network_context->ClearNetworkingHistorySince(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));
}

void SetCookieCallback(base::RunLoop* run_loop, bool* result_out, bool result) {
  *result_out = result;
  run_loop->Quit();
}

void GetCookieListCallback(base::RunLoop* run_loop,
                           net::CookieList* result_out,
                           const net::CookieList& result) {
  *result_out = result;
  run_loop->Quit();
}

TEST_F(NetworkContextTest, CookieManager) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(network::mojom::NetworkContextParams::New());

  network::mojom::CookieManagerPtr cookie_manager_ptr;
  network::mojom::CookieManagerRequest cookie_manager_request(
      mojo::MakeRequest(&cookie_manager_ptr));
  network_context->GetCookieManager(std::move(cookie_manager_request));

  // Set a cookie through the cookie interface.
  base::RunLoop run_loop1;
  bool result = false;
  cookie_manager_ptr->SetCanonicalCookie(
      net::CanonicalCookie("TestCookie", "1", "www.test.com", "/", base::Time(),
                           base::Time(), base::Time(), false, false,
                           net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_LOW),
      true, true, base::BindOnce(&SetCookieCallback, &run_loop1, &result));
  run_loop1.Run();
  EXPECT_TRUE(result);

  // Confirm that cookie is visible directly through the store associated with
  // the network context.
  base::RunLoop run_loop2;
  net::CookieList cookies;
  network_context->url_request_context()
      ->cookie_store()
      ->GetCookieListWithOptionsAsync(
          GURL("http://www.test.com/whatever"), net::CookieOptions(),
          base::Bind(&GetCookieListCallback, &run_loop2, &cookies));
  run_loop2.Run();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("TestCookie", cookies[0].Name());
}

TEST_F(NetworkContextTest, ProxyConfig) {
  // Create a bunch of proxy rules to switch between. All that matters is that
  // they're all different. It's important that none of these configs require
  // fetching a PAC scripts, as this test checks ProxyService::config(), which
  // is only updated after fetching PAC scripts (if applicable).
  net::ProxyConfig proxy_configs[3];
  proxy_configs[0].proxy_rules().ParseFromString("http=foopy:80");
  proxy_configs[1].proxy_rules().ParseFromString("http=foopy:80;ftp=foopy2");
  proxy_configs[2] = net::ProxyConfig::CreateDirect();

  // Sanity check.
  EXPECT_FALSE(proxy_configs[0].Equals(proxy_configs[1]));
  EXPECT_FALSE(proxy_configs[0].Equals(proxy_configs[2]));
  EXPECT_FALSE(proxy_configs[1].Equals(proxy_configs[2]));

  // Try each proxy config as the initial config, to make sure setting the
  // initial config works.
  for (const auto& initial_proxy_config : proxy_configs) {
    network::mojom::NetworkContextParamsPtr context_params =
        CreateContextParams();
    context_params->initial_proxy_config = initial_proxy_config;
    network::mojom::ProxyConfigClientPtr config_client;
    context_params->proxy_config_client_request =
        mojo::MakeRequest(&config_client);
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    net::ProxyService* proxy_service =
        network_context->url_request_context()->proxy_service();
    // Kick the ProxyService into action, as it doesn't start updating its
    // config until it's first used.
    proxy_service->ForceReloadProxyConfig();
    EXPECT_TRUE(proxy_service->config().is_valid());
    EXPECT_TRUE(proxy_service->config().Equals(initial_proxy_config));

    // Always go through the other configs in the same order. This has the
    // advantage of testing the case where there's no change, for
    // proxy_config[0].
    for (const auto& proxy_config : proxy_configs) {
      config_client->OnProxyConfigUpdated(proxy_config);
      scoped_task_environment_.RunUntilIdle();
      EXPECT_TRUE(proxy_service->config().is_valid());
      EXPECT_TRUE(proxy_service->config().Equals(proxy_config));
    }
  }
}

// Verify that a proxy config works without a ProxyConfigClientRequest.
TEST_F(NetworkContextTest, StaticProxyConfig) {
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString("http=foopy:80;ftp=foopy2");

  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->initial_proxy_config = proxy_config;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  net::ProxyService* proxy_service =
      network_context->url_request_context()->proxy_service();
  // Kick the ProxyService into action, as it doesn't start updating its
  // config until it's first used.
  proxy_service->ForceReloadProxyConfig();
  EXPECT_TRUE(proxy_service->config().is_valid());
  EXPECT_TRUE(proxy_service->config().Equals(proxy_config));
}

TEST_F(NetworkContextTest, NoInitialProxyConfig) {
  network::mojom::NetworkContextParamsPtr context_params =
      CreateContextParams();
  context_params->initial_proxy_config.reset();
  network::mojom::ProxyConfigClientPtr config_client;
  context_params->proxy_config_client_request =
      mojo::MakeRequest(&config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  net::ProxyService* proxy_service =
      network_context->url_request_context()->proxy_service();
  EXPECT_FALSE(proxy_service->config().is_valid());
  EXPECT_FALSE(proxy_service->fetched_config().is_valid());

  // Before there's a proxy configuration, proxy requests should hang.
  net::ProxyInfo proxy_info;
  net::TestCompletionCallback test_callback;
  net::ProxyService::Request* request = nullptr;
  ASSERT_EQ(net::ERR_IO_PENDING,
            proxy_service->ResolveProxy(GURL("http://bar/"), "GET", &proxy_info,
                                        test_callback.callback(), &request,
                                        nullptr, net::NetLogWithSource()));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(proxy_service->config().is_valid());
  EXPECT_FALSE(proxy_service->fetched_config().is_valid());
  ASSERT_FALSE(test_callback.have_result());

  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString("http=foopy:80");
  config_client->OnProxyConfigUpdated(proxy_config);
  ASSERT_EQ(net::OK, test_callback.WaitForResult());

  EXPECT_TRUE(proxy_info.is_http());
  EXPECT_EQ("foopy", proxy_info.proxy_server().host_port_pair().host());
}

class TestProxyConfigLazyPoller
    : public network::mojom::ProxyConfigPollerClient {
 public:
  TestProxyConfigLazyPoller() : binding_(this) {}
  ~TestProxyConfigLazyPoller() override {}

  void OnLazyProxyConfigPoll() override { ++times_polled_; }

  network::mojom::ProxyConfigPollerClientPtr BindInterface() {
    network::mojom::ProxyConfigPollerClientPtr interface;
    binding_.Bind(MakeRequest(&interface));
    return interface;
  }

  int GetAndClearTimesPolled() {
    int out = times_polled_;
    times_polled_ = 0;
    return out;
  }

 private:
  int times_polled_ = 0;
  mojo::Binding<ProxyConfigPollerClient> binding_;

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestProxyConfigLazyPoller);
};

}  // namespace
}  // namespace content
