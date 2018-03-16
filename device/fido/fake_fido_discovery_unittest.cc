// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fake_fido_discovery.h"

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace test {

class FakeFidoDiscoveryTest : public ::testing::Test {
 public:
  FakeFidoDiscoveryTest() = default;
  ~FakeFidoDiscoveryTest() override = default;

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeFidoDiscoveryTest);
};

using ScopedFakeFidoDiscoveryFactoryTest = FakeFidoDiscoveryTest;

TEST_F(FakeFidoDiscoveryTest, Transport) {
  FakeFidoDiscovery discovery_ble(U2fTransportProtocol::kBluetoothLowEnergy);
  EXPECT_EQ(U2fTransportProtocol::kBluetoothLowEnergy,
            discovery_ble.GetTransportProtocol());

  FakeFidoDiscovery discovery_hid(
      U2fTransportProtocol::kUsbHumanInterfaceDevice);
  EXPECT_EQ(U2fTransportProtocol::kUsbHumanInterfaceDevice,
            discovery_hid.GetTransportProtocol());
}

TEST_F(FakeFidoDiscoveryTest, InitialState) {
  FakeFidoDiscovery discovery(U2fTransportProtocol::kBluetoothLowEnergy);

  ASSERT_FALSE(discovery.is_running());
  ASSERT_FALSE(discovery.is_start_requested());
  ASSERT_FALSE(discovery.is_stop_requested());
}

TEST_F(FakeFidoDiscoveryTest, StartStopDiscovery) {
  FakeFidoDiscovery discovery(U2fTransportProtocol::kBluetoothLowEnergy);

  MockFidoDiscoveryObserver observer;
  discovery.AddObserver(&observer);

  discovery.Start();

  ASSERT_FALSE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_FALSE(discovery.is_stop_requested());

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true));
  discovery.WaitForCallToStartAndSimulateSuccess();
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  ASSERT_TRUE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_FALSE(discovery.is_stop_requested());

  discovery.Stop();

  ASSERT_TRUE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_TRUE(discovery.is_stop_requested());

  EXPECT_CALL(observer, DiscoveryStopped(&discovery, true));
  discovery.WaitForCallToStopAndSimulateSuccess();
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  ASSERT_FALSE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_TRUE(discovery.is_stop_requested());
}

TEST_F(FakeFidoDiscoveryTest, WaitThenStartStopDiscovery) {
  FakeFidoDiscovery discovery(U2fTransportProtocol::kBluetoothLowEnergy);

  MockFidoDiscoveryObserver observer;
  discovery.AddObserver(&observer);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { discovery.Start(); }));

  discovery.WaitForCallToStart();

  ASSERT_FALSE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_FALSE(discovery.is_stop_requested());

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true));
  discovery.SimulateStarted(true);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  ASSERT_TRUE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_FALSE(discovery.is_stop_requested());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { discovery.Stop(); }));

  discovery.WaitForCallToStop();

  ASSERT_TRUE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_TRUE(discovery.is_stop_requested());

  EXPECT_CALL(observer, DiscoveryStopped(&discovery, true));
  discovery.SimulateStopped(true);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  ASSERT_FALSE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_TRUE(discovery.is_stop_requested());
}

// Starting discovery and failing: instance stays in "not running" state
TEST_F(FakeFidoDiscoveryTest, StartFail) {
  FakeFidoDiscovery discovery(U2fTransportProtocol::kBluetoothLowEnergy);

  MockFidoDiscoveryObserver observer;
  discovery.AddObserver(&observer);

  discovery.Start();

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, false));
  discovery.SimulateStarted(false);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  ASSERT_FALSE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_FALSE(discovery.is_stop_requested());
}

// Stopping discovery and failing: instance stays in a "running" state.
TEST_F(FakeFidoDiscoveryTest, StopFail) {
  FakeFidoDiscovery discovery(U2fTransportProtocol::kBluetoothLowEnergy);

  MockFidoDiscoveryObserver observer;
  discovery.AddObserver(&observer);

  discovery.Start();

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true));
  discovery.SimulateStarted(true);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  ASSERT_TRUE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_FALSE(discovery.is_stop_requested());

  discovery.Stop();

  EXPECT_CALL(observer, DiscoveryStopped(&discovery, false));
  discovery.SimulateStopped(false);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  ASSERT_TRUE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_TRUE(discovery.is_stop_requested());
}

// Adding device is possible both before and after discovery actually starts.
TEST_F(FakeFidoDiscoveryTest, AddDevice) {
  FakeFidoDiscovery discovery(U2fTransportProtocol::kBluetoothLowEnergy);

  MockFidoDiscoveryObserver observer;
  discovery.AddObserver(&observer);

  discovery.Start();

  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillOnce(::testing::Return("device0"));
  EXPECT_CALL(observer, DeviceAdded(&discovery, ::testing::_));
  discovery.AddDevice(std::move(device0));
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true));
  discovery.SimulateStarted(true);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillOnce(::testing::Return("device1"));
  EXPECT_CALL(observer, DeviceAdded(&discovery, ::testing::_));
  discovery.AddDevice(std::move(device1));
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

#if !defined(OS_ANDROID)
TEST_F(ScopedFakeFidoDiscoveryFactoryTest,
       OverridesUsbFactoryFunctionWhileInScope) {
  ScopedFakeFidoDiscoveryFactory factory;
  auto* injected_fake_discovery = factory.ForgeNextHidDiscovery();
  ASSERT_EQ(U2fTransportProtocol::kUsbHumanInterfaceDevice,
            injected_fake_discovery->GetTransportProtocol());
  auto produced_discovery = FidoDiscovery::Create(
      U2fTransportProtocol::kUsbHumanInterfaceDevice, nullptr);
  EXPECT_TRUE(produced_discovery);
  EXPECT_EQ(injected_fake_discovery, produced_discovery.get());
}
#endif

TEST_F(ScopedFakeFidoDiscoveryFactoryTest,
       OverridesBleFactoryFunctionWhileInScope) {
  ScopedFakeFidoDiscoveryFactory factory;

  auto* injected_fake_discovery_1 = factory.ForgeNextBleDiscovery();
  ASSERT_EQ(U2fTransportProtocol::kBluetoothLowEnergy,
            injected_fake_discovery_1->GetTransportProtocol());
  auto produced_discovery_1 =
      FidoDiscovery::Create(U2fTransportProtocol::kBluetoothLowEnergy, nullptr);
  EXPECT_EQ(injected_fake_discovery_1, produced_discovery_1.get());

  auto* injected_fake_discovery_2 = factory.ForgeNextBleDiscovery();
  ASSERT_EQ(U2fTransportProtocol::kBluetoothLowEnergy,
            injected_fake_discovery_2->GetTransportProtocol());
  auto produced_discovery_2 =
      FidoDiscovery::Create(U2fTransportProtocol::kBluetoothLowEnergy, nullptr);
  EXPECT_EQ(injected_fake_discovery_2, produced_discovery_2.get());
}

}  // namespace test
}  // namespace device
