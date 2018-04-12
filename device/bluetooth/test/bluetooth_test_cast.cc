// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_cast.h"

#include "base/bind_helpers.h"
#include "base/task_scheduler/post_task.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager_impl.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "device/bluetooth/cast/bluetooth_adapter_cast.h"
#include "device/bluetooth/test/bluetooth_test_cast.h"

namespace device {

class BluetoothTestCast::FakeLeScanManager
    : public chromecast::bluetooth::LeScanManager {
 public:
  FakeLeScanManager() = default;
  ~FakeLeScanManager() override = default;

  void AddObserver(Observer* o) override {
    DCHECK(!observer_);
    observer_ = o;
  }
  void RemoveObserver(Observer* o) override {
    DCHECK_EQ(observer_, o);
    observer_ = nullptr;
  }
  void SetScanEnable(bool enable, SetScanEnableCallback cb) override {
    std::move(cb).Run(true);
  }
  void GetScanResults(GetScanResultsCallback cb,
                      base::Optional<chromecast::bluetooth::ScanFilter>
                          scan_filter = base::nullopt) override {}
  void ClearScanResults() override {}

  Observer* observer() {
    DCHECK(observer_);
    return observer_;
  }

 private:
  Observer* observer_ = nullptr;
};

BluetoothTestCast::BluetoothTestCast()
    : io_task_runner_(base::CreateSingleThreadTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND})),
      gatt_client_manager_(
          new chromecast::bluetooth::GattClientManagerImpl(&mock_gatt_client_)),
      fake_le_scan_manager_(new FakeLeScanManager()) {
  gatt_client_manager_->Initialize(io_task_runner_);
  mock_gatt_client_.SetDelegate(gatt_client_manager_.get());
}

BluetoothTestCast::~BluetoothTestCast() {
  // Destroy |discovery_sesions_| before adapter, which may hold references to
  // it.
  discovery_sessions_.clear();

  // Tear down adapter, which relies on members in the subclass.
  adapter_ = nullptr;
  gatt_client_manager_->Finalize();
}

void BluetoothTestCast::InitWithFakeAdapter() {
  adapter_ = new BluetoothAdapterCast(gatt_client_manager_.get(),
                                      fake_le_scan_manager_.get());
  adapter_->SetPowered(true, base::DoNothing(), base::DoNothing());
}

bool BluetoothTestCast::PlatformSupportsLowEnergy() {
  return true;
}

BluetoothDevice* BluetoothTestCast::SimulateLowEnergyDevice(
    int device_ordinal) {
  if (device_ordinal > 7 || device_ordinal < 1)
    return nullptr;

  base::Optional<std::string> device_name = std::string(kTestDeviceName);
  std::string device_address = kTestDeviceAddress1;
  std::vector<std::string> service_uuids;
  std::map<std::string, std::vector<uint8_t>> service_data;
  std::map<uint16_t, std::vector<uint8_t>> manufacturer_data;

  switch (device_ordinal) {
    case 1:
      service_uuids.push_back(kTestUUIDGenericAccess);
      service_uuids.push_back(kTestUUIDGenericAttribute);
      service_data[kTestUUIDHeartRate] = {0x01};
      manufacturer_data[kTestManufacturerId] = {1, 2, 3, 4};
      break;
    case 2:
      service_uuids.push_back(kTestUUIDImmediateAlert);
      service_uuids.push_back(kTestUUIDLinkLoss);
      service_data[kTestUUIDHeartRate] = {};
      service_data[kTestUUIDImmediateAlert] = {0x00, 0x02};
      manufacturer_data[kTestManufacturerId] = {};
      break;
    case 3:
      device_name = std::string(kTestDeviceNameEmpty);
      break;
    case 4:
      device_name = std::string(kTestDeviceNameEmpty);
      device_address = kTestDeviceAddress2;
      break;
    case 5:
      device_name = base::nullopt;
      break;
    default:
      NOTREACHED();
  }
  UpdateAdapter(device_address, device_name, service_uuids, service_data,
                manufacturer_data);
  return adapter_->GetDevice(device_address);
}

void BluetoothTestCast::UpdateAdapter(
    const std::string& address,
    const base::Optional<std::string>& name,
    const std::vector<std::string>& service_uuids,
    const std::map<std::string, std::vector<uint8_t>>& service_data,
    const std::map<uint16_t, std::vector<uint8_t>>& manufacturer_data) {
  // Create a scan result with the desired address and name.
  chromecast::bluetooth::LeScanResult result;
  ASSERT_TRUE(chromecast::bluetooth::util::ParseAddr(address, &result.addr));
  if (name) {
    result.type_to_data[chromecast::bluetooth::LeScanResult::kGapCompleteName]
        .push_back(std::vector<uint8_t>(name->begin(), name->end()));
  }

  // Add service data.
  for (const auto& it : service_data) {
    chromecast::bluetooth_v2_shlib::Uuid uuid;
    ASSERT_TRUE(chromecast::bluetooth::util::ParseUuid(it.first, &uuid));
    std::vector<uint8_t> data(uuid.rbegin(), uuid.rend());
    data.insert(data.end(), it.second.begin(), it.second.end());
    result
        .type_to_data
            [chromecast::bluetooth::LeScanResult::kGapServicesData128bit]
        .push_back(std::move(data));
  }

  // Add manufacturer data.
  for (const auto& it : manufacturer_data) {
    std::vector<uint8_t> data({(it.first & 0xFF), ((it.first >> 8) & 0xFF)});
    data.insert(data.end(), it.second.begin(), it.second.end());
    result
        .type_to_data[chromecast::bluetooth::LeScanResult::kGapManufacturerData]
        .push_back(std::move(data));
  }

  // Update the adapter with the ScanResult.
  fake_le_scan_manager_->observer()->OnNewScanResult(result);
  scoped_task_environment_.RunUntilIdle();

  // Add the services.
  std::vector<chromecast::bluetooth_v2_shlib::Gatt::Service> services;
  for (const auto& uuid : service_uuids) {
    chromecast::bluetooth_v2_shlib::Gatt::Service service;
    ASSERT_TRUE(chromecast::bluetooth::util::ParseUuid(uuid, &service.uuid));
    services.push_back(service);
  }
  mock_gatt_client_.delegate()->OnServicesAdded(result.addr, services);
  scoped_task_environment_.RunUntilIdle();
}

}  // namespace device
