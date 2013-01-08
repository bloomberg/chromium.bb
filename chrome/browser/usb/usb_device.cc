// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_device.h"

#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/usb/usb_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

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

UsbDevice::Transfer::Transfer() : length(0) {}

UsbDevice::Transfer::~Transfer() {}

UsbDevice::UsbDevice(UsbService* service, PlatformUsbDeviceHandle handle)
    : service_(service), handle_(handle) {
  DCHECK(handle) << "Cannot create device with NULL handle.";
}

UsbDevice::UsbDevice() : service_(NULL), handle_(NULL) {}

UsbDevice::~UsbDevice() {}

void UsbDevice::Close(const base::Callback<void()>& callback) {
  CheckDevice();
  service_->CloseDevice(this);
  handle_ = NULL;
  callback.Run();
}

void UsbDevice::TransferComplete(PlatformUsbTransferHandle handle) {
  base::AutoLock lock(lock_);

  // TODO(gdk): Handle device disconnect.
  DCHECK(ContainsKey(transfers_, handle)) << "Missing transfer completed";
  Transfer* const transfer = &transfers_[handle];

  DCHECK(handle->actual_length >= 0) << "Negative actual length received";
  size_t actual_length =
      static_cast<size_t>(std::max(handle->actual_length, 0));

  DCHECK(transfer->length >= actual_length) <<
      "data too big for our buffer (libusb failure?)";

  scoped_refptr<net::IOBuffer> buffer = transfer->buffer;
  switch (transfer->transfer_type) {
    case USB_TRANSFER_CONTROL:
      // If the transfer is a control transfer we do not expose the control
      // setup header to the caller. This logic strips off the header if
      // present before invoking the callback provided with the transfer.
      if (actual_length > 0) {
        CHECK(transfer->length >= LIBUSB_CONTROL_SETUP_SIZE) <<
            "buffer was not correctly set: too small for the control header";

        if (transfer->length >= actual_length &&
            actual_length >= LIBUSB_CONTROL_SETUP_SIZE) {
          // If the payload is zero bytes long, pad out the allocated buffer
          // size to one byte so that an IOBuffer of that size can be allocated.
          scoped_refptr<net::IOBuffer> resized_buffer = new net::IOBuffer(
              std::max(actual_length, static_cast<size_t>(1)));
          memcpy(resized_buffer->data(),
                 buffer->data() + LIBUSB_CONTROL_SETUP_SIZE,
                 actual_length);
          buffer = resized_buffer;
        }
      }
      break;

    case USB_TRANSFER_ISOCHRONOUS:
      // Isochronous replies might carry data in the different isoc packets even
      // if the transfer actual_data value is zero. Furthermore, not all of the
      // received packets might contain data, so we need to calculate how many
      // data bytes we are effectively providing and pack the results.
      if (actual_length == 0) {
        size_t packet_buffer_start = 0;
        for (int i = 0; i < handle->num_iso_packets; ++i) {
          PlatformUsbIsoPacketDescriptor packet = &handle->iso_packet_desc[i];
          if (packet->actual_length > 0) {
            // We don't need to copy as long as all packets until now provide
            // all the data the packet can hold.
            if (actual_length < packet_buffer_start) {
              CHECK(packet_buffer_start + packet->actual_length <=
                    transfer->length);
              memmove(buffer->data() + actual_length,
                      buffer->data() + packet_buffer_start,
                      packet->actual_length);
            }
            actual_length += packet->actual_length;
          }

          packet_buffer_start += packet->length;
        }
      }
      break;

    case USB_TRANSFER_BULK:
    case USB_TRANSFER_INTERRUPT:
      break;

    default:
      NOTREACHED() << "Invalid usb transfer type";
  }

  transfer->callback.Run(ConvertTransferStatus(handle->status), buffer,
                         actual_length);

  transfers_.erase(handle);
  libusb_free_transfer(handle);
}

