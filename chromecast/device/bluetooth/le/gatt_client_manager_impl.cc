// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/gatt_client_manager_impl.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/remote_characteristic_impl.h"
#include "chromecast/device/bluetooth/le/remote_descriptor_impl.h"
#include "chromecast/device/bluetooth/le/remote_device_impl.h"
#include "chromecast/device/bluetooth/le/remote_service_impl.h"

namespace chromecast {
namespace bluetooth {

namespace {

#define RUN_ON_IO_THREAD(method, ...)                                       \
  io_task_runner_->PostTask(                                                \
      FROM_HERE, base::BindOnce(&GattClientManagerImpl::method, weak_this_, \
                                ##__VA_ARGS__));

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

// static
constexpr base::TimeDelta GattClientManagerImpl::kConnectTimeout;
constexpr base::TimeDelta GattClientManagerImpl::kReadRemoteRssiTimeout;

GattClientManagerImpl::GattClientManagerImpl(
    bluetooth_v2_shlib::GattClient* gatt_client)
    : gatt_client_(gatt_client),
      observers_(new base::ObserverListThreadSafe<Observer>()),
      weak_factory_(
          std::make_unique<base::WeakPtrFactory<GattClientManagerImpl>>(this)) {
  weak_this_ = weak_factory_->GetWeakPtr();
}

GattClientManagerImpl::~GattClientManagerImpl() {}

void GattClientManagerImpl::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  io_task_runner_ = std::move(io_task_runner);
}

void GattClientManagerImpl::Finalize() {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GattClientManagerImpl::FinalizeOnIoThread,
                                std::move(weak_factory_)));
}

void GattClientManagerImpl::AddObserver(Observer* o) {
  observers_->AddObserver(o);
}

void GattClientManagerImpl::RemoveObserver(Observer* o) {
  observers_->RemoveObserver(o);
}

void GattClientManagerImpl::GetDevice(
    const bluetooth_v2_shlib::Addr& addr,
    base::OnceCallback<void(scoped_refptr<RemoteDevice>)> cb) {
  MAKE_SURE_IO_THREAD(GetDevice, addr, BindToCurrentSequence(std::move(cb)));
  DCHECK(cb);
  std::move(cb).Run(GetDeviceSync(addr));
}

scoped_refptr<RemoteDevice> GattClientManagerImpl::GetDeviceSync(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = addr_to_device_.find(addr);
  if (it != addr_to_device_.end()) {
    return it->second.get();
  }

  scoped_refptr<RemoteDeviceImpl> new_device(
      new RemoteDeviceImpl(addr, weak_this_, io_task_runner_));
  addr_to_device_[addr] = new_device;
  return new_device;
}

void GattClientManagerImpl::GetConnectedDevices(GetConnectDevicesCallback cb) {
  MAKE_SURE_IO_THREAD(GetConnectedDevices,
                      BindToCurrentSequence(std::move(cb)));
  std::vector<scoped_refptr<RemoteDevice>> devices;
  for (const auto& device : addr_to_device_) {
    if (device.second->IsConnected()) {
      devices.push_back(device.second);
    }
  }

  std::move(cb).Run(std::move(devices));
}

void GattClientManagerImpl::GetNumConnected(
    base::OnceCallback<void(size_t)> cb) const {
  MAKE_SURE_IO_THREAD(GetNumConnected, BindToCurrentSequence(std::move(cb)));
  DCHECK(cb);
  std::move(cb).Run(connected_devices_.size());
}

void GattClientManagerImpl::NotifyConnect(
    const bluetooth_v2_shlib::Addr& addr) {
  observers_->Notify(FROM_HERE, &Observer::OnConnectInitated, addr);
}

void GattClientManagerImpl::EnqueueConnectRequest(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  pending_connect_requests_.push_back(addr);

  // Run the request if this is the only request in the queue. Otherwise, it
  // will be run when all previous requests complete.
  if (pending_connect_requests_.size() == 1) {
    RunQueuedConnectRequest();
  }
}

void GattClientManagerImpl::EnqueueReadRemoteRssiRequest(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  pending_read_remote_rssi_requests_.push_back(addr);

  // Run the request if this is the only request in the queue. Otherwise, it
  // will be run when all previous requests complete.
  if (pending_read_remote_rssi_requests_.size() == 1) {
    RunQueuedReadRemoteRssiRequest();
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
GattClientManagerImpl::task_runner() {
  return io_task_runner_;
}

void GattClientManagerImpl::OnConnectChanged(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    bool connected) {
  MAKE_SURE_IO_THREAD(OnConnectChanged, addr, status, connected);
  auto it = addr_to_device_.find(addr);

  // Silently ignore devices we aren't keeping track of.
  if (it == addr_to_device_.end()) {
    return;
  }

  it->second->SetConnected(connected);
  if (connected) {
    // We won't declare the device connected until service discovery completes,
    // so we won't start next Connect request until then.
    connected_devices_.insert(addr);
  } else {
    connected_devices_.erase(addr);
    if (!pending_connect_requests_.empty() &&
        addr == pending_connect_requests_.front()) {
      pending_connect_requests_.pop_front();
      connect_timeout_timer_.Stop();
      RunQueuedConnectRequest();
    } else {
      base::Erase(pending_connect_requests_, addr);
    }

    base::Erase(pending_read_remote_rssi_requests_, addr);
    read_remote_rssi_timeout_timer_.Stop();
  }

  // We won't declare the device connected until service discovery completes.
  // Only report disconnect callback if the connect callback was called (
  // service discovery completed).
  if (!connected && it->second->GetServicesDiscovered()) {
    it->second->SetServicesDiscovered(false);
    observers_->Notify(FROM_HERE, &Observer::OnConnectChanged, it->second,
                       false);
  }
}

void GattClientManagerImpl::OnNotification(const bluetooth_v2_shlib::Addr& addr,
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

void GattClientManagerImpl::OnCharacteristicReadResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle,
    const std::vector<uint8_t>& value) {
  MAKE_SURE_IO_THREAD(OnCharacteristicReadResponse, addr, status, handle,
                      value);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnCharacteristicRead(status, handle, value);
}

void GattClientManagerImpl::OnCharacteristicWriteResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle) {
  MAKE_SURE_IO_THREAD(OnCharacteristicWriteResponse, addr, status, handle);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnCharacteristicWrite(status, handle);
}

void GattClientManagerImpl::OnDescriptorReadResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle,
    const std::vector<uint8_t>& value) {
  MAKE_SURE_IO_THREAD(OnDescriptorReadResponse, addr, status, handle, value);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnDescriptorRead(status, handle, value);
}

void GattClientManagerImpl::OnDescriptorWriteResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle) {
  MAKE_SURE_IO_THREAD(OnDescriptorWriteResponse, addr, status, handle);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnDescriptorWrite(status, handle);
}

void GattClientManagerImpl::OnReadRemoteRssi(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    int rssi) {
  MAKE_SURE_IO_THREAD(OnReadRemoteRssi, addr, status, rssi);

  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnReadRemoteRssiComplete(status, rssi);

  if (pending_read_remote_rssi_requests_.empty() ||
      addr != pending_read_remote_rssi_requests_.front()) {
    // This can happen when the regular OnReadRemoteRssi is received after
    // ReadRemoteRssi timed out.
    LOG(ERROR) << "Unexpected call to " << __func__;
    return;
  }

  pending_read_remote_rssi_requests_.pop_front();
  read_remote_rssi_timeout_timer_.Stop();
  // Try to run the next ReadRemoteRssi request
  RunQueuedReadRemoteRssiRequest();
}

void GattClientManagerImpl::OnMtuChanged(const bluetooth_v2_shlib::Addr& addr,
                                         bool status,
                                         int mtu) {
  MAKE_SURE_IO_THREAD(OnMtuChanged, addr, status, mtu);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->SetMtu(mtu);

  observers_->Notify(FROM_HERE, &Observer::OnMtuChanged, it->second, mtu);
}

void GattClientManagerImpl::OnGetServices(
    const bluetooth_v2_shlib::Addr& addr,
    const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) {
  MAKE_SURE_IO_THREAD(OnGetServices, addr, services);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnGetServices(services);

  if (!it->second->GetServicesDiscovered()) {
    it->second->SetServicesDiscovered(true);
    observers_->Notify(FROM_HERE, &Observer::OnConnectChanged, it->second,
                       true);
  }

  observers_->Notify(FROM_HERE, &Observer::OnServicesUpdated, it->second,
                     it->second->GetServicesSync());

  if (pending_connect_requests_.empty() ||
      addr != pending_connect_requests_.front()) {
    NOTREACHED() << "Unexpected call to " << __func__;
    return;
  }

  pending_connect_requests_.pop_front();
  connect_timeout_timer_.Stop();
  // Try to run the next Connect request
  RunQueuedConnectRequest();
}

void GattClientManagerImpl::OnServicesRemoved(
    const bluetooth_v2_shlib::Addr& addr,
    uint16_t start_handle,
    uint16_t end_handle) {
  MAKE_SURE_IO_THREAD(OnServicesRemoved, addr, start_handle, end_handle);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnServicesRemoved(start_handle, end_handle);

  observers_->Notify(FROM_HERE, &Observer::OnServicesUpdated, it->second,
                     it->second->GetServicesSync());
}

void GattClientManagerImpl::OnServicesAdded(
    const bluetooth_v2_shlib::Addr& addr,
    const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) {
  MAKE_SURE_IO_THREAD(OnServicesAdded, addr, services);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnServicesAdded(services);
  observers_->Notify(FROM_HERE, &Observer::OnServicesUpdated, it->second,
                     it->second->GetServicesSync());
}

void GattClientManagerImpl::RunQueuedConnectRequest() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (pending_connect_requests_.empty()) {
    return;
  }

