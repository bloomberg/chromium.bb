// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MOCK_U2F_DISCOVERY_OBSERVER_H_
#define DEVICE_FIDO_MOCK_U2F_DISCOVERY_OBSERVER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "device/fido/u2f_discovery.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class U2fDevice;

class MockU2fDiscoveryObserver : public U2fDiscovery::Observer {
 public:
  MockU2fDiscoveryObserver();
  ~MockU2fDiscoveryObserver() override;

  MOCK_METHOD2(DiscoveryStarted, void(U2fDiscovery*, bool));
  MOCK_METHOD2(DiscoveryStopped, void(U2fDiscovery*, bool));
  MOCK_METHOD2(DeviceAdded, void(U2fDiscovery*, U2fDevice*));
  MOCK_METHOD2(DeviceRemoved, void(U2fDiscovery*, U2fDevice*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockU2fDiscoveryObserver);
};

}  // namespace device

#endif  // DEVICE_FIDO_MOCK_U2F_DISCOVERY_OBSERVER_H_
