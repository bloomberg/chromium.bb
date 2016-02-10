// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_USB_WEB_USB_DEVICE_IMPL_H_
#define CONTENT_RENDERER_USB_WEB_USB_DEVICE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/usb/public/interfaces/device.mojom.h"
#include "device/usb/public/interfaces/device_manager.mojom.h"
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDevice.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceInfo.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBError.h"

namespace mojo {
class Shell;
}

namespace content {

class WebUSBDeviceImpl : public blink::WebUSBDevice {
 public:
  WebUSBDeviceImpl(device::usb::DevicePtr device,
                   const blink::WebUSBDeviceInfo& device_info);
  ~WebUSBDeviceImpl() override;

 private:
  // blink::WebUSBDevice implementation:
  const blink::WebUSBDeviceInfo& info() const override;
  void open(blink::WebUSBDeviceOpenCallbacks* callbacks) override;
  void close(blink::WebUSBDeviceCloseCallbacks* callbacks) override;
  void getConfiguration(
      blink::WebUSBDeviceGetConfigurationCallbacks* callbacks) override;
  void setConfiguration(
      uint8_t configuration_value,
      blink::WebUSBDeviceSetConfigurationCallbacks* callbacks) override;
  void claimInterface(
      uint8_t interface_number,
      blink::WebUSBDeviceClaimInterfaceCallbacks* callbacks) override;
  void releaseInterface(
      uint8_t interface_number,
      blink::WebUSBDeviceReleaseInterfaceCallbacks* callbacks) override;
  void setInterface(uint8_t interface_number,
                    uint8_t alternate_setting,
                    blink::WebUSBDeviceSetInterfaceAlternateSettingCallbacks*
                        callbacks) override;
  void clearHalt(uint8_t endpoint_number,
                 blink::WebUSBDeviceClearHaltCallbacks* callbacks) override;
  void controlTransfer(
      const blink::WebUSBDevice::ControlTransferParameters& parameters,
      uint8_t* data,
      size_t data_size,
      unsigned int timeout,
      blink::WebUSBDeviceTransferCallbacks* callbacks) override;
  void transfer(blink::WebUSBDevice::TransferDirection direction,
                uint8_t endpoint_number,
                uint8_t* data,
                size_t data_size,
                unsigned int timeout,
                blink::WebUSBDeviceTransferCallbacks* callbacks) override;
  void isochronousTransfer(
      blink::WebUSBDevice::TransferDirection direction,
      uint8_t endpoint_number,
      uint8_t* data,
      size_t data_size,
      blink::WebVector<uint32_t> packet_lengths,
      unsigned int timeout,
      blink::WebUSBDeviceTransferCallbacks* callbacks) override;
  void reset(blink::WebUSBDeviceResetCallbacks* callbacks) override;

  device::usb::DevicePtr device_;
  blink::WebUSBDeviceInfo device_info_;

  base::WeakPtrFactory<WebUSBDeviceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUSBDeviceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_USB_WEB_USB_DEVICE_IMPL_H_
