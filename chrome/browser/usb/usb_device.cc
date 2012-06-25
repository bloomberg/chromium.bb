// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_device.h"

#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/usb/usb_service.h"
#include "third_party/libusb/libusb.h"

namespace {

static uint8 ConvertTransferDirection(
    const UsbDevice::TransferDirection direction) {
  switch (direction) {
    case UsbDevice::INBOUND:
      return LIBUSB_ENDPOINT_IN;
    case UsbDevice::OUTBOUND:
      return LIBUSB_ENDPOINT_OUT;
  }
  NOTREACHED();
  return LIBUSB_ENDPOINT_OUT;
}

static uint8 CreateRequestType(const UsbDevice::TransferDirection direction,
    const UsbDevice::TransferRequestType request_type,
    const UsbDevice::TransferRecipient recipient) {
  uint8 result = ConvertTransferDirection(direction);

  switch (request_type) {
    case UsbDevice::STANDARD:
      result |= LIBUSB_REQUEST_TYPE_STANDARD;
      break;
    case UsbDevice::CLASS:
      result |= LIBUSB_REQUEST_TYPE_CLASS;
      break;
    case UsbDevice::VENDOR:
      result |= LIBUSB_REQUEST_TYPE_VENDOR;
      break;
    case UsbDevice::RESERVED:
      result |= LIBUSB_REQUEST_TYPE_RESERVED;
      break;
  }

  switch (recipient) {
    case UsbDevice::DEVICE:
      result |= LIBUSB_RECIPIENT_DEVICE;
      break;
    case UsbDevice::INTERFACE:
      result |= LIBUSB_RECIPIENT_INTERFACE;
      break;
    case UsbDevice::ENDPOINT:
      result |= LIBUSB_RECIPIENT_ENDPOINT;
      break;
    case UsbDevice::OTHER:
      result |= LIBUSB_RECIPIENT_OTHER;
      break;
  }

  return result;
}

static UsbTransferStatus ConvertTransferStatus(
    const libusb_transfer_status status) {
  switch (status) {
    case LIBUSB_TRANSFER_COMPLETED:
      return USB_TRANSFER_COMPLETED;
    case LIBUSB_TRANSFER_ERROR:
      return USB_TRANSFER_ERROR;
    case LIBUSB_TRANSFER_TIMED_OUT:
      return USB_TRANSFER_TIMEOUT;
    case LIBUSB_TRANSFER_STALL:
      return USB_TRANSFER_STALLED;
    case LIBUSB_TRANSFER_NO_DEVICE:
      return USB_TRANSFER_DISCONNECT;
    case LIBUSB_TRANSFER_OVERFLOW:
      return USB_TRANSFER_OVERFLOW;
    case LIBUSB_TRANSFER_CANCELLED:
      return USB_TRANSFER_CANCELLED;
  }
  NOTREACHED();
  return USB_TRANSFER_ERROR;
}

static void HandleTransferCompletion(struct libusb_transfer* transfer) {
  UsbDevice* const device = reinterpret_cast<UsbDevice*>(transfer->user_data);
  device->TransferComplete(transfer);
}

}  // namespace

UsbDevice::Transfer::Transfer() {}

UsbDevice::Transfer::~Transfer() {}

UsbDevice::UsbDevice(UsbService* service, PlatformUsbDeviceHandle handle)
    : service_(service), handle_(handle) {
  DCHECK(handle) << "Cannot create device with NULL handle.";
}

UsbDevice::~UsbDevice() {}

void UsbDevice::Close() {
  CheckDevice();
  service_->CloseDevice(this);
  handle_ = NULL;
}

void UsbDevice::TransferComplete(PlatformUsbTransferHandle handle) {
  base::AutoLock lock(lock_);

  // TODO(gdk): Handle device disconnect.
  DCHECK(ContainsKey(transfers_, handle)) << "Missing transfer completed";
  Transfer* const transfer = &transfers_[handle];
  if (transfer->buffer.get()) {
    transfer->callback.Run(ConvertTransferStatus(handle->status));
  }

  transfers_.erase(handle);
  libusb_free_transfer(handle);
}

void UsbDevice::ControlTransfer(const TransferDirection direction,
    const TransferRequestType request_type, const TransferRecipient recipient,
    const uint8 request, const uint16 value, const uint16 index,
    net::IOBuffer* buffer, const size_t length, const unsigned int timeout,
    const UsbTransferCallback& callback) {
  CheckDevice();

  struct libusb_transfer* const transfer = libusb_alloc_transfer(0);
  const uint8 converted_type = CreateRequestType(direction, request_type,
                                                 recipient);
  libusb_fill_control_setup(reinterpret_cast<uint8*>(buffer->data()),
                            converted_type, request, value, index, length);
  libusb_fill_control_transfer(transfer, handle_, reinterpret_cast<uint8*>(
      buffer->data()), reinterpret_cast<libusb_transfer_cb_fn>(
          &HandleTransferCompletion), this, timeout);
  SubmitTransfer(transfer, buffer, callback);
}

void UsbDevice::BulkTransfer(const TransferDirection direction,
    const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
    const unsigned int timeout, const UsbTransferCallback& callback) {
  CheckDevice();

  struct libusb_transfer* const transfer = libusb_alloc_transfer(0);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_bulk_transfer(transfer, handle_, new_endpoint,
      reinterpret_cast<uint8*>(buffer->data()), length,
      reinterpret_cast<libusb_transfer_cb_fn>(&HandleTransferCompletion), this,
      timeout);
  SubmitTransfer(transfer, buffer, callback);
}

void UsbDevice::InterruptTransfer(const TransferDirection direction,
    const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
    const unsigned int timeout, const UsbTransferCallback& callback) {
  CheckDevice();

  struct libusb_transfer* const transfer = libusb_alloc_transfer(0);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_interrupt_transfer(transfer, handle_, new_endpoint,
      reinterpret_cast<uint8*>(buffer->data()), length,
      reinterpret_cast<libusb_transfer_cb_fn>(&HandleTransferCompletion), this,
      timeout);
  SubmitTransfer(transfer, buffer, callback);
}

void UsbDevice::IsochronousTransfer(const TransferDirection direction,
    const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
    const unsigned int packets, const unsigned int packet_length,
    const unsigned int timeout, const UsbTransferCallback& callback) {
  CheckDevice();

  struct libusb_transfer* const transfer = libusb_alloc_transfer(packets);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_iso_transfer(transfer, handle_, new_endpoint,
      reinterpret_cast<uint8*>(buffer->data()), length, packets,
      reinterpret_cast<libusb_transfer_cb_fn>(&HandleTransferCompletion), this,
      timeout);
  libusb_set_iso_packet_lengths(transfer, packet_length);

  SubmitTransfer(transfer, buffer, callback);
}

void UsbDevice::CheckDevice() {
  DCHECK(handle_) << "Device is already closed.";
}

void UsbDevice::SubmitTransfer(PlatformUsbTransferHandle handle,
                               net::IOBuffer* buffer,
                               const UsbTransferCallback& callback) {
  libusb_submit_transfer(handle);

  Transfer transfer;
  transfer.buffer = buffer;
  transfer.callback = callback;

  {
    base::AutoLock lock(lock_);
    transfers_[handle] = transfer;
  }
}
