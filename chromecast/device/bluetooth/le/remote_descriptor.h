// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DESCRIPTOR_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DESCRIPTOR_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager.h"
#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth {

class RemoteDevice;
class RemoteDescriptor;

// A proxy for a remote descriptor on a RemoteDevice. Unless otherwise
// specified, all callbacks are run on the caller's thread.
class RemoteDescriptor : public base::RefCountedThreadSafe<RemoteDescriptor> {
 public:
  static constexpr uint8_t kEnableNotificationValue[] = {0x01, 0x00};
  static constexpr uint8_t kDisableNotificationValue[] = {0x00, 0x00};
  static const bluetooth_v2_shlib::Uuid kCccdUuid;

  using ReadCallback =
      base::OnceCallback<void(bool, const std::vector<uint8_t>&)>;
  using StatusCallback = base::OnceCallback<void(bool)>;

  // Read the descriptor with |auth_req|. When completed, |callback| will be
  // called.
  void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                ReadCallback callback);

  // Read the descriptor. When completed, |callback| will be called.
  void Read(ReadCallback callback);

  // Write |value| to the descriptor with |auth_req|. When completed, |callback|
  // will be called.
  void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                 const std::vector<uint8_t>& value,
                 StatusCallback callback);

  // Write |value| to the descriptor. Will retry if auth_req isn't met. When
  // completed, |callback| will be called.
  void Write(const std::vector<uint8_t>& value, StatusCallback callback);

  const bluetooth_v2_shlib::Gatt::Descriptor& descriptor() const {
    return *descriptor_;
  }
  const bluetooth_v2_shlib::Uuid uuid() const { return descriptor_->uuid; }
  uint16_t handle() const { return descriptor_->handle; }
  bluetooth_v2_shlib::Gatt::Permissions permissions() const {
    return descriptor_->permissions;
  }

 private:
  friend class GattClientManager;
  friend class RemoteCharacteristic;
  friend class RemoteDevice;
  friend class base::RefCountedThreadSafe<RemoteDescriptor>;

  RemoteDescriptor(RemoteDevice* device,
                   base::WeakPtr<GattClientManager> gatt_client_manager,
                   const bluetooth_v2_shlib::Gatt::Descriptor* characteristic,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteDescriptor();

  void OnConnectChanged(bool connected);
  void OnReadComplete(bool status, const std::vector<uint8_t>& value);
  void OnWriteComplete(bool status);

  RemoteDevice* const device_;
  const base::WeakPtr<GattClientManager> gatt_client_manager_;
  const bluetooth_v2_shlib::Gatt::Descriptor* const descriptor_;

  // All bluetooth_v2_shlib calls are run on this task_runner. All members must
  // be accessed on this task_runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  ReadCallback read_callback_;
  StatusCallback write_callback_;

  bool pending_read_ = false;
  bool pending_write_ = false;

  DISALLOW_COPY_AND_ASSIGN(RemoteDescriptor);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DESCRIPTOR_H_
