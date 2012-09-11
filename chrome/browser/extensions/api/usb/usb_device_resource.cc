// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/usb/usb_device_resource.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/usb/usb_device.h"
#include "chrome/common/extensions/api/experimental_usb.h"

using extensions::api::experimental_usb::ControlTransferInfo;
using extensions::api::experimental_usb::GenericTransferInfo;
using extensions::api::experimental_usb::IsochronousTransferInfo;
using std::string;
using std::vector;

namespace {

static const char* kDirectionIn = "in";
static const char* kDirectionOut = "out";

static const char* kRequestTypeStandard = "standard";
static const char* kRequestTypeClass = "class";
static const char* kRequestTypeVendor = "vendor";
static const char* kRequestTypeReserved = "reserved";

static const char* kRecipientDevice = "device";
static const char* kRecipientInterface = "interface";
static const char* kRecipientEndpoint = "endpoint";
static const char* kRecipientOther = "other";

static const char* kErrorGeneric = "Transfer failed";
static const char* kErrorTimeout = "Transfer timed out";
static const char* kErrorCancelled = "Transfer was cancelled";
static const char* kErrorStalled = "Transfer stalled";
static const char* kErrorDisconnect = "Device disconnected";
static const char* kErrorOverflow = "Inbound transfer overflow";

static bool ConvertDirection(const string& input,
                             UsbDevice::TransferDirection* output) {
  if (input == kDirectionIn) {
    *output = UsbDevice::INBOUND;
    return true;
  } else if (input == kDirectionOut) {
    *output = UsbDevice::OUTBOUND;
    return true;
  }
  return false;
}

static bool ConvertRequestType(const string& input,
                               UsbDevice::TransferRequestType* output) {
  if (input == kRequestTypeStandard) {
    *output = UsbDevice::STANDARD;
    return true;
  } else if (input == kRequestTypeClass) {
    *output = UsbDevice::CLASS;
    return true;
  } else if (input == kRequestTypeVendor) {
    *output = UsbDevice::VENDOR;
    return true;
  } else if (input == kRequestTypeReserved) {
    *output = UsbDevice::RESERVED;
    return true;
  }
  return false;
}

static bool ConvertRecipient(const string& input,
                             UsbDevice::TransferRecipient* output) {
  if (input == kRecipientDevice) {
    *output = UsbDevice::DEVICE;
    return true;
  } else if (input == kRecipientInterface) {
    *output = UsbDevice::INTERFACE;
    return true;
  } else if (input == kRecipientEndpoint) {
    *output = UsbDevice::ENDPOINT;
    return true;
  } else if (input == kRecipientOther) {
    *output = UsbDevice::OTHER;
    return true;
  }
  return false;
}

template<class T>
static bool GetTransferSize(const T& input, size_t* output) {
  if (input.direction == kDirectionIn) {
    const int* length = input.length.get();
    if (length) {
      *output = *length;
      return true;
    }
  } else if (input.direction == kDirectionOut) {
    if (input.data.get()) {
      *output = input.data->size();
      return true;
    }
  }
  return false;
}

template<class T>
static scoped_refptr<net::IOBuffer> CreateBufferForTransfer(const T& input) {
  size_t size = 0;
  if (!GetTransferSize(input, &size))
    return NULL;

  // Allocate a |size|-bytes buffer, or a one-byte buffer if |size| is 0. This
  // is due to an impedance mismatch between IOBuffer and URBs. An IOBuffer
  // cannot represent a zero-length buffer, while an URB can.
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(std::max(
      static_cast<size_t>(1), size));
  if (!input.data.get())
    return buffer;

  memcpy(buffer->data(), input.data->data(), size);
  return buffer;
}

static const char* ConvertTransferStatusToErrorString(
    const UsbTransferStatus status) {
  switch (status) {
    case USB_TRANSFER_COMPLETED:
      return "";
    case USB_TRANSFER_ERROR:
      return kErrorGeneric;
    case USB_TRANSFER_TIMEOUT:
      return kErrorTimeout;
    case USB_TRANSFER_CANCELLED:
      return kErrorCancelled;
    case USB_TRANSFER_STALLED:
      return kErrorStalled;
    case USB_TRANSFER_DISCONNECT:
      return kErrorDisconnect;
    case USB_TRANSFER_OVERFLOW:
      return kErrorOverflow;
  }

  NOTREACHED();
  return "";
}

}  // namespace

