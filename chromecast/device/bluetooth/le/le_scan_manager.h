// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "chromecast/public/bluetooth/bluetooth_types.h"

namespace chromecast {
namespace bluetooth {

class LeScanManager {
 public:
  struct ScanResult {
    ScanResult();
    ScanResult(const ScanResult& other);
    ~ScanResult();

    bluetooth_v2_shlib::Addr addr;
    std::vector<uint8_t> adv_data;
    int rssi = -255;

    std::string name;
    uint8_t flags = 0;
    std::map<uint8_t, std::vector<uint8_t>> type_to_data;
  };

  class Observer {
   public:
    // Called when the scan has been enabled or disabled.
    virtual void OnScanEnableChanged(bool enabled) {}

    // Called when a new scan result is ready.
    virtual void OnNewScanResult(ScanResult result) {}

    virtual ~Observer() = default;
  };

  virtual void AddObserver(Observer* o) = 0;
  virtual void RemoveObserver(Observer* o) = 0;

  // Enable or disable BLE scnaning. Can be called on any thread. |cb| is
  // called on the thread that calls this method. |success| is false iff the
  // operation failed.
  using SetScanEnableCallback = base::OnceCallback<void(bool success)>;
  virtual void SetScanEnable(bool enable, SetScanEnableCallback cb) = 0;

  // Asynchronously get the most recent scan results. Can be called on any
  // thread. |cb| is called on the calling thread with the results. If
  // |service_uuid| is passed, only scan results advertising the given
  // |service_uuid| will be returned.
  using GetScanResultsCallback =
      base::OnceCallback<void(std::vector<ScanResult>)>;
  virtual void GetScanResults(
      GetScanResultsCallback cb,
      base::Optional<uint16_t> service_uuid = base::nullopt) = 0;

  virtual void ClearScanResults() = 0;

 protected:
  virtual ~LeScanManager() = default;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_H_
