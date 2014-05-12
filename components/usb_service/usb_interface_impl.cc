// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/usb_service/usb_interface_impl.h"

#include "base/logging.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace usb_service {

UsbEndpointDescriptorImpl::UsbEndpointDescriptorImpl(
    scoped_refptr<const UsbConfigDescriptor> config,
    PlatformUsbEndpointDescriptor descriptor)
    : config_(config), descriptor_(descriptor) {
}

UsbEndpointDescriptorImpl::~UsbEndpointDescriptorImpl() {
}

int UsbEndpointDescriptorImpl::GetAddress() const {
  return descriptor_->bEndpointAddress & LIBUSB_ENDPOINT_ADDRESS_MASK;
}

UsbEndpointDirection UsbEndpointDescriptorImpl::GetDirection() const {
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

int UsbEndpointDescriptorImpl::GetMaximumPacketSize() const {
  return descriptor_->wMaxPacketSize;
}

UsbSynchronizationType
    UsbEndpointDescriptorImpl::GetSynchronizationType() const {
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

UsbTransferType UsbEndpointDescriptorImpl::GetTransferType() const {
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

UsbUsageType UsbEndpointDescriptorImpl::GetUsageType() const {
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

int UsbEndpointDescriptorImpl::GetPollingInterval() const {
  return descriptor_->bInterval;
}

UsbInterfaceAltSettingDescriptorImpl::UsbInterfaceAltSettingDescriptorImpl(
    scoped_refptr<const UsbConfigDescriptor> config,
    PlatformUsbInterfaceDescriptor descriptor)
    : config_(config), descriptor_(descriptor) {
}

UsbInterfaceAltSettingDescriptorImpl::~UsbInterfaceAltSettingDescriptorImpl() {
}

size_t UsbInterfaceAltSettingDescriptorImpl::GetNumEndpoints() const {
  return descriptor_->bNumEndpoints;
}

scoped_refptr<const UsbEndpointDescriptor>
UsbInterfaceAltSettingDescriptorImpl::GetEndpoint(size_t index) const {
  return new UsbEndpointDescriptorImpl(config_, &descriptor_->endpoint[index]);
}

int UsbInterfaceAltSettingDescriptorImpl::GetInterfaceNumber() const {
  return descriptor_->bInterfaceNumber;
}

int UsbInterfaceAltSettingDescriptorImpl::GetAlternateSetting() const {
  return descriptor_->bAlternateSetting;
}

int UsbInterfaceAltSettingDescriptorImpl::GetInterfaceClass() const {
  return descriptor_->bInterfaceClass;
}

int UsbInterfaceAltSettingDescriptorImpl::GetInterfaceSubclass() const {
  return descriptor_->bInterfaceSubClass;
}

int UsbInterfaceAltSettingDescriptorImpl::GetInterfaceProtocol() const {
  return descriptor_->bInterfaceProtocol;
}

UsbInterfaceDescriptorImpl::UsbInterfaceDescriptorImpl(
    scoped_refptr<const UsbConfigDescriptor> config,
    PlatformUsbInterface usbInterface)
    : config_(config), interface_(usbInterface) {
}

UsbInterfaceDescriptorImpl::~UsbInterfaceDescriptorImpl() {
}

size_t UsbInterfaceDescriptorImpl::GetNumAltSettings() const {
  return interface_->num_altsetting;
}

scoped_refptr<const UsbInterfaceAltSettingDescriptor>
UsbInterfaceDescriptorImpl::GetAltSetting(size_t index) const {
  return new UsbInterfaceAltSettingDescriptorImpl(
      config_, &interface_->altsetting[index]);
}

UsbConfigDescriptorImpl::UsbConfigDescriptorImpl(
    PlatformUsbConfigDescriptor config)
    : config_(config) {
  DCHECK(config);
}

UsbConfigDescriptorImpl::~UsbConfigDescriptorImpl() {
  libusb_free_config_descriptor(config_);
}

size_t UsbConfigDescriptorImpl::GetNumInterfaces() const {
  return config_->bNumInterfaces;
}

scoped_refptr<const UsbInterfaceDescriptor>
    UsbConfigDescriptorImpl::GetInterface(size_t index) const {
  return new UsbInterfaceDescriptorImpl(this, &config_->interface[index]);
}

}  // namespace usb_service
