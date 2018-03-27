// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/gatt_client_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "chromecast/device/bluetooth/le/remote_service.h"

namespace chromecast {
namespace bluetooth {

namespace {

#define RUN_ON_IO_THREAD(method, ...) \
  io_task_runner_->PostTask(          \
      FROM_HERE,                      \
      base::BindOnce(&GattClientManager::method, weak_this_, ##__VA_ARGS__));

#define MAKE_SURE_IO_THREAD(method, ...)            \
  DCHECK(io_task_runner_);                          \
  if (!io_task_runner_->BelongsToCurrentThread()) { \
    RUN_ON_IO_THREAD(method, ##__VA_ARGS__)         \
    return;                                         \
  }

#define CHECK_DEVICE_EXISTS_IT(it)                  \
  do {                                              \
    if (it == addr_to_device_.end()) {              \
      LOG(ERROR) << __func__ << ": No such device"; \
      return;                                       \
    }                                               \
  } while (0)

}  // namespace

GattClientManager::GattClientManager(
    bluetooth_v2_shlib::GattClient* gatt_client)
    : gatt_client_(gatt_client),
      observers_(new base::ObserverListThreadSafe<Observer>()),
      weak_factory_(
          std::make_unique<base::WeakPtrFactory<GattClientManager>>(this)) {
  weak_this_ = weak_factory_->GetWeakPtr();
}

GattClientManager::~GattClientManager() {}

void GattClientManager::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  io_task_runner_ = std::move(io_task_runner);
}

void GattClientManager::Finalize() {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GattClientManager::FinalizeOnIoThread,
                                std::move(weak_factory_)));
}

void GattClientManager::AddObserver(Observer* o) {
  observers_->AddObserver(o);
}

void GattClientManager::RemoveObserver(Observer* o) {
  observers_->RemoveObserver(o);
}

void GattClientManager::GetDevice(
    const bluetooth_v2_shlib::Addr& addr,
    base::OnceCallback<void(scoped_refptr<RemoteDevice>)> cb) {
  MAKE_SURE_IO_THREAD(GetDevice, addr, BindToCurrentSequence(std::move(cb)));
  DCHECK(cb);
  std::move(cb).Run(GetDeviceSync(addr));
}

scoped_refptr<RemoteDevice> GattClientManager::GetDeviceSync(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = addr_to_device_.find(addr);
  if (it != addr_to_device_.end()) {
    return it->second.get();
  }

  scoped_refptr<RemoteDevice> new_device(
      new RemoteDevice(addr, weak_this_, io_task_runner_));
  addr_to_device_[addr] = new_device;
  return new_device;
}

size_t GattClientManager::GetNumConnected() const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return connected_devices_.size();
}

void GattClientManager::NotifyConnect(const bluetooth_v2_shlib::Addr& addr) {
  observers_->Notify(FROM_HERE, &Observer::OnConnectInitated, addr);
}

void GattClientManager::OnConnectChanged(const bluetooth_v2_shlib::Addr& addr,
                                         bool status,
                                         bool connected) {
  MAKE_SURE_IO_THREAD(OnConnectChanged, addr, status, connected);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);

  it->second->SetConnected(connected);
  if (connected) {
    connected_devices_.insert(addr);
  } else {
    connected_devices_.erase(addr);
  }

  observers_->Notify(FROM_HERE, &Observer::OnConnectChanged, it->second,
                     connected);
}

void GattClientManager::OnNotification(const bluetooth_v2_shlib::Addr& addr,
                                       uint16_t handle,
                                       const std::vector<uint8_t>& value) {
  MAKE_SURE_IO_THREAD(OnNotification, addr, handle, value);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  auto characteristic = it->second->CharacteristicFromHandle(handle);
  if (!characteristic) {
    LOG(ERROR) << "No such characteristic";
    return;
  }

  observers_->Notify(FROM_HERE, &Observer::OnCharacteristicNotification,
                     it->second, characteristic, value);
}

void GattClientManager::OnCharacteristicReadResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle,
    const std::vector<uint8_t>& value) {
  MAKE_SURE_IO_THREAD(OnCharacteristicReadResponse, addr, status, handle,
                      value);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  auto characteristic = it->second->CharacteristicFromHandle(handle);
  if (!characteristic) {
    LOG(ERROR) << "No such characteristic";
    return;
  }

  characteristic->OnReadComplete(status, value);
}

void GattClientManager::OnCharacteristicWriteResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle) {
  MAKE_SURE_IO_THREAD(OnCharacteristicWriteResponse, addr, status, handle);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  auto characteristic = it->second->CharacteristicFromHandle(handle);
  if (!characteristic) {
    LOG(ERROR) << "No such characteristic";
    return;
  }

  characteristic->OnWriteComplete(status);
}

void GattClientManager::OnDescriptorReadResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle,
    const std::vector<uint8_t>& value) {
  MAKE_SURE_IO_THREAD(OnDescriptorReadResponse, addr, status, handle, value);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  auto descriptor = it->second->DescriptorFromHandle(handle);
  if (!descriptor) {
    LOG(ERROR) << "No such descriptor";
    return;
  }

  descriptor->OnReadComplete(status, value);
}

void GattClientManager::OnDescriptorWriteResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle) {
  MAKE_SURE_IO_THREAD(OnDescriptorWriteResponse, addr, status, handle);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  auto descriptor = it->second->DescriptorFromHandle(handle);
  if (!descriptor) {
    LOG(ERROR) << "No such descriptor";
    return;
  }

  descriptor->OnWriteComplete(status);
}

void GattClientManager::OnReadRemoteRssi(const bluetooth_v2_shlib::Addr& addr,
                                         bool status,
                                         int rssi) {
  MAKE_SURE_IO_THREAD(OnReadRemoteRssi, addr, status, rssi);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnReadRemoteRssiComplete(status, rssi);
}

void GattClientManager::OnMtuChanged(const bluetooth_v2_shlib::Addr& addr,
                                     bool status,
                                     int mtu) {
  MAKE_SURE_IO_THREAD(OnMtuChanged, addr, status, mtu);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->SetMtu(mtu);

  observers_->Notify(FROM_HERE, &Observer::OnMtuChanged, it->second, mtu);
}

void GattClientManager::OnGetServices(
    const bluetooth_v2_shlib::Addr& addr,
    const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) {
  MAKE_SURE_IO_THREAD(OnGetServices, addr, services);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnGetServices(services);

  observers_->Notify(FROM_HERE, &Observer::OnServicesUpdated, it->second,
                     it->second->GetServicesSync());
}

void GattClientManager::OnServicesRemoved(const bluetooth_v2_shlib::Addr& addr,
                                          uint16_t start_handle,
                                          uint16_t end_handle) {
  MAKE_SURE_IO_THREAD(OnServicesRemoved, addr, start_handle, end_handle);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnServicesRemoved(start_handle, end_handle);

  observers_->Notify(FROM_HERE, &Observer::OnServicesUpdated, it->second,
                     it->second->GetServicesSync());
}

void GattClientManager::OnServicesAdded(
    const bluetooth_v2_shlib::Addr& addr,
    const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) {
  MAKE_SURE_IO_THREAD(OnServicesAdded, addr, services);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);

  it->second->OnServicesAdded(services);
  observers_->Notify(FROM_HERE, &Observer::OnServicesUpdated, it->second,
                     it->second->GetServicesSync());
}

// static
void GattClientManager::FinalizeOnIoThread(
    std::unique_ptr<base::WeakPtrFactory<GattClientManager>> weak_factory) {
  weak_factory->InvalidateWeakPtrs();
}

}  // namespace bluetooth
}  // namespace chromecast
