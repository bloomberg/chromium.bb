// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ref_counted.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/test_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

// The end-to-end test exercises the synchronous APIs in ObjectProxy and
// ExportedObject. The test will launch a thread for the service side
// operations (i.e. ExportedObject side).
class EndToEndSyncTest : public testing::Test {
 public:
  EndToEndSyncTest() = default;

  void SetUp() override {
    // Start the test service;
    TestService::Options options;
    test_service_.reset(new TestService(options));
    ASSERT_TRUE(test_service_->StartService());
    test_service_->WaitUntilServiceIsStarted();
    ASSERT_FALSE(test_service_->HasDBusThread());

    // Create the client.
    Bus::Options client_bus_options;
    client_bus_options.bus_type = Bus::SESSION;
    client_bus_options.connection_type = Bus::PRIVATE;
    client_bus_ = new Bus(client_bus_options);
    object_proxy_ = client_bus_->GetObjectProxy(
        test_service_->service_name(),
        ObjectPath("/org/chromium/TestObject"));
    ASSERT_FALSE(client_bus_->HasDBusThread());
  }

  void TearDown() override {
    test_service_->ShutdownAndBlock();
    test_service_->Stop();
    client_bus_->ShutdownAndBlock();
  }

 protected:
  std::unique_ptr<TestService> test_service_;
  scoped_refptr<Bus> client_bus_;
  ObjectProxy* object_proxy_;
};

TEST_F(EndToEndSyncTest, Echo) {
  const std::string kHello = "hello";

  // Create the method call.
  MethodCall method_call("org.chromium.TestInterface", "Echo");
  MessageWriter writer(&method_call);
  writer.AppendString(kHello);

  // Call the method.
  const int timeout_ms = ObjectProxy::TIMEOUT_USE_DEFAULT;
  std::unique_ptr<Response> response(
      object_proxy_->CallMethodAndBlock(&method_call, timeout_ms));
  ASSERT_TRUE(response.get());

  // Check the response. kHello should be echoed back.
  MessageReader reader(response.get());
  std::string returned_message;
  ASSERT_TRUE(reader.PopString(&returned_message));
  EXPECT_EQ(kHello, returned_message);
}

TEST_F(EndToEndSyncTest, Timeout) {
  const std::string kHello = "hello";

  // Create the method call.
  MethodCall method_call("org.chromium.TestInterface", "DelayedEcho");
  MessageWriter writer(&method_call);
  writer.AppendString(kHello);

  // Call the method with timeout of 0ms.
  const int timeout_ms = 0;
  std::unique_ptr<Response> response(
      object_proxy_->CallMethodAndBlock(&method_call, timeout_ms));
  // Should fail because of timeout.
  ASSERT_FALSE(response.get());
}

TEST_F(EndToEndSyncTest, NonexistentMethod) {
  MethodCall method_call("org.chromium.TestInterface", "Nonexistent");

  const int timeout_ms = ObjectProxy::TIMEOUT_USE_DEFAULT;
  std::unique_ptr<Response> response(
      object_proxy_->CallMethodAndBlock(&method_call, timeout_ms));
  ASSERT_FALSE(response.get());
}

TEST_F(EndToEndSyncTest, BrokenMethod) {
  MethodCall method_call("org.chromium.TestInterface", "BrokenMethod");

  const int timeout_ms = ObjectProxy::TIMEOUT_USE_DEFAULT;
  std::unique_ptr<Response> response(
      object_proxy_->CallMethodAndBlock(&method_call, timeout_ms));
  ASSERT_FALSE(response.get());
}

TEST_F(EndToEndSyncTest, InvalidServiceName) {
  // Bus name cannot contain '/'.
  const std::string invalid_service_name = ":1/2";

  // Replace object proxy with new one.
  object_proxy_ = client_bus_->GetObjectProxy(
      invalid_service_name, ObjectPath("/org/chromium/TestObject"));

  MethodCall method_call("org.chromium.TestInterface", "Echo");

  const int timeout_ms = ObjectProxy::TIMEOUT_USE_DEFAULT;
  std::unique_ptr<Response> response(
      object_proxy_->CallMethodAndBlock(&method_call, timeout_ms));
  ASSERT_FALSE(response.get());
}

}  // namespace dbus
