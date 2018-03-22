// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/le_scan_manager.h"

#include <algorithm>

#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/public/cast_media_shlib.h"

#define RUN_ON_IO_THREAD(method, ...)                   \
  io_task_runner_->PostTask(                            \
      FROM_HERE, base::BindOnce(&LeScanManager::method, \
                                weak_factory_.GetWeakPtr(), ##__VA_ARGS__));

#define MAKE_SURE_IO_THREAD(method, ...)            \
  DCHECK(io_task_runner_);                          \
  if (!io_task_runner_->BelongsToCurrentThread()) { \
    RUN_ON_IO_THREAD(method, ##__VA_ARGS__)         \
    return;                                         \
  }

#define EXEC_CB_AND_RET(cb, ret, ...)        \
  do {                                       \
    if (cb) {                                \
      std::move(cb).Run(ret, ##__VA_ARGS__); \
    }                                        \
    return;                                  \
  } while (0)

namespace chromecast {
namespace bluetooth {

namespace {

// http://www.argenox.com/bluetooth-low-energy-ble-v4-0-development/library/a-ble-advertising-primer/
const uint8_t kGapFlags = 0x01;
const uint8_t kGapIncomplete16BitServiceUuids = 0x02;
const uint8_t kGapComplete16BitServiceUuids = 0x03;
const uint8_t kGapShortName = 0x08;
const uint8_t kGapCompleteName = 0x09;

const int kMaxMessagesInQueue = 5;

bool DataContainsUuid(const std::vector<uint8_t>& data, uint16_t uuid) {
  if (data.size() % 2 != 0) {
    LOG(ERROR) << "Malformed service UUID data";
    return false;
  }
  for (size_t i = 0; i < data.size(); i += 2) {
    // Uuids are transmitted in little endian byte order (i.e. 0x1827 is
    // transmitted as 0x27, 0x18), so we have to reverse the order on
    // reconstruction. See Bluetooth Core Specification v4.0 Vol 3 Part C
    // Section 11.1 for reference.
    uint16_t uuid_from_data = ((uint16_t)data[i + 1] << 8) | data[i];
    if (uuid_from_data == uuid) {
      return true;
    }
  }
  return false;
}

bool ScanResultHasServiceUuid(const LeScanManager::ScanResult& scan_result,
                              uint16_t service_uuid) {
  auto it = scan_result.type_to_data.find(kGapIncomplete16BitServiceUuids);
  if (it != scan_result.type_to_data.end() &&
      DataContainsUuid(it->second, service_uuid)) {
    return true;
  }

  it = scan_result.type_to_data.find(kGapComplete16BitServiceUuids);
  if (it != scan_result.type_to_data.end() &&
      DataContainsUuid(it->second, service_uuid)) {
    return true;
  }

  return false;
}

}  // namespace

LeScanManager::ScanResult::ScanResult() = default;
LeScanManager::ScanResult::ScanResult(const LeScanManager::ScanResult& other) =
    default;
LeScanManager::ScanResult::~ScanResult() = default;

LeScanManager::LeScanManager(bluetooth_v2_shlib::LeScannerImpl* le_scanner)
    : le_scanner_(le_scanner),
      observers_(new base::ObserverListThreadSafe<Observer>()),
      weak_factory_(this) {}

LeScanManager::~LeScanManager() = default;

void LeScanManager::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  io_task_runner_ = std::move(io_task_runner);
}

void LeScanManager::Finalize() {}

void LeScanManager::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void LeScanManager::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

void LeScanManager::SetScanEnable(bool enable, SetScanEnableCallback cb) {
  MAKE_SURE_IO_THREAD(SetScanEnable, enable,
                      BindToCurrentThread(std::move(cb)));
  bool success;
  if (enable) {
    success = le_scanner_->StartScan();
  } else {
    success = le_scanner_->StopScan();
  }

  if (!success) {
    LOG(ERROR) << "Failed to " << (enable ? "enable" : "disable")
               << " ble scanning";
    EXEC_CB_AND_RET(cb, false);
  }

  observers_->Notify(FROM_HERE, &Observer::OnScanEnableChanged, enable);
  EXEC_CB_AND_RET(cb, true);
}

void LeScanManager::GetScanResults(GetScanResultsCallback cb,
                                   base::Optional<uint16_t> service_uuid) {
  MAKE_SURE_IO_THREAD(GetScanResults, BindToCurrentThread(std::move(cb)),
                      service_uuid);
  std::move(cb).Run(GetScanResultsInternal(service_uuid));
}

// Returns a list of all scan results. The results are sorted by RSSI.
std::vector<LeScanManager::ScanResult> LeScanManager::GetScanResultsInternal(
    base::Optional<uint16_t> service_uuid) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  std::vector<ScanResult> results;
  for (const auto& pair : addr_to_scan_results_) {
    for (const auto& scan_result : pair.second) {
      if (!service_uuid ||
          ScanResultHasServiceUuid(scan_result, *service_uuid)) {
        results.push_back(scan_result);
      }
    }
  }

  std::sort(results.begin(), results.end(),
            [](const ScanResult& d1, const ScanResult& d2) {
              return d1.rssi > d2.rssi;
            });

  return results;
}

void LeScanManager::ClearScanResults() {
  MAKE_SURE_IO_THREAD(ClearScanResults);
  addr_to_scan_results_.clear();
}

void LeScanManager::OnScanResult(
    const bluetooth_v2_shlib::LeScanner::ScanResult& scan_result_shlib) {
  ScanResult scan_result;
  scan_result.addr = scan_result_shlib.addr;
  scan_result.adv_data = scan_result_shlib.adv_data;
  scan_result.rssi = scan_result_shlib.rssi;

  const char* as_char =
      reinterpret_cast<const char*>(scan_result.adv_data.data());

  size_t i = 0;
  while (i < scan_result.adv_data.size()) {
    if (i + 1 == scan_result.adv_data.size()) {
      LOG(ERROR) << "Malformed BLE packet";
      return;
    }

    // http://www.argenox.com/bluetooth-low-energy-ble-v4-0-development/library/a-ble-advertising-primer/
    // Format:
    // [size][type][payload     ]
    // [i   ][i+1 ][i+2:i+1+size]
    //
    // Note: size does not include its own byte
    uint8_t size = scan_result.adv_data[i];
    uint8_t type = scan_result.adv_data[i + 1];

    // Avoid infinite loop if invalid data
    if (size == 0 || i + 1 + size > scan_result.adv_data.size()) {
      LOG(ERROR) << "Invalid size";
      return;
    }

    if (type == kGapCompleteName ||
        (type == kGapShortName && scan_result.name.empty())) {
      scan_result.name.assign(as_char + i + 2, size - 1);
    } else if (type == kGapFlags) {
      scan_result.flags = scan_result.adv_data[i + 2];
    }

    std::vector<uint8_t> data(scan_result.adv_data.begin() + i + 2,
                              scan_result.adv_data.begin() + i + 1 + size);
    scan_result.type_to_data[type] = std::move(data);

    i += (size + 1);
  }

  // Remove results with the same data as the current result to avoid duplicate
  // messages in the queue
  auto& previous_scan_results = addr_to_scan_results_[scan_result.addr];
  previous_scan_results.remove_if([&scan_result](const auto& previous_result) {
    return previous_result.adv_data == scan_result.adv_data;
  });

  previous_scan_results.push_front(scan_result);
  if (previous_scan_results.size() > kMaxMessagesInQueue) {
    previous_scan_results.pop_back();
  }

  // Update observers.
  observers_->Notify(FROM_HERE, &Observer::OnNewScanResult, scan_result);
}

}  // namespace bluetooth
}  // namespace chromecast
