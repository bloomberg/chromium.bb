// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/services/proxy_resolution_service_provider.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/services/service_provider_test_helper.h"
#include "dbus/message.h"
#include "net/base/net_errors.h"
#include "net/proxy/mock_proxy_resolver.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// ProxyResolutionServiceProvider will return the proxy info as a D-Bus
// signal, to the following signal interface and the signal name.
const char kReturnSignalInterface[] = "org.chromium.TestInterface";
const char kReturnSignalName[] = "TestSignal";

// Trivial net::ProxyResolver implementation that returns canned data either
// synchronously or asynchronously.
class TestProxyResolver : public net::ProxyResolver {
 public:
  explicit TestProxyResolver(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
      : network_task_runner_(network_task_runner) {
    proxy_info_.UseDirect();
  }
  ~TestProxyResolver() override = default;

  const net::ProxyInfo& proxy_info() const { return proxy_info_; }
  net::ProxyInfo* mutable_proxy_info() { return &proxy_info_; }

  void set_async(bool async) { async_ = async; }

  // net::ProxyResolver:
  int GetProxyForURL(const GURL& url,
                     net::ProxyInfo* results,
                     const net::CompletionCallback& callback,
                     std::unique_ptr<Request>* request,
                     const net::NetLogWithSource& net_log) override {
    CHECK(network_task_runner_->BelongsToCurrentThread());
    results->Use(proxy_info_);
    if (!async_)
      return net::OK;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, net::OK));
    return net::ERR_IO_PENDING;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Proxy info for GetProxyForURL() to return.
  net::ProxyInfo proxy_info_;

  // If true, GetProxyForURL() replies asynchronously rather than synchronously.
  bool async_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestProxyResolver);
};

// Trivial net::ProxyResolverFactory implementation that synchronously creates
// net::ForwardingProxyResolvers that forward to a single passed-in resolver.
class TestProxyResolverFactory : public net::ProxyResolverFactory {
 public:
  // Ownership of |resolver| remains with the caller. |resolver| must outlive
  // the forwarding resolvers returned by CreateProxyResolver().
  explicit TestProxyResolverFactory(net::ProxyResolver* resolver)
      : net::ProxyResolverFactory(false /* expects_pac_bytes */),
        resolver_(resolver) {}
  ~TestProxyResolverFactory() override = default;

  // net::ProxyResolverFactory:
  int CreateProxyResolver(
      const scoped_refptr<net::ProxyResolverScriptData>& pac_script,
      std::unique_ptr<net::ProxyResolver>* resolver,
      const net::CompletionCallback& callback,
      std::unique_ptr<Request>* request) override {
    *resolver = base::MakeUnique<net::ForwardingProxyResolver>(resolver_);
    return net::OK;
  }

 private:
  net::ProxyResolver* resolver_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TestProxyResolverFactory);
};

// Test ProxyResolutionServiceProvider::Delegate implementation.
class TestDelegate : public ProxyResolutionServiceProvider::Delegate {
 public:
  explicit TestDelegate(
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner,
      net::ProxyResolver* proxy_resolver)
      : proxy_resolver_(proxy_resolver),
        context_getter_(
            new net::TestURLRequestContextGetter(network_task_runner)) {
    // The config's autodetect property needs to be set in order for
    // net::ProxyService to send requests to our resolver.
    net::ProxyConfig config = net::ProxyConfig::CreateAutoDetect();
    proxy_service_ = base::MakeUnique<net::ProxyService>(
        base::MakeUnique<net::ProxyConfigServiceFixed>(config),
        base::MakeUnique<TestProxyResolverFactory>(proxy_resolver_),
        nullptr /* net_log */);
    context_getter_->GetURLRequestContext()->set_proxy_service(
        proxy_service_.get());
  }
  ~TestDelegate() override = default;

  // ProxyResolutionServiceProvider::Delegate:
  scoped_refptr<net::URLRequestContextGetter> GetRequestContext() override {
    return context_getter_;
  }

 private:
  net::ProxyResolver* proxy_resolver_;  // Not owned.
  std::unique_ptr<net::ProxyService> proxy_service_;
  scoped_refptr<net::TestURLRequestContextGetter> context_getter_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class ProxyResolutionServiceProviderTest : public testing::Test {
 public:
  ProxyResolutionServiceProviderTest() = default;
  ~ProxyResolutionServiceProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    proxy_resolver_ = base::MakeUnique<TestProxyResolver>(
        base::ThreadTaskRunnerHandle::Get());
    service_provider_ = base::MakeUnique<ProxyResolutionServiceProvider>(
        base::MakeUnique<TestDelegate>(base::ThreadTaskRunnerHandle::Get(),
                                       proxy_resolver_.get()));
    test_helper_.SetUp(kResolveNetworkProxy, service_provider_.get());
  }
  void TearDown() override {
    test_helper_.TearDown();
  }

 protected:
  // Arguments extracted from a D-Bus signal.
  struct SignalInfo {
    std::string source_url;
    std::string proxy_info;
    std::string error_message;
  };

  // Called when a signal is received.
  void OnSignalReceived(dbus::Signal* signal) {
    EXPECT_EQ(kReturnSignalInterface, signal->GetInterface());
    EXPECT_EQ(kReturnSignalName, signal->GetMember());

    ASSERT_FALSE(signal_);
    signal_ = base::MakeUnique<SignalInfo>();

    // The signal should contain three strings.
    dbus::MessageReader reader(signal);
    EXPECT_TRUE(reader.PopString(&signal_->source_url));
    EXPECT_TRUE(reader.PopString(&signal_->proxy_info));
    EXPECT_TRUE(reader.PopString(&signal_->error_message));
  }

