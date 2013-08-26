// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_interface.h"

#include "base/logging.h"
#include "third_party/libusb/src/libusb/libusb.h"

UsbEndpointDescriptor::UsbEndpointDescriptor(
    scoped_refptr<const UsbConfigDescriptor> config,
    PlatformUsbEndpointDescriptor descriptor)
    : config_(config), descriptor_(descriptor) {
}

UsbEndpointDescriptor::~UsbEndpointDescriptor() {}

int UsbEndpointDescriptor::GetAddress() const {
  return descriptor_->bEndpointAddress & LIBUSB_ENDPOINT_ADDRESS_MASK;
}

UsbEndpointDirection UsbEndpointDescriptor::GetDirection() const {
  switch (descriptor_->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) {
    case LIBUSB_ENDPOINT_IN:
      return USB_DIRECTION_INBOUND;
    case LIBUSB_ENDPOINT_OUT:
      return USB_DIRECTION_OUTBOUND;
    default:
      NOTREACHED();
      return USB_DIRECTION_INBOUND;
  }
}

int UsbEndpointDescriptor::GetMaximumPacketSize() const {
  return descriptor_->wMaxPacketSize;
}

UsbSynchronizationType UsbEndpointDescriptor::GetSynchronizationType() const {
  switch (descriptor_->bmAttributes & LIBUSB_ISO_SYNC_TYPE_MASK) {
    case LIBUSB_ISO_SYNC_TYPE_NONE:
      return USB_SYNCHRONIZATION_NONE;
    case LIBUSB_ISO_SYNC_TYPE_ASYNC:
      return USB_SYNCHRONIZATION_ASYNCHRONOUS;
    case LIBUSB_ISO_SYNC_TYPE_ADAPTIVE:
      return USB_SYNCHRONIZATION_ADAPTIVE;
    case LIBUSB_ISO_SYNC_TYPE_SYNC:
      return USB_SYNCHRONIZATION_SYNCHRONOUS;
    default:
      NOTREACHED();
      return USB_SYNCHRONIZATION_NONE;
  }
}

UsbTransferType UsbEndpointDescriptor::GetTransferType() const {
  switch (descriptor_->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
      return USB_TRANSFER_CONTROL;
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
      return USB_TRANSFER_ISOCHRONOUS;
    case LIBUSB_TRANSFER_TYPE_BULK:
      return USB_TRANSFER_BULK;
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
      return USB_TRANSFER_INTERRUPT;
    default:
      NOTREACHED();
      return USB_TRANSFER_CONTROL;
  }
}

UsbUsageType UsbEndpointDescriptor::GetUsageType() const {
  switch (descriptor_->bmAttributes & LIBUSB_ISO_USAGE_TYPE_MASK) {
    case LIBUSB_ISO_USAGE_TYPE_DATA:
      return USB_USAGE_DATA;
    case LIBUSB_ISO_USAGE_TYPE_FEEDBACK:
      return USB_USAGE_FEEDBACK;
    case LIBUSB_ISO_USAGE_TYPE_IMPLICIT:
      return USB_USAGE_EXPLICIT_FEEDBACK;
    default:
      NOTREACHED();
      return USB_USAGE_DATA;
  }
}

int UsbEndpointDescriptor::GetPollingInterval() const {
  return descriptor_->bInterval;
}

UsbInterfaceAltSettingDescriptor::UsbInterfaceAltSettingDescriptor(
    scoped_refptr<const UsbConfigDescriptor> config,
    PlatformUsbInterfaceDescriptor descriptor)
    : config_(config), descriptor_(descriptor) {
}

UsbInterfaceAltSettingDescriptor::~UsbInterfaceAltSettingDescriptor() {}

size_t UsbInterfaceAltSettingDescriptor::GetNumEndpoints() const {
  return descriptor_->bNumEndpoints;
}

scoped_refptr<const UsbEndpointDescriptor>
    UsbInterfaceAltSettingDescriptor::GetEndpoint(size_t index) const {
  return new UsbEndpointDescriptor(config_, &descriptor_->endpoint[index]);
}

int UsbInterfaceAltSettingDescriptor::GetInterfaceNumber() const {
  return descriptor_->bInterfaceNumber;
}

int UsbInterfaceAltSettingDescriptor::GetAlternateSetting() const {
  return descriptor_->bAlternateSetting;
}

int UsbInterfaceAltSettingDescriptor::GetInterfaceClass() const {
  return descriptor_->bInterfaceClass;
}

int UsbInterfaceAltSettingDescriptor::GetInterfaceSubclass() const {
  return descriptor_->bInterfaceSubClass;
}

int UsbInterfaceAltSettingDescriptor::GetInterfaceProtocol() const {
  return descriptor_->bInterfaceProtocol;
}

UsbInterfaceDescriptor::UsbInterfaceDescriptor(
    scoped_refptr<const UsbConfigDescriptor> config,
    PlatformUsbInterface usbInterface)
    : config_(config), interface_(usbInterface) {
}

UsbInterfaceDescriptor::~UsbInterfaceDescriptor() {}

size_t UsbInterfaceDescriptor::GetNumAltSettings() const {
  return interface_->num_altsetting;
}

scoped_refptr<const UsbInterfaceAltSettingDescriptor>
    UsbInterfaceDescriptor::GetAltSetting(size_t index) const {
  return new UsbInterfaceAltSettingDescriptor(config_,
                                              &interface_->altsetting[index]);
}

UsbConfigDescriptor::UsbConfigDescriptor(PlatformUsbConfigDescriptor config)
    : config_(config) {
}

UsbConfigDescriptor::~UsbConfigDescriptor() {
  if (config_ != NULL) {
    libusb_free_config_descriptor(config_);
    config_ = NULL;
  }
}

size_t UsbConfigDescriptor::GetNumInterfaces() const {
  return config_->bNumInterfaces;
}

scoped_refptr<const UsbInterfaceDescriptor>
    UsbConfigDescriptor::GetInterface(size_t index) const {
  return new UsbInterfaceDescriptor(this, &config_->interface[index]);
}
