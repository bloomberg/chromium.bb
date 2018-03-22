// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_H_

#include <atomic>
#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/internal/public/bluetooth_v2_shlib/gatt.h"

namespace chromecast {
namespace bluetooth {

class GattClientManager;
class RemoteDescriptor;
class RemoteDevice;

// A proxy for a remote characteristic on a RemoteDevice. Unless otherwise
// specified, all callbacks are run on the caller's thread.
class RemoteCharacteristic
    : public base::RefCountedThreadSafe<RemoteCharacteristic> {
 public:
  using ReadCallback =
      base::OnceCallback<void(bool, const std::vector<uint8_t>&)>;
  using StatusCallback = base::OnceCallback<void(bool)>;

  // Return a list of all descriptors.
  std::vector<scoped_refptr<RemoteDescriptor>> GetDescriptors();

  // Retrieves the descriptor with |uuid|, or nullptr if it doesn't exist.
  scoped_refptr<RemoteDescriptor> GetDescriptorByUuid(
      const bluetooth_v2_shlib::Uuid& uuid);

  // Register or deregister from a notification. Calls |SetNotification| and
  // writes the cccd.
  void SetRegisterNotification(bool enable, StatusCallback cb);

  // Enable notifications for this characteristic. Client must still write to
  // the CCCD seperately (or use |SetRegisterNotification| instead).
  void SetNotification(bool enable, StatusCallback cb);

  // Read the characteristic with |auth_req|. When completed, |callback| will be
  // called.
  void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                ReadCallback callback);

  // Read the characteristic. Will retry if auth_req isn't met. When completed,
  // |callback| will be called.
  void Read(ReadCallback callback);

  // Write |value| to the characteristic with |auth_req| and |write_type|. When
  // completed, |callback| will be called.
  void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                 bluetooth_v2_shlib::Gatt::WriteType write_type,
                 const std::vector<uint8_t>& value,
                 StatusCallback callback);

  // Write |value| to the characteristic with |write_type|. Will retry if
  // auth_req isn't met. When completed, |callback| will be called.
  void Write(bluetooth_v2_shlib::Gatt::WriteType write_type,
             const std::vector<uint8_t>& value,
             StatusCallback callback);

  // Returns true if notifications are enabled.
  bool NotificationEnabled();

  const bluetooth_v2_shlib::Gatt::Characteristic& characteristic() const {
    return *characteristic_;
  }
  const bluetooth_v2_shlib::Uuid& uuid() const { return characteristic_->uuid; }
  uint16_t handle() const { return characteristic_->handle; }
  bluetooth_v2_shlib::Gatt::Permissions permissions() const {
    return characteristic_->permissions;
  }

  bluetooth_v2_shlib::Gatt::Properties properties() const {
    return characteristic_->properties;
  }

 private:
  friend class GattClientManager;
  friend class RemoteDevice;
  friend class RemoteService;
  friend class base::RefCountedThreadSafe<RemoteCharacteristic>;

  static std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteDescriptor>>
  CreateDescriptorMap(
      RemoteDevice* device,
      base::WeakPtr<GattClientManager> gatt_client_manager,
      const bluetooth_v2_shlib::Gatt::Characteristic* characteristic,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  RemoteCharacteristic(
      RemoteDevice* device,
      base::WeakPtr<GattClientManager> gatt_client_manager,
      const bluetooth_v2_shlib::Gatt::Characteristic* characteristic,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteCharacteristic();

  void OnConnectChanged(bool connected);
  void OnReadComplete(bool status, const std::vector<uint8_t>& value);
  void OnWriteComplete(bool status);

  // Weak reference to avoid refcount loop.
  RemoteDevice* const device_;
  const base::WeakPtr<GattClientManager> gatt_client_manager_;
  const bluetooth_v2_shlib::Gatt::Characteristic* const characteristic_;

  // All bluetooth_v2_shlib calls are run on this task_runner. All members must
  // be accessed on this task_runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  const std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteDescriptor>>
      uuid_to_descriptor_;

  ReadCallback read_callback_;
  StatusCallback write_callback_;

  std::atomic<bool> notification_enabled_{false};

  bool pending_read_ = false;
  bool pending_write_ = false;

  DISALLOW_COPY_AND_ASSIGN(RemoteCharacteristic);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_H_
