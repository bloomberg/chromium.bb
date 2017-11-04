// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/power/arc_power_bridge.h"

#include "base/test/scoped_task_environment.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/power.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using device::mojom::WakeLockType;

class ArcPowerBridgeTest : public testing::Test {
 public:
  ArcPowerBridgeTest() {
    auto wake_lock_provider_ptr =
        std::make_unique<device::TestWakeLockProvider>();
    wake_lock_provider_ = wake_lock_provider_ptr.get();

    connector_factory_ =
        std::make_unique<service_manager::TestConnectorFactory>(
            std::move(wake_lock_provider_ptr));
    connector_ = connector_factory_->CreateConnector();

    bridge_service_ = std::make_unique<ArcBridgeService>();
    power_bridge_ = std::make_unique<ArcPowerBridge>(nullptr /* context */,
                                                     bridge_service_.get());
    power_bridge_->set_connector_for_test(connector_.get());
  }

  ~ArcPowerBridgeTest() override = default;

 protected:
  // Acquires or releases a display wake lock of type |type|.
  void AcquireDisplayWakeLock(mojom::DisplayWakeLockType type) {
    power_bridge_->OnAcquireDisplayWakeLock(type);
    power_bridge_->FlushWakeLocksForTesting();
  }
  void ReleaseDisplayWakeLock(mojom::DisplayWakeLockType type) {
    power_bridge_->OnReleaseDisplayWakeLock(type);
    power_bridge_->FlushWakeLocksForTesting();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  device::TestWakeLockProvider* wake_lock_provider_;

  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<ArcPowerBridge> power_bridge_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcPowerBridgeTest);
};

TEST_F(ArcPowerBridgeTest, DifferentWakeLocks) {
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleep));
  EXPECT_EQ(0, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleepAllowDimming));

  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::DIM);
  EXPECT_EQ(1, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleep));
  EXPECT_EQ(1, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleepAllowDimming));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(0, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleep));
  EXPECT_EQ(1, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleepAllowDimming));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::DIM);
  EXPECT_EQ(0, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleep));
  EXPECT_EQ(0, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleepAllowDimming));
}

TEST_F(ArcPowerBridgeTest, ConsolidateWakeLocks) {
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleep));

  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleep));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleep));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(0, wake_lock_provider_->GetActiveWakeLocksOfType(
                   WakeLockType::PreventDisplaySleep));
}

}  // namespace arc