  auto addr = pending_connect_requests_.front();
  while (!gatt_client_->Connect(addr)) {
    // If current request fails, run the next request
    LOG(ERROR) << "Connect failed";
    auto it = addr_to_device_.find(addr);
    if (it != addr_to_device_.end()) {
      it->second->SetConnected(false);
    }
    pending_connect_requests_.pop_front();

    if (pending_connect_requests_.empty()) {
      return;
    }

    addr = pending_connect_requests_.front();
  }

  connect_timeout_timer_.Start(
      FROM_HERE, kConnectTimeout,
      base::BindRepeating(&GattClientManagerImpl::OnConnectTimeout, weak_this_,
                          addr));
}

void GattClientManagerImpl::RunQueuedReadRemoteRssiRequest() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (pending_read_remote_rssi_requests_.empty()) {
    return;
  }

  auto addr = pending_read_remote_rssi_requests_.front();
  while (!gatt_client_->ReadRemoteRssi(addr)) {
    // If current request fails, run the next request
    LOG(ERROR) << "ReadRemoteRssi failed";
    auto it = addr_to_device_.find(addr);
    if (it != addr_to_device_.end()) {
      it->second->OnReadRemoteRssiComplete(false, 0);
    }
    pending_read_remote_rssi_requests_.pop_front();

    if (pending_read_remote_rssi_requests_.empty()) {
      return;
    }

    addr = pending_read_remote_rssi_requests_.front();
  }

  read_remote_rssi_timeout_timer_.Start(
      FROM_HERE, kReadRemoteRssiTimeout,
      base::BindRepeating(&GattClientManagerImpl::OnReadRemoteRssiTimeout,
                          weak_this_, addr));
}

