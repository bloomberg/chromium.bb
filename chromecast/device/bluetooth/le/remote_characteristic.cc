// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/remote_characteristic.h"

#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "chromecast/device/bluetooth/le/remote_device.h"

#define EXEC_CB_AND_RET(cb, ret, ...)        \
  do {                                       \
    if (cb) {                                \
      std::move(cb).Run(ret, ##__VA_ARGS__); \
    }                                        \
    return;                                  \
  } while (0)

#define RUN_ON_IO_THREAD(method, ...) \
  io_task_runner_->PostTask(          \
      FROM_HERE,                      \
      base::BindOnce(&RemoteCharacteristic::method, this, ##__VA_ARGS__));

#define MAKE_SURE_IO_THREAD(method, ...)            \
  DCHECK(io_task_runner_);                          \
  if (!io_task_runner_->BelongsToCurrentThread()) { \
    RUN_ON_IO_THREAD(method, ##__VA_ARGS__)         \
    return;                                         \
  }

namespace chromecast {
namespace bluetooth {

namespace {
std::vector<uint8_t> GetDescriptorNotificationValue(bool enable) {
  if (enable) {
    return std::vector<uint8_t>(
        std::begin(bluetooth::RemoteDescriptor::kEnableNotificationValue),
        std::end(bluetooth::RemoteDescriptor::kEnableNotificationValue));
  }
  return std::vector<uint8_t>(
      std::begin(bluetooth::RemoteDescriptor::kDisableNotificationValue),
      std::end(bluetooth::RemoteDescriptor::kDisableNotificationValue));
}
}  // namespace

// static
std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteDescriptor>>
RemoteCharacteristic::CreateDescriptorMap(
    RemoteDevice* device,
    base::WeakPtr<GattClientManager> gatt_client_manager,
    const bluetooth_v2_shlib::Gatt::Characteristic* characteristic,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteDescriptor>> ret;
  for (const auto& descriptor : characteristic->descriptors) {
    ret[descriptor.uuid] = new RemoteDescriptor(device, gatt_client_manager,
                                                &descriptor, io_task_runner);
  }

  return ret;
}

RemoteCharacteristic::RemoteCharacteristic(
    RemoteDevice* device,
    base::WeakPtr<GattClientManager> gatt_client_manager,
    const bluetooth_v2_shlib::Gatt::Characteristic* characteristic,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : device_(device),
      gatt_client_manager_(gatt_client_manager),
      characteristic_(characteristic),
      io_task_runner_(io_task_runner),
      uuid_to_descriptor_(CreateDescriptorMap(device,
                                              gatt_client_manager,
                                              characteristic_,
                                              io_task_runner)) {
  DCHECK(gatt_client_manager);
  DCHECK(characteristic);
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

RemoteCharacteristic::~RemoteCharacteristic() = default;

std::vector<scoped_refptr<RemoteDescriptor>>
RemoteCharacteristic::GetDescriptors() {
  std::vector<scoped_refptr<RemoteDescriptor>> ret;
  ret.reserve(uuid_to_descriptor_.size());
  for (const auto& pair : uuid_to_descriptor_) {
    ret.push_back(pair.second);
  }

  return ret;
}

scoped_refptr<RemoteDescriptor> RemoteCharacteristic::GetDescriptorByUuid(
    const bluetooth_v2_shlib::Uuid& uuid) {
  auto it = uuid_to_descriptor_.find(uuid);
  if (it == uuid_to_descriptor_.end()) {
    return nullptr;
  }

  return it->second;
}

void RemoteCharacteristic::SetRegisterNotification(bool enable,
                                                   StatusCallback cb) {
  MAKE_SURE_IO_THREAD(SetRegisterNotification, enable,
                      BindToCurrentThread(std::move(cb)));
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(cb, false);
  }

  if (!gatt_client_manager_->gatt_client()->SetCharacteristicNotification(
          device_->addr(), *characteristic_, enable)) {
    LOG(ERROR) << "Set characteristic notification failed";
    EXEC_CB_AND_RET(cb, false);
  }

  auto it = uuid_to_descriptor_.find(RemoteDescriptor::kCccdUuid);
  if (it == uuid_to_descriptor_.end()) {
    LOG(ERROR) << "No CCCD found";
    EXEC_CB_AND_RET(cb, false);
  }

  it->second->WriteAuth(bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_NONE,
                        GetDescriptorNotificationValue(enable), std::move(cb));
}

void RemoteCharacteristic::SetNotification(bool enable, StatusCallback cb) {
  MAKE_SURE_IO_THREAD(SetNotification, enable,
                      BindToCurrentThread(std::move(cb)));
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(cb, false);
  }
  if (!gatt_client_manager_->gatt_client()->SetCharacteristicNotification(
          device_->addr(), *characteristic_, enable)) {
    LOG(ERROR) << "Set characteristic notification failed";
    EXEC_CB_AND_RET(cb, false);
  }

  notification_enabled_ = enable;
  EXEC_CB_AND_RET(cb, true);
}

void RemoteCharacteristic::ReadAuth(
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    ReadCallback callback) {
  MAKE_SURE_IO_THREAD(ReadAuth, auth_req,
                      BindToCurrentThread(std::move(callback)));
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(callback, false, {});
  }
  if (pending_read_) {
    LOG(ERROR) << "Read already pending";
    EXEC_CB_AND_RET(callback, false, {});
  }

  if (!gatt_client_manager_->gatt_client()->ReadCharacteristic(
          device_->addr(), *characteristic_, auth_req)) {
    EXEC_CB_AND_RET(callback, false, {});
  }
  pending_read_ = true;
  read_callback_ = std::move(callback);
}

void RemoteCharacteristic::Read(ReadCallback callback) {
  ReadAuth(bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_INVALID,
           std::move(callback));
}

void RemoteCharacteristic::WriteAuth(
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    bluetooth_v2_shlib::Gatt::WriteType write_type,
    const std::vector<uint8_t>& value,
    StatusCallback callback) {
  MAKE_SURE_IO_THREAD(WriteAuth, auth_req, write_type, value,
                      BindToCurrentThread(std::move(callback)));
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(callback, false);
  }
  if (pending_write_) {
    LOG(ERROR) << "Write already pending";
    EXEC_CB_AND_RET(callback, false);
  }

  if (!gatt_client_manager_->gatt_client()->WriteCharacteristic(
          device_->addr(), *characteristic_, auth_req, write_type, value)) {
    EXEC_CB_AND_RET(callback, false);
  }

  pending_write_ = true;
  write_callback_ = std::move(callback);
}

void RemoteCharacteristic::Write(bluetooth_v2_shlib::Gatt::WriteType write_type,
                                 const std::vector<uint8_t>& value,
                                 StatusCallback callback) {
  return WriteAuth(bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_NONE, write_type,
                   value, std::move(callback));
}

bool RemoteCharacteristic::NotificationEnabled() {
  return notification_enabled_;
}

void RemoteCharacteristic::OnConnectChanged(bool connected) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (connected) {
    return;
  }

  pending_read_ = false;
  pending_write_ = false;

  if (read_callback_) {
    LOG(ERROR) << "Read failed: Device disconnected";
    std::move(read_callback_).Run(false, std::vector<uint8_t>());
  }

  if (write_callback_) {
    LOG(ERROR) << "Write failed: Device disconnected";
    std::move(write_callback_).Run(false);
  }
}

void RemoteCharacteristic::OnReadComplete(bool status,
                                          const std::vector<uint8_t>& value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  pending_read_ = false;
  if (read_callback_) {
    std::move(read_callback_).Run(status, value);
  }
}

void RemoteCharacteristic::OnWriteComplete(bool status) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  pending_write_ = false;
  if (write_callback_)
    std::move(write_callback_).Run(status);
}

}  // namespace bluetooth
}  // namespace chromecast