  // Called when connected to a signal.
  void OnConnectedToSignal(const std::string& signal_interface,
                           const std::string& signal_name,
                           bool success){
    EXPECT_EQ(kReturnSignalInterface, signal_interface);
    EXPECT_EQ(kReturnSignalName, signal_name);
    EXPECT_TRUE(success);
  }

  // Makes a D-Bus call to |service_provider_|'s ResolveNetworkProxy method. If
  // |request_signal| is true, requests that the proxy information be returned
  // via a signal; otherwise it should be included in the response.
  // |response_out| is updated to hold the response, and |signal_out| is updated
  // to hold information about the emitted signal, if any.
  void CallMethod(const std::string& source_url,
                  bool request_signal,
                  std::unique_ptr<dbus::Response>* response_out,
                  std::unique_ptr<SignalInfo>* signal_out) {
    dbus::MethodCall method_call(kLibCrosServiceInterface,
                                 kResolveNetworkProxy);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(source_url);
    if (request_signal) {
      writer.AppendString(kReturnSignalInterface);
      writer.AppendString(kReturnSignalName);

      // Connect to the signal that will be sent to kReturnSignalInterface and
      // kReturnSignalName. ResolveNetworkProxy() will send the result as a
      // signal. OnSignalReceived() will be called upon the delivery.
      test_helper_.SetUpReturnSignal(
          kReturnSignalInterface, kReturnSignalName,
          base::Bind(&ProxyResolutionServiceProviderTest::OnSignalReceived,
                     base::Unretained(this)),
          base::Bind(&ProxyResolutionServiceProviderTest::OnConnectedToSignal,
                     base::Unretained(this)));
    }

    *response_out = test_helper_.CallMethod(&method_call);
    base::RunLoop().RunUntilIdle();
    *signal_out = std::move(signal_);
  }

  // Information about the last D-Bus signal received by OnSignalReceived().
  std::unique_ptr<SignalInfo> signal_;

  std::unique_ptr<TestProxyResolver> proxy_resolver_;
  std::unique_ptr<ProxyResolutionServiceProvider> service_provider_;
  ServiceProviderTestHelper test_helper_;
};

// Tests that synchronously-resolved proxy information is returned via a signal.
TEST_F(ProxyResolutionServiceProviderTest, SignalSync) {
  const char kSourceURL[] = "http://www.gmail.com/";

  std::unique_ptr<dbus::Response> response;
  std::unique_ptr<SignalInfo> signal;
  CallMethod(kSourceURL, true /* request_signal */, &response, &signal);

  // An empty response should be returned.
  ASSERT_TRUE(response);
  EXPECT_FALSE(dbus::MessageReader(response.get()).HasMoreData());

  // Confirm that the signal is received successfully.
  ASSERT_TRUE(signal);
  EXPECT_EQ(kSourceURL, signal->source_url);
  EXPECT_EQ(proxy_resolver_->proxy_info().ToPacString(), signal->proxy_info);
  EXPECT_EQ("", signal->error_message);
}

// Tests that asynchronously-resolved proxy information is returned via a
// signal.
TEST_F(ProxyResolutionServiceProviderTest, SignalAsync) {
  const char kSourceURL[] = "http://www.gmail.com/";
  proxy_resolver_->set_async(true);
  proxy_resolver_->mutable_proxy_info()->UseNamedProxy("http://localhost:8080");

  std::unique_ptr<dbus::Response> response;
  std::unique_ptr<SignalInfo> signal;
  CallMethod(kSourceURL, true /* request_signal */, &response, &signal);

  // An empty response should be returned.
  ASSERT_TRUE(response);
  EXPECT_FALSE(dbus::MessageReader(response.get()).HasMoreData());

  // Confirm that the signal is received successfully.
  ASSERT_TRUE(signal);
  EXPECT_EQ(kSourceURL, signal->source_url);
  EXPECT_EQ(proxy_resolver_->proxy_info().ToPacString(), signal->proxy_info);
  EXPECT_EQ("", signal->error_message);
}

TEST_F(ProxyResolutionServiceProviderTest, ResponseSync) {
  const char kSourceURL[] = "http://www.gmail.com/";
  std::unique_ptr<dbus::Response> response;
  std::unique_ptr<SignalInfo> signal;
  CallMethod(kSourceURL, false /* request_signal */, &response, &signal);

  // The response should contain the proxy info and an empty error.
  ASSERT_TRUE(response);
  dbus::MessageReader reader(response.get());
  std::string proxy_info, error;
  EXPECT_TRUE(reader.PopString(&proxy_info));
  EXPECT_TRUE(reader.PopString(&error));
  EXPECT_EQ(proxy_resolver_->proxy_info().ToPacString(), proxy_info);
  EXPECT_EQ("", error);

  // No signal should've been emitted.
  EXPECT_FALSE(signal);
}

TEST_F(ProxyResolutionServiceProviderTest, ResponseAsync) {
  const char kSourceURL[] = "http://www.gmail.com/";
  proxy_resolver_->set_async(true);
  proxy_resolver_->mutable_proxy_info()->UseNamedProxy("http://localhost:8080");
  std::unique_ptr<dbus::Response> response;
  std::unique_ptr<SignalInfo> signal;
  CallMethod(kSourceURL, false /* request_signal */, &response, &signal);

  // The response should contain the proxy info and an empty error.
  ASSERT_TRUE(response);
  dbus::MessageReader reader(response.get());
  std::string proxy_info, error;
  EXPECT_TRUE(reader.PopString(&proxy_info));
  EXPECT_TRUE(reader.PopString(&error));
  EXPECT_EQ(proxy_resolver_->proxy_info().ToPacString(), proxy_info);
  EXPECT_EQ("", error);

  // No signal should've been emitted.
  EXPECT_FALSE(signal);
}

}  // namespace chromeos
