// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_central_manager_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothAdapterMacTest : public testing::Test {
 public:
  BluetoothAdapterMacTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        adapter_(new BluetoothAdapterMac()),
        adapter_mac_(static_cast<BluetoothAdapterMac*>(adapter_.get())),
        callback_count_(0),
        error_callback_count_(0) {
    adapter_mac_->InitForTest(ui_task_runner_);
  }

  // Helper methods for setup and access to BluetoothAdapterMacTest's members.
  bool SetMockCentralManager() {
    Class aClass = NSClassFromString(@"CBCentralManager");
    if (aClass == nil) {
      LOG(WARNING) << "CoreBluetooth not available, skipping unit test.";
      return false;
    }
    mock_central_manager_ = [[MockCentralManager alloc] init];
    adapter_mac_->low_energy_discovery_manager_->SetManagerForTesting(
        mock_central_manager_);
    return true;
  }

  void AddDiscoverySession(BluetoothDiscoveryFilter* discovery_filter) {
    adapter_mac_->AddDiscoverySession(
        discovery_filter,
        base::Bind(&BluetoothAdapterMacTest::Callback, base::Unretained(this)),
        base::Bind(&BluetoothAdapterMacTest::ErrorCallback,
                   base::Unretained(this)));
  }

  void RemoveDiscoverySession(BluetoothDiscoveryFilter* discovery_filter) {
    adapter_mac_->RemoveDiscoverySession(
        discovery_filter,
        base::Bind(&BluetoothAdapterMacTest::Callback, base::Unretained(this)),
        base::Bind(&BluetoothAdapterMacTest::ErrorCallback,
                   base::Unretained(this)));
  }

  int NumDiscoverySessions() { return adapter_mac_->num_discovery_sessions_; }

  // Generic callbacks.
  void Callback() { ++callback_count_; }
  void ErrorCallback() { ++error_callback_count_; }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothAdapter> adapter_;
  BluetoothAdapterMac* adapter_mac_;

  // Owned by |low_energy_discovery_manager_| on |adapter_mac_|.
  id mock_central_manager_ = NULL;

  int callback_count_;
  int error_callback_count_;
};

TEST_F(BluetoothAdapterMacTest, Poll) {
  EXPECT_FALSE(ui_task_runner_->GetPendingTasks().empty());
}

TEST_F(BluetoothAdapterMacTest, AddDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager())
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(0, NumDiscoverySessions());

  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  // Check that adding a discovery session resulted in
  // scanForPeripheralsWithServices being called on the Central Manager.
  EXPECT_EQ(1, [mock_central_manager_ scanForPeripheralsCallCount]);
}

// TODO(krstnmnlsn): Test changing the filter when adding the second discovery
// session (once we have that ability).
TEST_F(BluetoothAdapterMacTest, AddSecondDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager())
    return;
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  // We replaced the success callback handed to AddDiscoverySession, so
  // |adapter_mac_| should remain in a discovering state indefinitely.
  EXPECT_TRUE(adapter_mac_->IsDiscovering());

  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(2, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(2, NumDiscoverySessions());
}

TEST_F(BluetoothAdapterMacTest, RemoveDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager())
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);

  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
  RemoveDiscoverySession(discovery_filter.get());
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, NumDiscoverySessions());

  // Check that removing the discovery session resulted in stopScan being called
  // on the Central Manager.
  EXPECT_EQ(1, [mock_central_manager_ stopScanCallCount]);
}

TEST_F(BluetoothAdapterMacTest, RemoveDiscoverySessionWithLowEnergyFilterFail) {
  if (!SetMockCentralManager())
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
  EXPECT_EQ(0, NumDiscoverySessions());

  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  RemoveDiscoverySession(discovery_filter.get());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(0, NumDiscoverySessions());

  // Check that stopScan was not called.
  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
}

}  // namespace device