void UsbDevice::ClaimInterface(const int interface_number,
                               const UsbInterfaceCallback& callback) {
  CheckDevice();

  const int claim_result = libusb_claim_interface(handle_, interface_number);
  callback.Run(claim_result == 0);
}

void UsbDevice::ReleaseInterface(const int interface_number,
                                 const UsbInterfaceCallback& callback) {
  CheckDevice();

  const int release_result = libusb_release_interface(handle_,
                                                      interface_number);
  callback.Run(release_result == 0);
}

void UsbDevice::SetInterfaceAlternateSetting(
    const int interface_number,
    const int alternate_setting,
    const UsbInterfaceCallback& callback) {
  CheckDevice();

  const int setting_result = libusb_set_interface_alt_setting(handle_,
      interface_number, alternate_setting);

  callback.Run(setting_result == 0);
}

void UsbDevice::ControlTransfer(const TransferDirection direction,
    const TransferRequestType request_type, const TransferRecipient recipient,
    const uint8 request, const uint16 value, const uint16 index,
    net::IOBuffer* buffer, const size_t length, const unsigned int timeout,
    const UsbTransferCallback& callback) {
  CheckDevice();

  const size_t resized_length = LIBUSB_CONTROL_SETUP_SIZE + length;
  scoped_refptr<net::IOBuffer> resized_buffer(new net::IOBufferWithSize(
      resized_length));
  memcpy(resized_buffer->data() + LIBUSB_CONTROL_SETUP_SIZE, buffer->data(),
         length);

  struct libusb_transfer* const transfer = libusb_alloc_transfer(0);
  const uint8 converted_type = CreateRequestType(direction, request_type,
                                                 recipient);
  libusb_fill_control_setup(reinterpret_cast<uint8*>(resized_buffer->data()),
                            converted_type, request, value, index, length);
  libusb_fill_control_transfer(transfer, handle_, reinterpret_cast<uint8*>(
      resized_buffer->data()), reinterpret_cast<libusb_transfer_cb_fn>(
          &HandleTransferCompletion), this, timeout);
  SubmitTransfer(transfer, USB_TRANSFER_CONTROL, resized_buffer, resized_length,
                 callback);
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
  SubmitTransfer(transfer, USB_TRANSFER_BULK, buffer, length, callback);
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
  SubmitTransfer(transfer, USB_TRANSFER_INTERRUPT, buffer, length, callback);
}

void UsbDevice::IsochronousTransfer(const TransferDirection direction,
    const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
    const unsigned int packets, const unsigned int packet_length,
    const unsigned int timeout, const UsbTransferCallback& callback) {
  CheckDevice();

  const uint64 total_length = packets * packet_length;
  if (total_length > length) {
    callback.Run(USB_TRANSFER_LENGTH_SHORT, NULL, 0);
    return;
  }

  struct libusb_transfer* const transfer = libusb_alloc_transfer(packets);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_iso_transfer(transfer, handle_, new_endpoint,
      reinterpret_cast<uint8*>(buffer->data()), length, packets,
      reinterpret_cast<libusb_transfer_cb_fn>(&HandleTransferCompletion), this,
      timeout);
  libusb_set_iso_packet_lengths(transfer, packet_length);

  SubmitTransfer(transfer, USB_TRANSFER_ISOCHRONOUS, buffer, length, callback);
}

void UsbDevice::CheckDevice() {
  DCHECK(handle_) << "Device is already closed.";
}

void UsbDevice::SubmitTransfer(PlatformUsbTransferHandle handle,
                               UsbTransferType transfer_type,
                               net::IOBuffer* buffer,
                               const size_t length,
                               const UsbTransferCallback& callback) {
  Transfer transfer;
  transfer.transfer_type = transfer_type;
  transfer.buffer = buffer;
  transfer.length = length;
  transfer.callback = callback;

  {
    base::AutoLock lock(lock_);
    transfers_[handle] = transfer;
    libusb_submit_transfer(handle);
  }
}
