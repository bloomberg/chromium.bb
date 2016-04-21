// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_monitor_impl.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_monitor_observer.h"
#include "components/proximity_auth/remote_device.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothDevice;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;

namespace proximity_auth {
namespace {

const char kRemoteDeviceUserId[] = "example@gmail.com";
const char kBluetoothAddress[] = "AA:BB:CC:DD:EE:FF";
const char kRemoteDevicePublicKey[] = "Remote Public Key";
const char kRemoteDeviceName[] = "LGE Nexus 5";
const char kPersistentSymmetricKey[] = "PSK";

class TestProximityMonitorImpl : public ProximityMonitorImpl {
 public:
  explicit TestProximityMonitorImpl(const RemoteDevice& remote_device,
                                    std::unique_ptr<base::TickClock> clock)
      : ProximityMonitorImpl(remote_device, std::move(clock)) {}
  ~TestProximityMonitorImpl() override {}

  using ProximityMonitorImpl::SetStrategy;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProximityMonitorImpl);
};

class MockProximityMonitorObserver : public ProximityMonitorObserver {
 public:
  MockProximityMonitorObserver() {}
  ~MockProximityMonitorObserver() override {}

  MOCK_METHOD0(OnProximityStateChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProximityMonitorObserver);
};

// Creates a mock Bluetooth adapter and sets it as the global adapter for
// testing.
scoped_refptr<device::MockBluetoothAdapter>
CreateAndRegisterMockBluetoothAdapter() {
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new NiceMock<device::MockBluetoothAdapter>();
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);
  return adapter;
}

}  // namespace

class ProximityAuthProximityMonitorImplTest : public testing::Test {
 public:
  ProximityAuthProximityMonitorImplTest()
      : clock_(new base::SimpleTestTickClock()),
        bluetooth_adapter_(CreateAndRegisterMockBluetoothAdapter()),
        remote_bluetooth_device_(&*bluetooth_adapter_,
                                 0,
                                 kRemoteDeviceName,
                                 kBluetoothAddress,
                                 false /* paired */,
                                 true /* connected */),
        monitor_(RemoteDevice(kRemoteDeviceUserId,
                              kRemoteDeviceName,
                              kRemoteDevicePublicKey,
                              RemoteDevice::BLUETOOTH_CLASSIC,
                              kBluetoothAddress,
                              kPersistentSymmetricKey,
                              std::string()),
                 base::WrapUnique(clock_)),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {
    ON_CALL(*bluetooth_adapter_, GetDevice(kBluetoothAddress))
        .WillByDefault(Return(&remote_bluetooth_device_));
    ON_CALL(remote_bluetooth_device_, GetConnectionInfo(_))
        .WillByDefault(SaveArg<0>(&connection_info_callback_));
    monitor_.AddObserver(&observer_);
  }
  ~ProximityAuthProximityMonitorImplTest() override {}

  void RunPendingTasks() { task_runner_->RunPendingTasks(); }

  void ProvideConnectionInfo(
      const BluetoothDevice::ConnectionInfo& connection_info) {
    RunPendingTasks();
    connection_info_callback_.Run(connection_info);

    // Reset the callback to ensure that tests correctly only respond at most
    // once per call to GetConnectionInfo().
    connection_info_callback_ = BluetoothDevice::ConnectionInfoCallback();
  }

 protected:
  // Mock for verifying interactions with the proximity monitor's observer.
  NiceMock<MockProximityMonitorObserver> observer_;

  // Clock used for verifying time calculations. Owned by the monitor_.
  base::SimpleTestTickClock* clock_;

  // Mocks used for verifying interactions with the Bluetooth subsystem.
  scoped_refptr<device::MockBluetoothAdapter> bluetooth_adapter_;
  NiceMock<device::MockBluetoothDevice> remote_bluetooth_device_;

  // The proximity monitor under test.
  TestProximityMonitorImpl monitor_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  BluetoothDevice::ConnectionInfoCallback connection_info_callback_;
  ScopedDisableLoggingForTesting disable_logging_;
};

TEST_F(ProximityAuthProximityMonitorImplTest, GetStrategy) {
  EXPECT_EQ(ProximityMonitor::Strategy::NONE, monitor_.GetStrategy());

  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER);
  EXPECT_EQ(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER,
            monitor_.GetStrategy());
}

