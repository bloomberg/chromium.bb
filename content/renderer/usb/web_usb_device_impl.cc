// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/usb/web_usb_device_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/scoped_web_callbacks.h"
#include "content/renderer/usb/type_converters.h"
#include "device/devices_app/public/cpp/constants.h"
#include "mojo/shell/public/cpp/connect.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceInfo.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBTransferInfo.h"

namespace content {

namespace {

const char kClaimInterfaceFailed[] = "Unable to claim interface.";
const char kClearHaltFailed[] = "Unable to clear endpoint.";
const char kDeviceNoAccess[] = "Access denied.";
const char kDeviceNotConfigured[] = "Device not configured.";
const char kDeviceUnavailable[] = "Device unavailable.";
const char kDeviceResetFailed[] = "Unable to reset the device.";
const char kReleaseInterfaceFailed[] = "Unable to release interface.";
const char kSetConfigurationFailed[] = "Unable to set device configuration.";
const char kSetInterfaceFailed[] = "Unable to set device interface.";
const char kTransferFailed[] = "Transfer failed.";

// Generic default rejection handler for any WebUSB callbacks type. Assumes
// |CallbacksType| is a blink::WebCallbacks<T, const blink::WebUSBError&>
// for any type |T|.
template <typename CallbacksType>
void RejectWithError(const blink::WebUSBError& error,
                     scoped_ptr<CallbacksType> callbacks) {
  callbacks->onError(error);
}

template <typename CallbacksType>
void RejectWithTransferError(scoped_ptr<CallbacksType> callbacks) {
  RejectWithError(blink::WebUSBError(blink::WebUSBError::Error::Network,
                                     base::ASCIIToUTF16(kTransferFailed)),
                  std::move(callbacks));
}

// Create a new ScopedWebCallbacks for WebUSB device callbacks, defaulting to
// a "device unavailable" rejection.
template <typename CallbacksType>
ScopedWebCallbacks<CallbacksType> MakeScopedUSBCallbacks(
    CallbacksType* callbacks) {
  return make_scoped_web_callbacks(
      callbacks,
      base::Bind(&RejectWithError<CallbacksType>,
                 blink::WebUSBError(blink::WebUSBError::Error::NotFound,
                                    base::ASCIIToUTF16(kDeviceUnavailable))));
}

void OnOpenDevice(
    ScopedWebCallbacks<blink::WebUSBDeviceOpenCallbacks> callbacks,
    device::usb::OpenDeviceError error) {
  auto scoped_callbacks = callbacks.PassCallbacks();
  switch(error) {
    case device::usb::OPEN_DEVICE_ERROR_OK:
      scoped_callbacks->onSuccess();
      break;
    case device::usb::OPEN_DEVICE_ERROR_ACCESS_DENIED:
      scoped_callbacks->onError(blink::WebUSBError(
          blink::WebUSBError::Error::Security,
          base::ASCIIToUTF16(kDeviceNoAccess)));
      break;
    default:
      NOTREACHED();
  }
}

void OnDeviceClosed(
    ScopedWebCallbacks<blink::WebUSBDeviceCloseCallbacks> callbacks) {
  callbacks.PassCallbacks()->onSuccess();
}

void OnGetConfiguration(
    ScopedWebCallbacks<blink::WebUSBDeviceGetConfigurationCallbacks> callbacks,
    uint8_t configuration_value) {
  auto scoped_callbacks = callbacks.PassCallbacks();
  if (configuration_value == 0) {
    RejectWithError(blink::WebUSBError(blink::WebUSBError::Error::NotFound,
                                       kDeviceNotConfigured),
                    std::move(scoped_callbacks));
  } else {
    scoped_callbacks->onSuccess(configuration_value);
  }
}

void HandlePassFailDeviceOperation(
    ScopedWebCallbacks<blink::WebCallbacks<void, const blink::WebUSBError&>>
        callbacks,
    const std::string& failure_message,
    bool success) {
  auto scoped_callbacks = callbacks.PassCallbacks();
  if (success) {
    scoped_callbacks->onSuccess();
  } else {
    RejectWithError(blink::WebUSBError(blink::WebUSBError::Error::Network,
                                       base::ASCIIToUTF16(failure_message)),
                    std::move(scoped_callbacks));
  }
}

void OnTransferIn(
    ScopedWebCallbacks<blink::WebUSBDeviceControlTransferCallbacks> callbacks,
    device::usb::TransferStatus status,
    mojo::Array<uint8_t> data) {
  auto scoped_callbacks = callbacks.PassCallbacks();
  if (status != device::usb::TRANSFER_STATUS_COMPLETED) {
    RejectWithTransferError(std::move(scoped_callbacks));
    return;
  }
  scoped_ptr<blink::WebUSBTransferInfo> info(new blink::WebUSBTransferInfo());
  info->status = blink::WebUSBTransferInfo::Status::Ok;
  info->data.assign(data);
  scoped_callbacks->onSuccess(adoptWebPtr(info.release()));
}

void OnTransferOut(
    ScopedWebCallbacks<blink::WebUSBDeviceControlTransferCallbacks> callbacks,
    size_t bytes_written,
    device::usb::TransferStatus status) {
  auto scoped_callbacks = callbacks.PassCallbacks();
  scoped_ptr<blink::WebUSBTransferInfo> info(new blink::WebUSBTransferInfo());
  switch (status) {
    case device::usb::TRANSFER_STATUS_COMPLETED:
      info->status = blink::WebUSBTransferInfo::Status::Ok;
      break;
    case device::usb::TRANSFER_STATUS_STALLED:
      info->status = blink::WebUSBTransferInfo::Status::Stall;
      break;
    case device::usb::TRANSFER_STATUS_BABBLE:
      info->status = blink::WebUSBTransferInfo::Status::Babble;
      break;
    default:
      RejectWithTransferError(std::move(scoped_callbacks));
      return;
  }

  // TODO(rockot): Device::ControlTransferOut should expose the number of bytes
  // actually transferred so we can send it from here.
  info->bytesWritten = bytes_written;
  scoped_callbacks->onSuccess(adoptWebPtr(info.release()));
}

}  // namespace

WebUSBDeviceImpl::WebUSBDeviceImpl(device::usb::DevicePtr device,
                                   const blink::WebUSBDeviceInfo& device_info)
    : device_(std::move(device)),
      device_info_(device_info),
      weak_factory_(this) {}

WebUSBDeviceImpl::~WebUSBDeviceImpl() {}

const blink::WebUSBDeviceInfo& WebUSBDeviceImpl::info() const {
  return device_info_;
}

void WebUSBDeviceImpl::open(blink::WebUSBDeviceOpenCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_->Open(base::Bind(&OnOpenDevice, base::Passed(&scoped_callbacks)));
}

void WebUSBDeviceImpl::close(blink::WebUSBDeviceCloseCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_->Close(base::Bind(&OnDeviceClosed, base::Passed(&scoped_callbacks)));
}

void WebUSBDeviceImpl::getConfiguration(
    blink::WebUSBDeviceGetConfigurationCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_->GetConfiguration(
      base::Bind(&OnGetConfiguration, base::Passed(&scoped_callbacks)));
}

void WebUSBDeviceImpl::setConfiguration(
    uint8_t configuration_value,
    blink::WebUSBDeviceSetConfigurationCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_->SetConfiguration(
      configuration_value,
      base::Bind(&HandlePassFailDeviceOperation,
                 base::Passed(&scoped_callbacks), kSetConfigurationFailed));
}

void WebUSBDeviceImpl::claimInterface(
    uint8_t interface_number,
    blink::WebUSBDeviceClaimInterfaceCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_->ClaimInterface(
      interface_number,
      base::Bind(&HandlePassFailDeviceOperation,
                 base::Passed(&scoped_callbacks), kClaimInterfaceFailed));
}

void WebUSBDeviceImpl::releaseInterface(
    uint8_t interface_number,
    blink::WebUSBDeviceReleaseInterfaceCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_->ReleaseInterface(
      interface_number,
      base::Bind(&HandlePassFailDeviceOperation,
                 base::Passed(&scoped_callbacks), kReleaseInterfaceFailed));
}

void WebUSBDeviceImpl::setInterface(
    uint8_t interface_number,
    uint8_t alternate_setting,
    blink::WebUSBDeviceSetInterfaceAlternateSettingCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_->SetInterfaceAlternateSetting(
      interface_number, alternate_setting,
      base::Bind(&HandlePassFailDeviceOperation,
                 base::Passed(&scoped_callbacks), kSetInterfaceFailed));
}

void WebUSBDeviceImpl::clearHalt(
    uint8_t endpoint_number,
    blink::WebUSBDeviceClearHaltCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_->ClearHalt(
      endpoint_number,
      base::Bind(&HandlePassFailDeviceOperation,
                 base::Passed(&scoped_callbacks), kClearHaltFailed));
}

void WebUSBDeviceImpl::controlTransfer(
    const blink::WebUSBDevice::ControlTransferParameters& parameters,
    uint8_t* data,
    size_t data_size,
    unsigned int timeout,
    blink::WebUSBDeviceControlTransferCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device::usb::ControlTransferParamsPtr params =
      device::usb::ControlTransferParams::From(parameters);
  switch (parameters.direction) {
    case WebUSBDevice::TransferDirection::In:
      device_->ControlTransferIn(
          std::move(params), data_size, timeout,
          base::Bind(&OnTransferIn, base::Passed(&scoped_callbacks)));
      break;
    case WebUSBDevice::TransferDirection::Out: {
      std::vector<uint8_t> bytes;
      if (data)
        bytes.assign(data, data + data_size);
      mojo::Array<uint8_t> mojo_bytes;
      mojo_bytes.Swap(&bytes);
      device_->ControlTransferOut(
          std::move(params), std::move(mojo_bytes), timeout,
          base::Bind(&OnTransferOut, base::Passed(&scoped_callbacks),
                     data_size));
      break;
    }
    default:
      NOTREACHED();
  }
}

void WebUSBDeviceImpl::transfer(
    blink::WebUSBDevice::TransferDirection direction,
    uint8_t endpoint_number,
    uint8_t* data,
    size_t data_size,
    unsigned int timeout,
    blink::WebUSBDeviceBulkTransferCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  switch (direction) {
    case WebUSBDevice::TransferDirection::In:
      device_->GenericTransferIn(
          endpoint_number, data_size, timeout,
          base::Bind(&OnTransferIn, base::Passed(&scoped_callbacks)));
      break;
    case WebUSBDevice::TransferDirection::Out: {
      std::vector<uint8_t> bytes;
      if (data)
        bytes.assign(data, data + data_size);
      mojo::Array<uint8_t> mojo_bytes;
      mojo_bytes.Swap(&bytes);
      device_->GenericTransferOut(
          endpoint_number, std::move(mojo_bytes), timeout,
          base::Bind(&OnTransferOut, base::Passed(&scoped_callbacks),
                     data_size));
      break;
    }
    default:
      NOTREACHED();
  }
}

void WebUSBDeviceImpl::reset(blink::WebUSBDeviceResetCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_->Reset(base::Bind(&HandlePassFailDeviceOperation,
                            base::Passed(&scoped_callbacks),
                            kDeviceResetFailed));
}

}  // namespace content
