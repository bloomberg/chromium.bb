// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_LE_SCAN_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_LE_SCAN_MANAGER_H_

#include <vector>

#include "chromecast/device/bluetooth/le/le_scan_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockLeScanManager : public LeScanManager {
 public:
  MockLeScanManager();
  ~MockLeScanManager();

  MOCK_METHOD1(AddObserver, void(Observer* o));
  MOCK_METHOD1(RemoveObserver, void(Observer* o));
  void SetScanEnable(bool enable, SetScanEnableCallback cb) override {}
  void GetScanResults(GetScanResultsCallback cb,
                      base::Optional<uint16_t> service_uuid) override {}
  MOCK_METHOD0(ClearScanResults, void());
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_LE_SCAN_MANAGER_H_
