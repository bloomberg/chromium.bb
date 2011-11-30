// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Unused;

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
      : signal_received_successfully_(false),
        mock_resolver_(NULL),
        response_received_(false) {
  }

  virtual void SetUp() {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_, ShutdownAndBlock()).WillOnce(Return());

    // Create a mock exported object that behaves as
    // org.chromium.CrosDBusService.
    mock_exported_object_ =
        new dbus::MockExportedObject(mock_bus_.get(),
                                     kLibCrosServiceName,
                                     kLibCrosServicePath);

    // |mock_exported_object_|'s ExportMethod() will use
    // |MockExportedObject().
    EXPECT_CALL(
        *mock_exported_object_,
        ExportMethod(_, _, _, _)).WillOnce(
            Invoke(this,
                   &ProxyResolutionServiceProviderTest::MockExportMethod));
    // |mock_exported_object_|'s SendSignal() will use
    // |MockSendSignal().
    EXPECT_CALL(
        *mock_exported_object_,
        SendSignal(_)).WillOnce(
            Invoke(this,
                   &ProxyResolutionServiceProviderTest::MockSendSignal));

    // Create a mock object proxy, with which we call a method of
    // |mock_exported_object_|.
    mock_object_proxy_ =
        new dbus::MockObjectProxy(mock_bus_.get(),
                                  kLibCrosServiceName,
                                  kLibCrosServicePath);
    // |mock_object_proxy_|'s CallMethodAndBlock() will use
    // MockCallMethodAndBlock() to return responses.
    EXPECT_CALL(*mock_object_proxy_,
                CallMethodAndBlock(_, _))
        .WillOnce(Invoke(
            this,
            &ProxyResolutionServiceProviderTest::MockCallMethodAndBlock));
    // |mock_object_proxy_|'s ConnectToSignal will use
    // MockConnectToSignal().
    EXPECT_CALL(*mock_object_proxy_,
                ConnectToSignal(kReturnSignalInterface,
                                kReturnSignalName,
                                _, _))
        .WillOnce(Invoke(
            this,
            &ProxyResolutionServiceProviderTest::MockConnectToSignal));

    // Create a mock proxy resolver. Will be owned by
    // |proxy_resolution_service|.
    mock_resolver_ = new MockProxyResolver;
    // |mock_resolver_|'s ResolveProxy() will use MockResolveProxy().
    EXPECT_CALL(*mock_resolver_, ResolveProxy(kSourceURL,
                                              kReturnSignalInterface,
                                              kReturnSignalName,
                                              _))
        .WillOnce(Invoke(
            this,
            &ProxyResolutionServiceProviderTest::MockResolveProxy));

    // Create the proxy resolution service with the mock bus and the mock
    // resolver injected.
    proxy_resolution_service_.reset(
        ProxyResolutionServiceProvider::CreateForTesting(mock_resolver_));

    // Finally, start the service.
    proxy_resolution_service_->Start(mock_exported_object_);
  }

  virtual void TearDown() {
    mock_bus_->ShutdownAndBlock();
  }

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

    // Check the signal conetnts.
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

 protected:
  bool signal_received_successfully_;
  MockProxyResolver* mock_resolver_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  scoped_refptr<dbus::MockObjectProxy> mock_object_proxy_;
  scoped_ptr<ProxyResolutionServiceProvider> proxy_resolution_service_;
  dbus::ExportedObject::MethodCallCallback resolve_network_proxy_;
  dbus::ObjectProxy::SignalCallback on_signal_callback_;
  MessageLoop message_loop_;
  bool response_received_;
  scoped_ptr<dbus::Response> response_;

 private:
  // Behaves as |mock_exported_object_|'s ExportMethod().
  void MockExportMethod(
      const std::string& interface_name,
      const std::string& method_name,
      dbus::ExportedObject::MethodCallCallback method_callback,
      dbus::ExportedObject::OnExportedCallback on_exported_callback) {
    if (interface_name == kLibCrosServiceInterface &&
        method_name == kResolveNetworkProxy) {
      // Tell the call back that the method is exported successfully.
      on_exported_callback.Run(interface_name, method_name, true);
      // Capture the callback, so we can run this at a later time.
      resolve_network_proxy_ = method_callback;
      return;
    }

    LOG(ERROR) << "Unexpected method exported: " << interface_name
               << method_name;
  }

  // Behaves as |mock_exported_object_|'s SendSignal().
  void MockSendSignal(dbus::Signal* signal) {
    ASSERT_EQ(kReturnSignalInterface, signal->GetInterface());
    ASSERT_EQ(kReturnSignalName, signal->GetMember());

    // Run the callback captured in MockConnectToSignal(). This will call
    // OnSignalReceived().
    on_signal_callback_.Run(signal);
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

  // Calls exported method and waits for a response for |mock_object_proxy_|.
  dbus::Response* MockCallMethodAndBlock(
      dbus::MethodCall* method_call,
      Unused) {
    if (method_call->GetInterface() != kLibCrosServiceInterface ||
        method_call->GetMember() != kResolveNetworkProxy) {
      LOG(ERROR) << "Unexpected method call: " << method_call->ToString();
      return NULL;
    }
    // Set the serial number to non-zero, so
    // dbus_message_new_method_return() won't emit a warning.
    method_call->SetSerial(1);
    // Run the callback captured in MockExportMethod(). In addition to returning
    // a response that the caller will ignore, this will send a signal, which
    // will be received by |on_signal_callback_|.
    resolve_network_proxy_.Run(
        method_call,
        base::Bind(&ProxyResolutionServiceProviderTest::OnResponse,
                   base::Unretained(this)));
    // Wait for a response.
    while (!response_received_) {
      message_loop_.Run();
    }
    // Return response.
    return response_.release();
  }

  // Receives a response and makes it available to MockCallMethodAndBlock().
  void OnResponse(dbus::Response* response) {
    response_.reset(response);
    response_received_ = true;
    if (message_loop_.is_running()) {
        message_loop_.Quit();
    }
  }

  // Behaves as |mock_object_proxy_|'s ConnectToSignal().
  void MockConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback connected_callback) {
    // Tell the callback that the object proxy is connected to the signal.
    connected_callback.Run(interface_name, signal_name, true);
    // Capture the callback, so we can run this at a later time.
    on_signal_callback_ = signal_callback;
  }
};

