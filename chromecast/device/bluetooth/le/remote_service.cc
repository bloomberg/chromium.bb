// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/remote_service.h"

#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"

namespace chromecast {
namespace bluetooth {

// static
std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteCharacteristic>>
RemoteService::CreateCharMap(
    RemoteDevice* remote_device,
    base::WeakPtr<GattClientManager> gatt_client_manager,
    const bluetooth_v2_shlib::Gatt::Service& service,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteCharacteristic>> ret;
  for (const auto& characteristic : service.characteristics) {
    ret[characteristic.uuid] = new RemoteCharacteristic(
        remote_device, gatt_client_manager, &characteristic, io_task_runner);
  }
  return ret;
}

RemoteService::RemoteService(
    RemoteDevice* remote_device,
    base::WeakPtr<GattClientManager> gatt_client_manager,
    const bluetooth_v2_shlib::Gatt::Service& service,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : service_(service),
      uuid_to_characteristic_(CreateCharMap(remote_device,
                                            gatt_client_manager,
                                            service_,
                                            io_task_runner)) {
  DCHECK(gatt_client_manager);
  DCHECK(io_task_runner);
  DCHECK(io_task_runner->BelongsToCurrentThread());
}

RemoteService::~RemoteService() = default;

std::vector<scoped_refptr<RemoteCharacteristic>>
RemoteService::GetCharacteristics() {
  std::vector<scoped_refptr<RemoteCharacteristic>> ret;
  ret.reserve(uuid_to_characteristic_.size());
  for (const auto& pair : uuid_to_characteristic_) {
    ret.push_back(pair.second);
  }

  return ret;
}

scoped_refptr<RemoteCharacteristic> RemoteService::GetCharacteristicByUuid(
    const bluetooth_v2_shlib::Uuid& uuid) {
  auto it = uuid_to_characteristic_.find(uuid);
  if (it == uuid_to_characteristic_.end())
    return nullptr;
  return it->second;
}

}  // namespace bluetooth
}  // namespace chromecast