namespace extensions {

UsbDeviceResource::UsbDeviceResource(const std::string& owner_extension_id,
                                     ApiResourceEventNotifier* notifier,
                                     UsbDevice* device)
    : ApiResource(owner_extension_id, notifier), device_(device) {}

UsbDeviceResource::~UsbDeviceResource() {
  Close();
}

void UsbDeviceResource::Close() {
  device_->Close();
}

void UsbDeviceResource::ControlTransfer(const ControlTransferInfo& transfer) {
  UsbDevice::TransferDirection direction;
  UsbDevice::TransferRequestType request_type;
  UsbDevice::TransferRecipient recipient;
  size_t size;
  scoped_refptr<net::IOBuffer> buffer = CreateBufferForTransfer(transfer);

  if (!ConvertDirection(transfer.direction, &direction) ||
      !ConvertRequestType(transfer.request_type, &request_type) ||
      !ConvertRecipient(transfer.recipient, &recipient) ||
      !GetTransferSize(transfer, &size) || !buffer) {
    LOG(INFO) << "Malformed transfer parameters.";
    return;
  }

  device_->ControlTransfer(direction, request_type, recipient, transfer.request,
                           transfer.value, transfer.index, buffer, size, 0,
                           base::Bind(&UsbDeviceResource::TransferComplete,
                                      base::Unretained(this), buffer, size));
}

void UsbDeviceResource::InterruptTransfer(const GenericTransferInfo& transfer) {
  size_t size;
  UsbDevice::TransferDirection direction;
  scoped_refptr<net::IOBuffer> buffer = CreateBufferForTransfer(transfer);

  if (!ConvertDirection(transfer.direction, &direction) ||
      !GetTransferSize(transfer, &size) || !buffer) {
    LOG(INFO) << "Malformed transfer parameters.";
    return;
  }

  device_->InterruptTransfer(direction, transfer.endpoint, buffer, size, 0,
                             base::Bind(&UsbDeviceResource::TransferComplete,
                                        base::Unretained(this), buffer, size));
}

void UsbDeviceResource::BulkTransfer(const GenericTransferInfo& transfer) {
  size_t size;
  UsbDevice::TransferDirection direction;
  scoped_refptr<net::IOBuffer> buffer = CreateBufferForTransfer(transfer);

  if (!ConvertDirection(transfer.direction, &direction) ||
      !GetTransferSize(transfer, &size) || !buffer) {
    LOG(INFO) << "Malformed transfer parameters.";
    return;
  }

  device_->BulkTransfer(direction, transfer.endpoint, buffer, size, 0,
                        base::Bind(&UsbDeviceResource::TransferComplete,
                                   base::Unretained(this), buffer, size));
}

void UsbDeviceResource::IsochronousTransfer(
    const IsochronousTransferInfo& transfer) {
  const GenericTransferInfo& generic_transfer = transfer.transfer_info;

  size_t size;
  UsbDevice::TransferDirection direction;
  scoped_refptr<net::IOBuffer> buffer = CreateBufferForTransfer(
      generic_transfer);

  if (!ConvertDirection(generic_transfer.direction, &direction) ||
      !GetTransferSize(generic_transfer, &size) || !buffer) {
    LOG(INFO) << "Malformed transfer parameters.";
    return;
  }

  device_->IsochronousTransfer(direction, generic_transfer.endpoint, buffer,
      size, transfer.packets, transfer.packet_length, 0, base::Bind(
          &UsbDeviceResource::TransferComplete, base::Unretained(this), buffer,
          size));
}

void UsbDeviceResource::TransferComplete(net::IOBuffer* buffer,
                                         const size_t length,
                                         UsbTransferStatus status) {
  if (buffer) {
    base::BinaryValue* const response_buffer =
        base::BinaryValue::CreateWithCopiedBuffer(buffer->data(), length);
    event_notifier()->OnTransferComplete(status,
        ConvertTransferStatusToErrorString(status), response_buffer);
  }
}

}  // namespace extensions
