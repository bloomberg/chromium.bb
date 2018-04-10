// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_SERVICE_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth {

class RemoteCharacteristic;
class RemoteDevice;

// A proxy for a remote service on a RemoteDevice. Unless otherwise
// specified, all callbacks are run on the caller's thread.
class RemoteService : public base::RefCountedThreadSafe<RemoteService> {
 public:
  // Returns a list of characteristics in this service.
  virtual std::vector<scoped_refptr<RemoteCharacteristic>>
  GetCharacteristics() = 0;

  virtual scoped_refptr<RemoteCharacteristic> GetCharacteristicByUuid(
      const bluetooth_v2_shlib::Uuid& uuid) = 0;

  virtual const bluetooth_v2_shlib::Uuid& uuid() const = 0;
  virtual uint16_t handle() const = 0;
  virtual bool primary() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<RemoteService>;

  RemoteService() = default;
  virtual ~RemoteService() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteService);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_SERVICE_H_
