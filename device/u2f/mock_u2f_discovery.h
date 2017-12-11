// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_MOCK_U2F_DISCOVERY_H_
#define DEVICE_U2F_MOCK_U2F_DISCOVERY_H_

#include <memory>
#include <string>

#include "device/u2f/mock_u2f_device.h"
#include "device/u2f/u2f_device.h"
#include "device/u2f/u2f_discovery.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockU2fDiscoveryObserver : public U2fDiscovery::Observer {
 public:
  MockU2fDiscoveryObserver();
  ~MockU2fDiscoveryObserver() override;

  MOCK_METHOD2(DiscoveryStarted, void(U2fDiscovery*, bool));
  MOCK_METHOD2(DiscoveryStopped, void(U2fDiscovery*, bool));
  MOCK_METHOD2(DeviceAdded, void(U2fDiscovery*, U2fDevice*));
  MOCK_METHOD2(DeviceRemoved, void(U2fDiscovery*, U2fDevice*));
};

class MockU2fDiscovery : public U2fDiscovery {
 public:
  MockU2fDiscovery();
  ~MockU2fDiscovery() override;

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());

  bool AddDevice(std::unique_ptr<U2fDevice> device) override;
  bool RemoveDevice(base::StringPiece device_id) override;

  base::ObserverList<Observer>& GetObservers();

  void StartSuccess();
  void StartFailure();

  void StartSuccessAsync();
  void StartFailureAsync();
};

}  // namespace device

#endif  // DEVICE_U2F_MOCK_U2F_DISCOVERY_H_
