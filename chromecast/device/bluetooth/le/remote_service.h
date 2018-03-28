// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_SERVICE_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth {

class GattClientManager;
class RemoteCharacteristic;
class RemoteDevice;

// A proxy for a remote service on a RemoteDevice. Unless otherwise
// specified, all callbacks are run on the caller's thread.
class RemoteService : public base::RefCountedThreadSafe<RemoteService> {
 public:
  // Returns a list of characteristics in this service.
  void GetCharacteristics(
      base::OnceCallback<void(std::vector<scoped_refptr<RemoteCharacteristic>>)>
          cb);

  std::vector<scoped_refptr<RemoteCharacteristic>> GetCharacteristics();

  scoped_refptr<RemoteCharacteristic> GetCharacteristicByUuid(
      const bluetooth_v2_shlib::Uuid& uuid);

  const bluetooth_v2_shlib::Uuid& uuid() const { return service_.uuid; }
  uint16_t handle() const { return service_.handle; }
  bool primary() const { return service_.primary; }

 private:
  friend class RemoteDevice;
  friend class base::RefCountedThreadSafe<RemoteService>;

  static std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteCharacteristic>>
  CreateCharMap(RemoteDevice* remote_device,
                base::WeakPtr<GattClientManager> gatt_client_manager,
                const bluetooth_v2_shlib::Gatt::Service& service,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // May only be constructed by RemoteDevice.
  explicit RemoteService(
      RemoteDevice* remote_device,
      base::WeakPtr<GattClientManager> gatt_client_manager,
      const bluetooth_v2_shlib::Gatt::Service& service,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteService();

  const bluetooth_v2_shlib::Gatt::Service service_;

  const std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteCharacteristic>>
      uuid_to_characteristic_;

  DISALLOW_COPY_AND_ASSIGN(RemoteService);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_SERVICE_H_
