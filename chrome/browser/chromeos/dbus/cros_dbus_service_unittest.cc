// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

class MockProxyResolutionService
    : public CrosDBusService::ServiceProviderInterface {
 public:
  MOCK_METHOD1(Start, void(scoped_refptr<dbus::ExportedObject>
                           exported_object));
};

class CrosDBusServiceTest : public testing::Test {
 public:
  CrosDBusServiceTest() {
  }

  // Creates an instance of CrosDBusService with mocks injected.
  virtual void SetUp() {
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

    // |mock_bus_|'s GetExportedObject() will return mock_exported_object_|
    // for the given service name and the object path.
    EXPECT_CALL(*mock_bus_.get(),
                GetExportedObject(dbus::ObjectPath(kLibCrosServicePath)))
        .WillOnce(Return(mock_exported_object_.get()));

    // Create a mock proxy resolution service.
    MockProxyResolutionService* mock_proxy_resolution_service_provider =
        new MockProxyResolutionService;

    // Start() will be called with |mock_exported_object_|.
    EXPECT_CALL(*mock_proxy_resolution_service_provider,
                Start(Eq(mock_exported_object_))).WillOnce(Return());
    // Initialize the cros service with the mocks injected.
    CrosDBusService::InitializeForTesting(
        mock_bus_.get(), mock_proxy_resolution_service_provider);
  }

  virtual void TearDown() {
    // Shutdown the cros service.
    CrosDBusService::Shutdown();

    // Shutdown the bus.
    mock_bus_->ShutdownAndBlock();
  }

 protected:
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
};

TEST_F(CrosDBusServiceTest, Start) {
  // Simply start the service and see if mock expectations are met:
  // - The service object is exported by GetExportedObject()
  // - The proxy resolution service is started.
}

}  // namespace chromeos
