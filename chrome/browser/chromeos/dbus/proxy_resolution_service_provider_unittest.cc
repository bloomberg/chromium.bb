// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test is relatively complicated. Here's the summary of what it does:
//
// - Set up mock D-Bus related objects to mock out D-Bus calls.
// - Set up a mock proxy resolver to mock out the proxy resolution.
// - Create ProxyResolutionServiceProvider by injecting the mocks
// - Start the service provider.
// - Request ProxyResolutionServiceProvider to resolve proxy for kSourceURL.
// - ProxyResolutionServiceProvider will return the result as a signal.
// - Confirm that we receive the signal and check the contents of the signal.

#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/dbus/service_provider_test_helper.h"
#include "dbus/message.h"
#include "dbus/mock_exported_object.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;

namespace chromeos {

// We want to know about the proxy info for the URL.
const char kSourceURL[] = "http://www.gmail.com/";

// ProxyResolutionServiceProvider will return the proxy info as a D-Bus
// signal, to the following signal interface and the signal name.
const char kReturnSignalInterface[] = "org.chromium.TestInterface";
const char kReturnSignalName[] = "TestSignal";

// The returned proxy info.
const char kReturnProxyInfo[] = "PROXY cache.example.com:12345";

// The error message is empty if proxy resolution is successful.
const char kReturnEmptyErrorMessage[] = "";

// Mock for ProxyResolverInterface. We'll inject this to
// ProxyResolutionServiceProvider to mock out the proxy resolution.
class MockProxyResolver : public ProxyResolverInterface {
 public:
  MOCK_METHOD4(ResolveProxy,
               void(const std::string& source_url,
                    const std::string& signal_interface,
                    const std::string& signal_name,
                    scoped_refptr<dbus::ExportedObject> exported_object));
};

class ProxyResolutionServiceProviderTest : public testing::Test {
 public:
  ProxyResolutionServiceProviderTest()
      : signal_received_successfully_(false) {
  }

  virtual void SetUp() OVERRIDE {
    // Create a mock proxy resolver. Will be owned by
    // |proxy_resolution_service|.
    MockProxyResolver* mock_resolver = new MockProxyResolver;
    // |mock_resolver_|'s ResolveProxy() will use MockResolveProxy().
    EXPECT_CALL(*mock_resolver,
                ResolveProxy(kSourceURL, kReturnSignalInterface,
                             kReturnSignalName, _))
        .WillOnce(Invoke(
            this,
            &ProxyResolutionServiceProviderTest::MockResolveProxy));

    // Create the proxy resolution service with the mock bus and the mock
    // resolver injected.
    service_provider_.reset(
        ProxyResolutionServiceProvider::CreateForTesting(mock_resolver));

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

  virtual void TearDown() OVERRIDE {
    test_helper_.TearDown();
    service_provider_.reset();
  }

 protected:
  // Called when a signal is received.
  void OnSignalReceived(dbus::Signal* signal) {
    ASSERT_EQ(kReturnSignalInterface, signal->GetInterface());
    ASSERT_EQ(kReturnSignalName, signal->GetMember());

    std::string source_url;
    std::string proxy_info;
    std::string error_message;

    // The signal should contain three strings.
    dbus::MessageReader reader(signal);
    ASSERT_TRUE(reader.PopString(&source_url));
    ASSERT_TRUE(reader.PopString(&proxy_info));
    ASSERT_TRUE(reader.PopString(&error_message));

    // Check the signal contents.
    EXPECT_EQ(kSourceURL, source_url);
    EXPECT_EQ(kReturnProxyInfo, proxy_info);
    EXPECT_EQ(kReturnEmptyErrorMessage, error_message);

    // Mark that the signal is received successfully.
    signal_received_successfully_ = true;
  }

  // Called when connected to a signal.
  void OnConnectedToSignal(const std::string& signal_interface,
                           const std::string& signal_name,
                           bool success){
    ASSERT_EQ(kReturnSignalInterface, signal_interface);
    ASSERT_EQ(kReturnSignalName, signal_name);

    ASSERT_TRUE(success);
  }

  // Behaves as |mock_resolver_|'s ResolveProxy().
  void MockResolveProxy(const std::string& source_url,
                        const std::string& signal_interface,
                        const std::string& signal_name,
                        scoped_refptr<dbus::ExportedObject> exported_object) {
    if (source_url == kSourceURL) {
      dbus::Signal signal(signal_interface,
                          signal_name);
      dbus::MessageWriter writer(&signal);
      writer.AppendString(kSourceURL);
      writer.AppendString(kReturnProxyInfo);
      writer.AppendString(kReturnEmptyErrorMessage);
      // Send the signal back to the requested signal interface and the
      // signal name.
      exported_object->SendSignal(&signal);
      return;
    }

    LOG(ERROR) << "Unexpected source URL: " << source_url;
  }

  bool signal_received_successfully_;
  ServiceProviderTestHelper test_helper_;
  scoped_ptr<CrosDBusService::ServiceProviderInterface> service_provider_;
};

TEST_F(ProxyResolutionServiceProviderTest, ResolveProxy) {
  // The signal is not yet received.
  ASSERT_FALSE(signal_received_successfully_);

  // Create a method call to resolve proxy config for kSourceURL.
  dbus::MethodCall method_call(kLibCrosServiceInterface, kResolveNetworkProxy);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kSourceURL);
  writer.AppendString(kReturnSignalInterface);
  writer.AppendString(kReturnSignalName);

  // Call the ResolveNetworkProxy method.
  scoped_ptr<dbus::Response> response(test_helper_.CallMethod(&method_call));

  // An empty response should be returned.
  ASSERT_TRUE(response.get());
  dbus::MessageReader reader(response.get());
  ASSERT_FALSE(reader.HasMoreData());

  // Confirm that the signal is received successfully.
  // The contents of the signal are checked in OnSignalReceived().
  ASSERT_TRUE(signal_received_successfully_);
}

}  // namespace chromeos
