// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mojo/mock_permission_provider.h"

#include <stddef.h>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "device/usb/public/mojom/device.mojom.h"

using ::testing::Return;
using ::testing::_;

namespace device {
namespace usb {

const char MockPermissionProvider::kRestrictedSerialNumber[] = "no_permission";

MockPermissionProvider::MockPermissionProvider() : weak_factory_(this) {}

MockPermissionProvider::~MockPermissionProvider() = default;

bool MockPermissionProvider::HasDevicePermission(
    const mojom::UsbDeviceInfo& device_info) const {
  return device_info.serial_number ==
                 base::ASCIIToUTF16(kRestrictedSerialNumber)
             ? false
             : true;
}

base::WeakPtr<PermissionProvider> MockPermissionProvider::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace usb
}  // namespace device
