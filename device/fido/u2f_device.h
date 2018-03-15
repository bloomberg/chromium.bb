// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_DEVICE_H_
#define DEVICE_FIDO_U2F_DEVICE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace device {

// Device abstraction for an individual U2F device. A U2F device defines the
// standardized Register, Sign, and GetVersion methods.
class COMPONENT_EXPORT(DEVICE_FIDO) U2fDevice {
 public:
  enum class ProtocolVersion {
    U2F_V2,
    UNKNOWN,
  };

  using WinkCallback = base::OnceClosure;

  using DeviceCallback =
      base::OnceCallback<void(base::Optional<std::vector<uint8_t>> response)>;

  static constexpr auto kDeviceTimeout = base::TimeDelta::FromSeconds(3);

  U2fDevice();
  virtual ~U2fDevice();
  // Pure virtual function defined by each device type, implementing
  // the device communication transaction.
  virtual void DeviceTransact(std::vector<uint8_t> command,
                              DeviceCallback callback) = 0;
  virtual void TryWink(WinkCallback callback) = 0;
  virtual std::string GetId() const = 0;

 protected:
  virtual base::WeakPtr<U2fDevice> GetWeakPtr() = 0;

  DISALLOW_COPY_AND_ASSIGN(U2fDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_U2F_DEVICE_H_