TEST_F(ProximityAuthProximityMonitorImplTest, ProximityState_StrategyIsNone) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::NONE);

  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest, ProximityState_NeverStarted) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_Started_NoConnectionInfoReceivedYet) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_InformsObserverOfChanges) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER);

  // Initially, the device is not in proximity.
  monitor_.Start();
  EXPECT_FALSE(monitor_.IsUnlockAllowed());

  // Simulate a reading indicating proximity.
  EXPECT_CALL(observer_, OnProximityStateChanged()).Times(1);
  ProvideConnectionInfo({0, 0, 4});
  EXPECT_TRUE(monitor_.IsUnlockAllowed());

  // Simulate a reading indicating non-proximity.
  EXPECT_CALL(observer_, OnProximityStateChanged()).Times(1);
  ProvideConnectionInfo({0, 4, 4});
  EXPECT_FALSE(monitor_.IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckRssi_RssiIndicatesProximity_TxPowerDoesNot) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  ProvideConnectionInfo({0, 4, 4});

  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckRssi_TxPowerIndicatesProximity_RssiDoesNot) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  ProvideConnectionInfo({-10, 0, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckRssi_NeitherRssiNorTxPowerIndicatesProximity) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  ProvideConnectionInfo({-10, 4, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckRssi_BothRssiAndTxPowerIndicateProximity) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  ProvideConnectionInfo({0, 0, 4});

  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckRssi_UnknownRssi) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  ProvideConnectionInfo({0, 0, 4});
  ProvideConnectionInfo({BluetoothDevice::kUnknownPower, 0, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckRssi_UnknownTxPower) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  ProvideConnectionInfo({0, 0, 4});
  ProvideConnectionInfo({0, BluetoothDevice::kUnknownPower, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckRssi_UnknownMaxTxPower) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  ProvideConnectionInfo({0, 0, 4});
  ProvideConnectionInfo({0, 0, BluetoothDevice::kUnknownPower});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckTxPower_RssiIndicatesProximity_TxPowerDoesNot) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER);
  monitor_.Start();

  ProvideConnectionInfo({0, 4, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckTxPower_TxPowerIndicatesProximity_RssiDoesNot) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER);
  monitor_.Start();

  ProvideConnectionInfo({-10, 0, 4});

  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckTxPower_NeitherRssiNorTxPowerIndicatesProximity) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER);
  monitor_.Start();

  ProvideConnectionInfo({-10, 4, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckTxPower_BothRssiAndTxPowerIndicateProximity) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER);
  monitor_.Start();

  ProvideConnectionInfo({0, 0, 4});

  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckTxPower_UnknownRssi) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER);
  monitor_.Start();

  ProvideConnectionInfo({0, 0, 4});
  ProvideConnectionInfo({BluetoothDevice::kUnknownPower, 0, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckTxPower_UnknownTxPower) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER);
  monitor_.Start();

  ProvideConnectionInfo({0, 0, 4});
  ProvideConnectionInfo({0, BluetoothDevice::kUnknownPower, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_CheckTxPower_UnknownMaxTxPower) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_TRANSMIT_POWER);
  monitor_.Start();

  ProvideConnectionInfo({0, 0, 4});
  ProvideConnectionInfo({0, 0, BluetoothDevice::kUnknownPower});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest, ProximityState_StartThenStop) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);

  monitor_.Start();
  ProvideConnectionInfo({0, 0, 4});
  monitor_.Stop();

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_StartThenStopThenStartAgain) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);

  monitor_.Start();
  ProvideConnectionInfo({-10, 4, 4});
  ProvideConnectionInfo({-10, 4, 4});
  ProvideConnectionInfo({-10, 4, 4});
  ProvideConnectionInfo({-10, 4, 4});
  ProvideConnectionInfo({-10, 4, 4});
  monitor_.Stop();

  // Restarting the monitor should immediately reset the proximity state, rather
  // than building on the previous rolling average.
  monitor_.Start();
  ProvideConnectionInfo({0, 4, 4});

  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_RemoteDeviceRemainsInProximity) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);

  monitor_.Start();
  ProvideConnectionInfo({0, 4, 4});
  ProvideConnectionInfo({-1, 4, 4});
  ProvideConnectionInfo({0, 4, 4});
  ProvideConnectionInfo({-2, 4, 4});
  ProvideConnectionInfo({-1, 4, 4});

  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());

  // Brief drops in RSSI should be handled by weighted averaging.
  ProvideConnectionInfo({-10, 4, 4});

  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_RemoteDeviceLeavesProximity) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  // Start with a device in proximity.
  ProvideConnectionInfo({0, 4, 4});
  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());

  // Simulate readings for the remote device leaving proximity.
  ProvideConnectionInfo({-1, 4, 4});
  ProvideConnectionInfo({-4, 4, 4});
  ProvideConnectionInfo({0, 4, 4});
  ProvideConnectionInfo({-10, 4, 4});
  ProvideConnectionInfo({-8, 4, 4});
  ProvideConnectionInfo({-15, 4, 4});
  ProvideConnectionInfo({-10, 4, 4});
  ProvideConnectionInfo({-10, 4, 4});
  ProvideConnectionInfo({-10, 4, 4});
  ProvideConnectionInfo({-10, 4, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_RemoteDeviceEntersProximity) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  // Start with a device in proximity.
  ProvideConnectionInfo({-20, 4, 4});
  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());

  // Simulate readings for the remote device entering proximity.
  ProvideConnectionInfo({-15, 4, 4});
  ProvideConnectionInfo({-8, 4, 4});
  ProvideConnectionInfo({-12, 4, 4});
  ProvideConnectionInfo({-18, 4, 4});
  ProvideConnectionInfo({-7, 4, 4});
  ProvideConnectionInfo({-3, 4, 4});
  ProvideConnectionInfo({-2, 4, 4});
  ProvideConnectionInfo({0, 4, 4});
  ProvideConnectionInfo({0, 4, 4});

  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_DeviceNotKnownToAdapter) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  // Start with the device known to the adapter and in proximity.
  ProvideConnectionInfo({0, 4, 4});
  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());

  // Simulate it being forgotten.
  ON_CALL(*bluetooth_adapter_, GetDevice(kBluetoothAddress))
      .WillByDefault(Return(nullptr));
  EXPECT_CALL(observer_, OnProximityStateChanged());
  RunPendingTasks();

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_DeviceNotConnected) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);
  monitor_.Start();

  // Start with the device connected and in proximity.
  ProvideConnectionInfo({0, 4, 4});
  EXPECT_TRUE(monitor_.IsUnlockAllowed());
  EXPECT_TRUE(monitor_.IsInRssiRange());

  // Simulate it disconnecting.
  ON_CALL(remote_bluetooth_device_, IsConnected()).WillByDefault(Return(false));
  EXPECT_CALL(observer_, OnProximityStateChanged());
  RunPendingTasks();

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       ProximityState_ConnectionInfoReceivedAfterStopping) {
  monitor_.SetStrategy(ProximityMonitor::Strategy::CHECK_RSSI);

  monitor_.Start();
  monitor_.Stop();
  ProvideConnectionInfo({0, 4, 4});

  EXPECT_FALSE(monitor_.IsUnlockAllowed());
  EXPECT_FALSE(monitor_.IsInRssiRange());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       RecordProximityMetricsOnAuthSuccess_NormalValues) {
  monitor_.Start();
  ProvideConnectionInfo({0, 0, 4});

  clock_->Advance(base::TimeDelta::FromMilliseconds(101));
  ProvideConnectionInfo({-20, 3, 4});

  clock_->Advance(base::TimeDelta::FromMilliseconds(203));
  base::HistogramTester histogram_tester;
  monitor_.RecordProximityMetricsOnAuthSuccess();
  histogram_tester.ExpectUniqueSample("EasyUnlock.AuthProximity.RollingRssi",
                                      -6, 1);
  histogram_tester.ExpectUniqueSample(
      "EasyUnlock.AuthProximity.TransmitPowerDelta", -1, 1);
  histogram_tester.ExpectUniqueSample(
      "EasyUnlock.AuthProximity.TimeSinceLastZeroRssi", 304, 1);
  histogram_tester.ExpectUniqueSample(
      "EasyUnlock.AuthProximity.RemoteDeviceModelHash",
      1881443083 /* hash of "LGE Nexus 5" */, 1);
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       RecordProximityMetricsOnAuthSuccess_ClampedValues) {
  monitor_.Start();
  ProvideConnectionInfo({-99999, 99999, 12345});

  base::HistogramTester histogram_tester;
  monitor_.RecordProximityMetricsOnAuthSuccess();
  histogram_tester.ExpectUniqueSample("EasyUnlock.AuthProximity.RollingRssi",
                                      -100, 1);
  histogram_tester.ExpectUniqueSample(
      "EasyUnlock.AuthProximity.TransmitPowerDelta", 50, 1);
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       RecordProximityMetricsOnAuthSuccess_UnknownValues) {
  // Note: A device without a recorded name will have its Bluetooth address as
  // its name.
  RemoteDevice unnamed_remote_device(
      kRemoteDeviceUserId, kBluetoothAddress, kRemoteDevicePublicKey,
      RemoteDevice::BLUETOOTH_CLASSIC, kBluetoothAddress,
      kPersistentSymmetricKey, std::string());

  std::unique_ptr<base::TickClock> clock(new base::SimpleTestTickClock());
  ProximityMonitorImpl monitor(unnamed_remote_device, std::move(clock));
  monitor.AddObserver(&observer_);
  monitor.Start();
  ProvideConnectionInfo({127, 127, 127});

  base::HistogramTester histogram_tester;
  monitor.RecordProximityMetricsOnAuthSuccess();
  histogram_tester.ExpectUniqueSample("EasyUnlock.AuthProximity.RollingRssi",
                                      127, 1);
  histogram_tester.ExpectUniqueSample(
      "EasyUnlock.AuthProximity.TransmitPowerDelta", 127, 1);
  histogram_tester.ExpectUniqueSample(
      "EasyUnlock.AuthProximity.TimeSinceLastZeroRssi",
      base::TimeDelta::FromSeconds(10).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "EasyUnlock.AuthProximity.RemoteDeviceModelHash",
      -1808066424 /* hash of "Unknown" */, 1);
}

}  // namespace proximity_auth
