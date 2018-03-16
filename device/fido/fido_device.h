// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DEVICE_H_
#define DEVICE_FIDO_FIDO_DEVICE_H_

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

// Device abstraction for an individual CTAP1.0/CTAP2.0 device.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDevice {
 public:
  using WinkCallback = base::OnceClosure;
  using DeviceCallback =
      base::OnceCallback<void(base::Optional<std::vector<uint8_t>> response)>;

  // Internal state machine states.
  enum class State { kInit, kConnected, kBusy, kReady, kDeviceError };

  FidoDevice();
  virtual ~FidoDevice();
  // Pure virtual function defined by each device type, implementing
  // the device communication transaction. The function must not immediately
  // call (i.e. hairpin) |callback|.
  virtual void DeviceTransact(std::vector<uint8_t> command,
                              DeviceCallback callback) = 0;
  virtual void TryWink(WinkCallback callback) = 0;
  virtual std::string GetId() const = 0;

 protected:
  virtual base::WeakPtr<FidoDevice> GetWeakPtr() = 0;

  DISALLOW_COPY_AND_ASSIGN(FidoDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DEVICE_H_
