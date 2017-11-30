// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/fake_usb_device_handle.h"

#include "base/callback.h"
#include "base/logging.h"
#include "device/usb/usb_device.h"
#include "net/base/io_buffer.h"

namespace device {

FakeUsbDeviceHandle::FakeUsbDeviceHandle(const uint8_t* data, size_t size)
    : data_(data), size_(size), position_(0) {}

scoped_refptr<UsbDevice> FakeUsbDeviceHandle::GetDevice() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeUsbDeviceHandle::Close() {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::SetConfiguration(int configuration_value,
                                           ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ClaimInterface(int interface_number,
                                         ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ReleaseInterface(int interface_number,
                                           ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::SetInterfaceAlternateSetting(
    int interface_number,
    int alternate_setting,
    ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ResetDevice(ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ClearHalt(uint8_t endpoint, ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ControlTransfer(
    UsbTransferDirection direction,
    UsbControlTransferType request_type,
    UsbControlTransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length,
    unsigned int timeout,
    UsbDeviceHandle::TransferCallback callback) {
  if (position_ == size_) {
    std::move(callback).Run(UsbTransferStatus::DISCONNECT, buffer, 0);
    return;
  }

  if (data_[position_++] % 2) {
    size_t bytes_transferred = 0;
    if (position_ + 2 <= size_) {
      bytes_transferred = data_[position_] | data_[position_ + 1] << 8;
      position_ += 2;
      bytes_transferred = std::min(bytes_transferred, length);
      bytes_transferred = std::min(bytes_transferred, size_ - position_);
    }

    if (direction == UsbTransferDirection::INBOUND) {
      memcpy(buffer->data(), &data_[position_], bytes_transferred);
      position_ += bytes_transferred;
    }

    std::move(callback).Run(UsbTransferStatus::COMPLETED, buffer,
                            bytes_transferred);
  } else {
    std::move(callback).Run(UsbTransferStatus::TRANSFER_ERROR, buffer, 0);
  }
}

void FakeUsbDeviceHandle::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::IsochronousTransferOut(
    uint8_t endpoint_number,
    scoped_refptr<net::IOBuffer> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::GenericTransfer(UsbTransferDirection direction,
                                          uint8_t endpoint_number,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          size_t length,
                                          unsigned int timeout,
                                          TransferCallback callback) {
  NOTIMPLEMENTED();
}

const UsbInterfaceDescriptor* FakeUsbDeviceHandle::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  NOTIMPLEMENTED();
  return nullptr;
}

FakeUsbDeviceHandle::~FakeUsbDeviceHandle() = default;

}  // namespace device
