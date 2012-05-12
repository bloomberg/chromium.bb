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
using std::string;
using std::vector;

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

namespace {

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
static bool GetTransferSize(const T& input, unsigned int* output) {
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
  unsigned int size = 0;
  if (!GetTransferSize(input, &size)) {
    return NULL;
  }

  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(size);
  if (!input.data.get()) {
    return buffer;
  }

  const vector<int>& input_buffer = *input.data.get();
  for (unsigned int i = 0; i < size; ++i) {
    buffer->data()[i] = input_buffer[i];
  }

  return buffer;
}

}  // namespace

namespace extensions {

UsbDeviceResource::UsbDeviceResource(APIResourceEventNotifier* notifier,
                                     UsbDevice* device)
    : APIResource(APIResource::UsbDeviceResource, notifier), device_(device) {}

UsbDeviceResource::~UsbDeviceResource() {}

void UsbDeviceResource::ControlTransfer(const ControlTransferInfo& transfer) {
  UsbDevice::TransferDirection direction;
  UsbDevice::TransferRequestType request_type;
  UsbDevice::TransferRecipient recipient;
  unsigned int size;
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
  unsigned int size;
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
  unsigned int size;
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

void UsbDeviceResource::TransferComplete(net::IOBuffer* buffer,
                                         const size_t length,
                                         int success) {
  if (buffer) {
    base::ListValue *const response_buffer = new base::ListValue();
    for (unsigned int i = 0; i < length; ++i) {
      const uint8_t value = buffer->data()[i] & 0xFF;
      response_buffer->Append(base::Value::CreateIntegerValue(value));
    }
    event_notifier()->OnTransferComplete(success, response_buffer);
  }
}

}  // namespace extensions
