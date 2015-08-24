// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/usb/web_usb_device_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/usb/type_converters.h"
#include "device/devices_app/public/cpp/constants.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceInfo.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBTransferInfo.h"

namespace content {

namespace {

const char kNotImplementedError[] = "Not implemented.";

template <typename ResultType>
void RejectAsNotImplemented(
    blink::WebCallbacks<ResultType, const blink::WebUSBError&>* callbacks) {
  callbacks->onError(
      blink::WebUSBError(blink::WebUSBError::Error::Service,
                         base::UTF8ToUTF16(kNotImplementedError)));
  delete callbacks;
}

}  // namespace

WebUSBDeviceImpl::WebUSBDeviceImpl(device::usb::DeviceManagerPtr device_manager,
                                   const blink::WebUSBDeviceInfo& device_info)
    : device_manager_(device_manager.Pass()),
      device_info_(device_info),
      weak_factory_(this) {}

WebUSBDeviceImpl::~WebUSBDeviceImpl() {}

const blink::WebUSBDeviceInfo& WebUSBDeviceImpl::info() const {
  return device_info_;
}

void WebUSBDeviceImpl::open(blink::WebUSBDeviceOpenCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

void WebUSBDeviceImpl::close(blink::WebUSBDeviceCloseCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

void WebUSBDeviceImpl::setConfiguration(
    uint8_t configuration_value,
    blink::WebUSBDeviceSetConfigurationCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

void WebUSBDeviceImpl::claimInterface(
    uint8_t interface_number,
    blink::WebUSBDeviceClaimInterfaceCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

void WebUSBDeviceImpl::releaseInterface(
    uint8_t interface_number,
    blink::WebUSBDeviceReleaseInterfaceCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

void WebUSBDeviceImpl::setInterface(
    uint8_t interface_number,
    uint8_t alternate_setting,
    blink::WebUSBDeviceSetInterfaceAlternateSettingCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

void WebUSBDeviceImpl::clearHalt(
    uint8_t endpoint_number,
    blink::WebUSBDeviceClearHaltCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

void WebUSBDeviceImpl::controlTransfer(
    const blink::WebUSBDevice::ControlTransferParameters& parameters,
    uint8_t* data,
    size_t data_size,
    unsigned int timeout,
    blink::WebUSBDeviceControlTransferCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

void WebUSBDeviceImpl::transfer(
    blink::WebUSBDevice::TransferDirection direction,
    uint8_t endpoint_number,
    uint8_t* data,
    size_t data_size,
    unsigned int timeout,
    blink::WebUSBDeviceBulkTransferCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

void WebUSBDeviceImpl::reset(blink::WebUSBDeviceResetCallbacks* callbacks) {
  RejectAsNotImplemented(callbacks);
}

}  // namespace content
