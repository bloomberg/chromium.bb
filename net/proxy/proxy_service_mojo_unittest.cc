// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service_mojo.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/load_flags.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log.h"
#include "net/proxy/dhcp_proxy_script_fetcher.h"
#include "net/proxy/in_process_mojo_proxy_resolver_factory.h"
#include "net/proxy/mock_proxy_script_fetcher.h"
#include "net/proxy/mojo_proxy_resolver_factory.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/test/event_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kPacUrl[] = "http://example.com/proxy.pac";
const char kSimplePacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  return 'PROXY foo:1234';\n"
    "}";
const char kDnsResolvePacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  if (dnsResolveEx('example.com') != '1.2.3.4')\n"
    "    return 'DIRECT';\n"
    "  return 'QUIC bar:4321';\n"
    "}";
const char kErrorPacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  throw new Error('test error');\n"
    "}";

class TestNetworkDelegate : public NetworkDelegateImpl {
 public:
  enum Event {
    PAC_SCRIPT_ERROR,
  };

  EventWaiter<Event>& event_waiter() { return event_waiter_; }

  void OnPACScriptError(int line_number, const base::string16& error) override;

 private:
  EventWaiter<Event> event_waiter_;
};

void TestNetworkDelegate::OnPACScriptError(int line_number,
                                           const base::string16& error) {
  event_waiter_.NotifyEvent(PAC_SCRIPT_ERROR);
  EXPECT_EQ(2, line_number);
  EXPECT_TRUE(base::UTF16ToUTF8(error).find("test error") != std::string::npos);
}

}  // namespace

class ProxyServiceMojoTest : public testing::Test,
                             public MojoProxyResolverFactory {
 protected:
  void SetUp() override {
    mock_host_resolver_.rules()->AddRule("example.com", "1.2.3.4");

    fetcher_ = new MockProxyScriptFetcher;
    proxy_service_.reset(CreateProxyServiceUsingMojoFactory(
        this, new ProxyConfigServiceFixed(
                  ProxyConfig::CreateFromCustomPacURL(GURL(kPacUrl))),
        fetcher_, new DoNothingDhcpProxyScriptFetcher(), &mock_host_resolver_,
        nullptr /* NetLog* */, &network_delegate_));
  }

  scoped_ptr<base::ScopedClosureRunner> CreateResolver(
      const mojo::String& pac_script,
      mojo::InterfaceRequest<interfaces::ProxyResolver> req,
      interfaces::HostResolverPtr host_resolver,
      interfaces::ProxyResolverErrorObserverPtr error_observer,
      interfaces::ProxyResolverFactoryRequestClientPtr client) override {
    InProcessMojoProxyResolverFactory::GetInstance()->CreateResolver(
        pac_script, req.Pass(), host_resolver.Pass(), error_observer.Pass(),
        client.Pass());
    return make_scoped_ptr(
        new base::ScopedClosureRunner(on_delete_closure_.closure()));
  }

  TestNetworkDelegate network_delegate_;
  MockHostResolver mock_host_resolver_;
  MockProxyScriptFetcher* fetcher_;  // Owned by |proxy_service_|.
  scoped_ptr<ProxyService> proxy_service_;
  TestClosure on_delete_closure_;
};

TEST_F(ProxyServiceMojoTest, Basic) {
  ProxyInfo info;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            proxy_service_->ResolveProxy(GURL("http://foo"), LOAD_NORMAL, &info,
                                         callback.callback(), nullptr, nullptr,
                                         BoundNetLog()));

  // Proxy script fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());
  fetcher_->NotifyFetchCompletion(OK, kSimplePacScript);

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_EQ("PROXY foo:1234", info.ToPacString());
  EXPECT_EQ(0u, mock_host_resolver_.num_resolve());
  proxy_service_.reset();
  on_delete_closure_.WaitForResult();
}

TEST_F(ProxyServiceMojoTest, DnsResolution) {
  ProxyInfo info;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            proxy_service_->ResolveProxy(GURL("http://foo"), LOAD_NORMAL, &info,
                                         callback.callback(), nullptr, nullptr,
                                         BoundNetLog()));

  // Proxy script fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());
  fetcher_->NotifyFetchCompletion(OK, kDnsResolvePacScript);

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_EQ("QUIC bar:4321", info.ToPacString());
  EXPECT_EQ(1u, mock_host_resolver_.num_resolve());
  proxy_service_.reset();
  on_delete_closure_.WaitForResult();
}

TEST_F(ProxyServiceMojoTest, Error) {
  ProxyInfo info;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            proxy_service_->ResolveProxy(GURL("http://foo"), LOAD_NORMAL, &info,
                                         callback.callback(), nullptr, nullptr,
                                         BoundNetLog()));

  // Proxy script fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());
  fetcher_->NotifyFetchCompletion(OK, kErrorPacScript);

  network_delegate_.event_waiter().WaitForEvent(
      TestNetworkDelegate::PAC_SCRIPT_ERROR);
}

}  // namespace net
