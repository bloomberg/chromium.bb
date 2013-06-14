// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/service_provider_test_helper.h"

#include "base/bind.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Invoke;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::Unused;

namespace chromeos {

ServiceProviderTestHelper::ServiceProviderTestHelper()
    : response_received_(false) {
}

ServiceProviderTestHelper::~ServiceProviderTestHelper() {
}

void ServiceProviderTestHelper::SetUp(
    const std::string& exported_method_name,
    CrosDBusService::ServiceProviderInterface* service_provider) {
  // Create a mock bus.
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  mock_bus_ = new dbus::MockBus(options);

  // ShutdownAndBlock() will be called in TearDown().
  EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

  // Create a mock exported object that behaves as
  // org.chromium.CrosDBusService.
  mock_exported_object_ =
      new dbus::MockExportedObject(mock_bus_.get(),
                                   dbus::ObjectPath(kLibCrosServicePath));

  // |mock_exported_object_|'s ExportMethod() will use
  // |MockExportedObject().
  EXPECT_CALL(
      *mock_exported_object_.get(),
      ExportMethod(kLibCrosServiceInterface, exported_method_name, _, _))
      .WillOnce(Invoke(this, &ServiceProviderTestHelper::MockExportMethod));

  // Create a mock object proxy, with which we call a method of
  // |mock_exported_object_|.
  mock_object_proxy_ =
      new dbus::MockObjectProxy(mock_bus_.get(),
                                kLibCrosServiceName,
                                dbus::ObjectPath(kLibCrosServicePath));
  // |mock_object_proxy_|'s MockCallMethodAndBlock() will use
  // MockCallMethodAndBlock() to return responses.
  EXPECT_CALL(*mock_object_proxy_.get(),
              MockCallMethodAndBlock(
                  AllOf(ResultOf(std::mem_fun(&dbus::MethodCall::GetInterface),
                                 kLibCrosServiceInterface),
                        ResultOf(std::mem_fun(&dbus::MethodCall::GetMember),
                                 exported_method_name)),
                  _))
      .WillOnce(
           Invoke(this, &ServiceProviderTestHelper::MockCallMethodAndBlock));

  service_provider->Start(mock_exported_object_.get());
}

void ServiceProviderTestHelper::TearDown() {
  mock_bus_->ShutdownAndBlock();
  mock_exported_object_ = NULL;
  mock_object_proxy_ = NULL;
  mock_bus_ = NULL;
}

void ServiceProviderTestHelper::SetUpReturnSignal(
    const std::string& interface_name,
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback signal_callback,
    dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
  // |mock_exported_object_|'s SendSignal() will use
  // MockSendSignal().
  EXPECT_CALL(*mock_exported_object_.get(), SendSignal(_))
      .WillOnce(Invoke(this, &ServiceProviderTestHelper::MockSendSignal));

  // |mock_object_proxy_|'s ConnectToSignal will use
  // MockConnectToSignal().
  EXPECT_CALL(*mock_object_proxy_.get(),
              ConnectToSignal(interface_name, signal_name, _, _))
      .WillOnce(Invoke(this, &ServiceProviderTestHelper::MockConnectToSignal));

  mock_object_proxy_->ConnectToSignal(interface_name, signal_name,
                                      signal_callback, on_connected_callback);
}

scoped_ptr<dbus::Response> ServiceProviderTestHelper::CallMethod(
    dbus::MethodCall* method_call) {
  return mock_object_proxy_->CallMethodAndBlock(
      method_call,
      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
}

void ServiceProviderTestHelper::MockExportMethod(
    const std::string& interface_name,
    const std::string& method_name,
    dbus::ExportedObject::MethodCallCallback method_callback,
    dbus::ExportedObject::OnExportedCallback on_exported_callback) {
  // Tell the call back that the method is exported successfully.
  on_exported_callback.Run(interface_name, method_name, true);
  // Capture the callback, so we can run this at a later time.
  method_callback_ = method_callback;
}

dbus::Response* ServiceProviderTestHelper::MockCallMethodAndBlock(
    dbus::MethodCall* method_call,
    Unused) {
  // Set the serial number to non-zero, so
  // dbus_message_new_method_return() won't emit a warning.
  method_call->SetSerial(1);
  // Run the callback captured in MockExportMethod(). In addition to returning
  // a response that the caller will ignore, this will send a signal, which
  // will be received by |on_signal_callback_|.
  method_callback_.Run(method_call,
                       base::Bind(&ServiceProviderTestHelper::OnResponse,
                                  base::Unretained(this)));
  // Check for a response.
  if (!response_received_)
    message_loop_.Run();
  // Return response.
  return response_.release();
}

void ServiceProviderTestHelper::MockConnectToSignal(
    const std::string& interface_name,
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback signal_callback,
    dbus::ObjectProxy::OnConnectedCallback connected_callback) {
  // Tell the callback that the object proxy is connected to the signal.
  connected_callback.Run(interface_name, signal_name, true);
  // Capture the callback, so we can run this at a later time.
  on_signal_callback_ = signal_callback;
}

void ServiceProviderTestHelper::MockSendSignal(dbus::Signal* signal) {
  // Run the callback captured in MockConnectToSignal(). This will call
  // OnSignalReceived().
  on_signal_callback_.Run(signal);
}

void ServiceProviderTestHelper::OnResponse(
    scoped_ptr<dbus::Response> response) {
  response_ = response.Pass();
  response_received_ = true;
  if (message_loop_.is_running())
    message_loop_.Quit();
}

}  // namespace chromeos
