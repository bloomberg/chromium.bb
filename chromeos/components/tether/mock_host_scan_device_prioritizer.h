// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_MOCK_HOST_SCAN_DEVICE_PRIORITIZER_H_
#define CHROMEOS_COMPONENTS_TETHER_MOCK_HOST_SCAN_DEVICE_PRIORITIZER_H_

#include <vector>

#include "base/macros.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "components/cryptauth/remote_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

namespace tether {

// Test double for HostScanDevicePrioritizer.
class MockHostScanDevicePrioritizer : public HostScanDevicePrioritizer {
 public:
  MockHostScanDevicePrioritizer();
  ~MockHostScanDevicePrioritizer() override;

  MOCK_CONST_METHOD1(SortByHostScanOrder,
                     void(std::vector<cryptauth::RemoteDevice>*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHostScanDevicePrioritizer);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_MOCK_HOST_SCAN_DEVICE_PRIORITIZER_H_
