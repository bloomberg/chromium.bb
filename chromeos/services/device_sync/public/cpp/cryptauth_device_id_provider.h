// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CRYPTAUTH_DEVICE_ID_PROVIDER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CRYPTAUTH_DEVICE_ID_PROVIDER_H_

#include <string>

#include "base/macros.h"

namespace chromeos {

namespace device_sync {

// Provides the ID of the current device. In this context, "device ID" refers to
// the |long_device_id| field of the cryptauth::GcmDeviceInfo proto which is
// sent to the CryptAuth back-end during device enrollment.
class CryptAuthDeviceIdProvider {
 public:
  CryptAuthDeviceIdProvider() = default;
  virtual ~CryptAuthDeviceIdProvider() = default;

  virtual std::string GetDeviceId() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptAuthDeviceIdProvider);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CRYPTAUTH_DEVICE_ID_PROVIDER_H_
