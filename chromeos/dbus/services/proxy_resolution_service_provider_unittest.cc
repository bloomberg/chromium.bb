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
#include "net/url_request/url_request_test_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// ProxyResolutionServiceProvider will return the proxy info as a D-Bus
// signal, to the following signal interface and the signal name.
const char kReturnSignalInterface[] = "org.chromium.TestInterface";
const char kReturnSignalName[] = "TestSignal";

// Test ProxyResolverDelegate implementation.
class TestProxyResolverDelegate : public ProxyResolverDelegate {
public:
  explicit TestProxyResolverDelegate(
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
      : request_context_getter_(
            new net::TestURLRequestContextGetter(network_task_runner)) {}

  ~TestProxyResolverDelegate() override {}

  // ProxyResolverDelegate override.
  scoped_refptr<net::URLRequestContextGetter> GetRequestContext() override {
    return request_context_getter_;
  }

 private:
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(TestProxyResolverDelegate);
};

}  // namespace

class ProxyResolutionServiceProviderTest : public testing::Test {
 public:
  void SetUp() override {
    // Create the proxy resolution service with the mock bus and the mock
    // resolver injected.
    service_provider_.reset(ProxyResolutionServiceProvider::Create(
        base::MakeUnique<TestProxyResolverDelegate>(
            base::ThreadTaskRunnerHandle::Get())));

    test_helper_.SetUp(kResolveNetworkProxy, service_provider_.get());

    // Connect to the signal that will be sent to kReturnSignalInterface and
    // kReturnSignalName. ResolveNetworkProxy() will send the result as a
    // signal. OnSignalReceived() will be called upon the delivery.
    test_helper_.SetUpReturnSignal(
        kReturnSignalInterface,
        kReturnSignalName,
        base::Bind(&ProxyResolutionServiceProviderTest::OnSignalReceived,
                   base::Unretained(this)),
        base::Bind(&ProxyResolutionServiceProviderTest::OnConnectedToSignal,
                   base::Unretained(this)));
  }

  void TearDown() override {
    test_helper_.TearDown();
    service_provider_.reset();
  }

 protected:
  // Called when a signal is received.
  void OnSignalReceived(dbus::Signal* signal) {
    EXPECT_EQ(kReturnSignalInterface, signal->GetInterface());
    EXPECT_EQ(kReturnSignalName, signal->GetMember());

    // The signal should contain three strings.
    dbus::MessageReader reader(signal);
    EXPECT_TRUE(reader.PopString(&source_url_));
    EXPECT_TRUE(reader.PopString(&proxy_info_));
    EXPECT_TRUE(reader.PopString(&error_message_));
  }

  // Called when connected to a signal.
  void OnConnectedToSignal(const std::string& signal_interface,
                           const std::string& signal_name,
                           bool success){
    EXPECT_EQ(kReturnSignalInterface, signal_interface);
    EXPECT_EQ(kReturnSignalName, signal_name);

    EXPECT_TRUE(success);
  }

  std::string source_url_;
  std::string proxy_info_;
  std::string error_message_;
  ServiceProviderTestHelper test_helper_;
  std::unique_ptr<CrosDBusService::ServiceProviderInterface> service_provider_;
};

TEST_F(ProxyResolutionServiceProviderTest, ResolveProxy) {
  const char kSourceURL[] = "http://www.gmail.com/";

  // Create a method call to resolve proxy config for kSourceURL.
  dbus::MethodCall method_call(kLibCrosServiceInterface, kResolveNetworkProxy);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kSourceURL);
  writer.AppendString(kReturnSignalInterface);
  writer.AppendString(kReturnSignalName);

  // Call the ResolveNetworkProxy method.
  std::unique_ptr<dbus::Response> response(
      test_helper_.CallMethod(&method_call));
  base::RunLoop().RunUntilIdle();

  // An empty response should be returned.
  ASSERT_TRUE(response.get());
  dbus::MessageReader reader(response.get());
  EXPECT_FALSE(reader.HasMoreData());

  // Confirm that the signal is received successfully.
  EXPECT_EQ(kSourceURL, source_url_);
  EXPECT_EQ("DIRECT", proxy_info_);
  EXPECT_EQ("", error_message_);
}

}  // namespace chromeos