TEST_F(ProxyResolutionServiceProviderTest, ResolveProxy) {
  // Connect to the signal that will be sent to kReturnSignalInterface and
  // kReturnSignalName. ResolveNetworkProxy() will send the result as a
  // signal. OnSignalReceived() will be called upon the delivery.
  mock_object_proxy_->ConnectToSignal(
      kReturnSignalInterface,
      kReturnSignalName,
      base::Bind(&ProxyResolutionServiceProviderTest::OnSignalReceived,
                 base::Unretained(this)),
      base::Bind(&ProxyResolutionServiceProviderTest::OnConnectedToSignal,
                 base::Unretained(this)));
  // The signal is not yet received.
  ASSERT_FALSE(signal_received_successfully_);

  // Create a method call to resolve proxy config for kSourceURL.
  dbus::MethodCall method_call(kLibCrosServiceInterface,
                               kResolveNetworkProxy);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kSourceURL);
  writer.AppendString(kReturnSignalInterface);
  writer.AppendString(kReturnSignalName);

  // Call the ResolveNetworkProxy method.
  scoped_ptr<dbus::Response> response(
      mock_object_proxy_->CallMethodAndBlock(
          &method_call,
          dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));

  // An empty response should be returned.
  ASSERT_TRUE(response.get());
  dbus::MessageReader reader(response.get());
  ASSERT_FALSE(reader.HasMoreData());

  // Confirm that the signal is received successfully.
  // The contents of the signal are checked in OnSignalReceived().
  ASSERT_TRUE(signal_received_successfully_);
}

}  // namespace chromeos
