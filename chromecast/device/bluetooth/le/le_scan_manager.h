// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list_threadsafe.h"
#include "chromecast/device/bluetooth/shlib/le_scanner.h"

namespace chromecast {
namespace bluetooth {

class LeScanManager : public bluetooth_v2_shlib::LeScanner::Delegate {
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

  explicit LeScanManager(bluetooth_v2_shlib::LeScannerImpl* le_scanner);
  ~LeScanManager() override;

  void Initialize(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  void Finalize();

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  // Enable or disable BLE scnaning. Can be called on any thread. |cb| is
  // called on the thread that calls this method. |success| is false iff the
  // operation failed.
  using SetScanEnableCallback = base::OnceCallback<void(bool success)>;
  void SetScanEnable(bool enable, SetScanEnableCallback cb);

  // Asynchronously get the most recent scan results. Can be called on any
  // thread. |cb| is called on the calling thread with the results. If
  // |service_uuid| is passed, only scan results advertising the given
  // |service_uuid| will be returned.
  using GetScanResultsCallback =
      base::OnceCallback<void(std::vector<ScanResult>)>;
  void GetScanResults(GetScanResultsCallback cb,
                      base::Optional<uint16_t> service_uuid = base::nullopt);

  void ClearScanResults();

 private:
  // Returns a list of all BLE scan results. The results are sorted by RSSI.
  // Must be called on |io_task_runner|.
  std::vector<ScanResult> GetScanResultsInternal(
      base::Optional<uint16_t> service_uuid);

  // bluetooth_v2_shlib::LeScanner::Delegate implementation:
  void OnScanResult(const bluetooth_v2_shlib::LeScanner::ScanResult&
                        scan_result_shlib) override;

  bluetooth_v2_shlib::LeScannerImpl* const le_scanner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;
  std::map<bluetooth_v2_shlib::Addr, std::list<ScanResult>>
      addr_to_scan_results_;

  base::WeakPtrFactory<LeScanManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LeScanManager);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_H_