void GattClientManagerImpl::OnConnectTimeout(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Get the last byte because whole address is PII.
  std::string addr_str = util::AddrLastByteString(addr);

  LOG(ERROR) << "Connect (" << addr_str << ")"
             << " timed out. Disconnecting";

  if (connected_devices_.find(addr) != connected_devices_.end()) {
    // Connect times out before OnGetServices is received.
    gatt_client_->Disconnect(addr);
  } else {
    // Connect times out before OnConnectChanged is received.
    RUN_ON_IO_THREAD(OnConnectChanged, addr, false /* status */,
                     false /* connected */);
  }
}

void GattClientManagerImpl::OnReadRemoteRssiTimeout(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Get the last byte because whole address is PII.
  std::string addr_str = util::AddrLastByteString(addr);

  LOG(ERROR) << "ReadRemoteRssi (" << addr_str << ")"
             << " timed out.";

  // ReadRemoteRssi times out before OnReadRemoteRssi is received.
  RUN_ON_IO_THREAD(OnReadRemoteRssi, addr, false /* status */, 0 /* rssi */);
}

// static
void GattClientManagerImpl::FinalizeOnIoThread(
    std::unique_ptr<base::WeakPtrFactory<GattClientManagerImpl>> weak_factory) {
  weak_factory->InvalidateWeakPtrs();
}

}  // namespace bluetooth
}  // namespace chromecast
