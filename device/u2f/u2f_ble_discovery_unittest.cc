// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_ble_discovery.h"

#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/u2f/mock_u2f_discovery.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif defined(OS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#endif

namespace device {

ACTION_P(ReturnFromAsyncCall, closure) {
  closure.Run();
}

MATCHER_P(IdMatches, id, "") {
  return arg->GetId() == std::string("ble:") + id;
}

TEST_F(BluetoothTest, U2fBleDiscoveryFindsKnownDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  SimulateLowEnergyDevice(4);  // This device should be ignored.
  SimulateLowEnergyDevice(7);

  U2fBleDiscovery discovery;
  MockU2fDiscoveryObserver observer;
  discovery.AddObserver(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer,
                DeviceAdded(&discovery,
                            IdMatches(BluetoothTestBase::kTestDeviceAddress1)));
    EXPECT_CALL(observer, DiscoveryStarted(&discovery, true))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();
  }

  // TODO(crbug/763303): Delete device and check OnDeviceDeleted invocation.

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();

    EXPECT_CALL(observer, DiscoveryStopped(&discovery, true))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Stop();
    run_loop.Run();
  }
}

TEST_F(BluetoothTest, U2fBleDiscoveryFindsNewDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  U2fBleDiscovery discovery;
  MockU2fDiscoveryObserver observer;
  discovery.AddObserver(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer, DiscoveryStarted(&discovery, true))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer,
                DeviceAdded(&discovery,
                            IdMatches(BluetoothTestBase::kTestDeviceAddress1)))
        .WillOnce(ReturnFromAsyncCall(quit));

    SimulateLowEnergyDevice(4);  // This device should be ignored.
    SimulateLowEnergyDevice(7);

    run_loop.Run();
  }

  // TODO(crbug/763303): Delete device and check OnDeviceDeleted invocation.

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer, DiscoveryStopped(&discovery, true))
        .WillOnce(ReturnFromAsyncCall(quit));
    discovery.Stop();
    run_loop.Run();
  }
}

}  // namespace device
