// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/device_impl.h"
#include "device/usb/type_converters.h"
#include "device/usb/usb_device.h"

namespace device {
namespace usb {

DeviceImpl::DeviceImpl(scoped_refptr<UsbDevice> device,
                       mojo::InterfaceRequest<Device> request)
    : binding_(this, request.Pass()),
      device_(device),
      info_(DeviceInfo::From(*device)) {
  // TODO(rockot/reillyg): Support more than just the current configuration.
  // Also, converting configuration info should be done in the TypeConverter,
  // but GetConfiguration() is non-const so we do it here for now.
  const UsbConfigDescriptor* config = device->GetConfiguration();
  info_->configurations = mojo::Array<ConfigurationInfoPtr>::New(0);
  if (config)
    info_->configurations.push_back(ConfigurationInfo::From(*config));
}

DeviceImpl::~DeviceImpl() {
}

void DeviceImpl::GetDeviceInfo(const GetDeviceInfoCallback& callback) {
  callback.Run(info_.Clone());
}

}  // namespace usb
}  // namespace device
