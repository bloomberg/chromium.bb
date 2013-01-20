// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/usb/usb_api.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/usb/usb_device_resource.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_service.h"
#include "chrome/browser/usb/usb_service_factory.h"
#include "chrome/common/extensions/api/usb.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"

namespace BulkTransfer = extensions::api::usb::BulkTransfer;
namespace ClaimInterface = extensions::api::usb::ClaimInterface;
namespace CloseDevice = extensions::api::usb::CloseDevice;
namespace ControlTransfer = extensions::api::usb::ControlTransfer;
namespace FindDevices = extensions::api::usb::FindDevices;
namespace InterruptTransfer = extensions::api::usb::InterruptTransfer;
namespace IsochronousTransfer = extensions::api::usb::IsochronousTransfer;
namespace ReleaseInterface = extensions::api::usb::ReleaseInterface;
namespace SetInterfaceAlternateSetting =
    extensions::api::usb::SetInterfaceAlternateSetting;
namespace usb = extensions::api::usb;

using std::string;
using std::vector;
using usb::ControlTransferInfo;
using usb::Device;
using usb::Direction;
using usb::GenericTransferInfo;
using usb::IsochronousTransferInfo;
using usb::Recipient;
using usb::RequestType;

namespace {

static const char* kDataKey = "data";
static const char* kResultCodeKey = "resultCode";

static const char* kErrorCancelled = "Transfer was cancelled.";
static const char* kErrorDisconnect = "Device disconnected.";
static const char* kErrorGeneric = "Transfer failed.";
static const char* kErrorOverflow = "Inbound transfer overflow.";
static const char* kErrorStalled = "Transfer stalled.";
static const char* kErrorTimeout = "Transfer timed out.";
static const char* kErrorTransferLength = "Transfer length is insufficient.";

static const char* kErrorCannotClaimInterface = "Error claiming interface.";
static const char* kErrorCannotReleaseInterface = "Error releasing interface.";
static const char* kErrorCannotSetInterfaceAlternateSetting =
    "Error setting alternate interface setting.";
static const char* kErrorConvertDirection = "Invalid transfer direction.";
static const char* kErrorConvertRecipient = "Invalid transfer recipient.";
static const char* kErrorConvertRequestType = "Invalid request type.";
static const char* kErrorMalformedParameters = "Error parsing parameters.";
static const char* kErrorNoDevice = "No such device.";
static const char* kErrorPermissionDenied =
    "Permission to access device was denied";
static const char* kErrorInvalidTransferLength = "Transfer length must be a "
    "positive number less than 104,857,600.";

static const size_t kMaxTransferLength = 100 * 1024 * 1024;
static UsbDevice* device_for_test_ = NULL;

static bool ConvertDirection(const Direction& input,
                             UsbDevice::TransferDirection* output) {
  switch (input) {
    case usb::DIRECTION_IN:
      *output = UsbDevice::INBOUND;
      return true;
    case usb::DIRECTION_OUT:
      *output = UsbDevice::OUTBOUND;
      return true;
    default:
      return false;
  }
  NOTREACHED();
  return false;
}

static bool ConvertRequestType(const RequestType& input,
                               UsbDevice::TransferRequestType* output) {
  switch (input) {
    case usb::REQUEST_TYPE_STANDARD:
      *output = UsbDevice::STANDARD;
      return true;
    case usb::REQUEST_TYPE_CLASS:
      *output = UsbDevice::CLASS;
      return true;
    case usb::REQUEST_TYPE_VENDOR:
      *output = UsbDevice::VENDOR;
      return true;
    case usb::REQUEST_TYPE_RESERVED:
      *output = UsbDevice::RESERVED;
      return true;
    default:
      return false;
  }
  NOTREACHED();
  return false;
}

static bool ConvertRecipient(const Recipient& input,
                             UsbDevice::TransferRecipient* output) {
  switch (input) {
    case usb::RECIPIENT_DEVICE:
      *output = UsbDevice::DEVICE;
      return true;
    case usb::RECIPIENT_INTERFACE:
      *output = UsbDevice::INTERFACE;
      return true;
    case usb::RECIPIENT_ENDPOINT:
      *output = UsbDevice::ENDPOINT;
      return true;
    case usb::RECIPIENT_OTHER:
      *output = UsbDevice::OTHER;
      return true;
    default:
      return false;
  }
  NOTREACHED();
  return false;
}

template<class T>
static bool GetTransferSize(const T& input, size_t* output) {
  if (input.direction == usb::DIRECTION_IN) {
    const int* length = input.length.get();
    if (length && *length >= 0 &&
        static_cast<size_t>(*length) < kMaxTransferLength) {
      *output = *length;
      return true;
    }
  } else if (input.direction == usb::DIRECTION_OUT) {
    if (input.data.get()) {
      *output = input.data->size();
      return true;
    }
  }
  return false;
}

template<class T>
static scoped_refptr<net::IOBuffer> CreateBufferForTransfer(
    const T& input, UsbDevice::TransferDirection direction, size_t size) {

  if (size > kMaxTransferLength)
    return NULL;

  // Allocate a |size|-bytes buffer, or a one-byte buffer if |size| is 0. This
  // is due to an impedance mismatch between IOBuffer and URBs. An IOBuffer
  // cannot represent a zero-length buffer, while an URB can.
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(std::max(
      static_cast<size_t>(1), size));

  if (direction == UsbDevice::INBOUND) {
    return buffer;
  } else if (direction == UsbDevice::OUTBOUND) {
    if (input.data.get() && size <= input.data->size()) {
      memcpy(buffer->data(), input.data->data(), size);
      return buffer;
    }
  }
  NOTREACHED();
  return NULL;
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
    case USB_TRANSFER_LENGTH_SHORT:
      return kErrorTransferLength;
  }

  NOTREACHED();
  return "";
}

static base::DictionaryValue* CreateTransferInfo(
    UsbTransferStatus status,
    scoped_refptr<net::IOBuffer> data,
    size_t length) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kResultCodeKey, status);
  result->Set(kDataKey, base::BinaryValue::CreateWithCopiedBuffer(data->data(),
                                                                  length));
  return result;
}

static base::Value* PopulateDevice(int handle, int vendor_id, int product_id) {
  Device device;
  device.handle = handle;
  device.vendor_id = vendor_id;
  device.product_id = product_id;
  return device.ToValue().release();
}

}  // namespace

namespace extensions {

UsbAsyncApiFunction::UsbAsyncApiFunction()
    : manager_(NULL) {
}

UsbAsyncApiFunction::~UsbAsyncApiFunction() {
}

bool UsbAsyncApiFunction::PrePrepare() {
  manager_ = ExtensionSystem::Get(profile())->usb_device_resource_manager();
  return manager_ != NULL;
}

bool UsbAsyncApiFunction::Respond() {
  return error_.empty();
}

UsbDeviceResource* UsbAsyncApiFunction::GetUsbDeviceResource(
    int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void UsbAsyncApiFunction::RemoveUsbDeviceResource(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

void UsbAsyncApiFunction::CompleteWithError(const std::string& error) {
  SetError(error);
  AsyncWorkCompleted();
}

UsbAsyncApiTransferFunction::UsbAsyncApiTransferFunction() {}

UsbAsyncApiTransferFunction::~UsbAsyncApiTransferFunction() {}

void UsbAsyncApiTransferFunction::OnCompleted(UsbTransferStatus status,
                                              scoped_refptr<net::IOBuffer> data,
                                              size_t length) {
  if (status != USB_TRANSFER_COMPLETED)
    SetError(ConvertTransferStatusToErrorString(status));

  SetResult(CreateTransferInfo(status, data, length));
  AsyncWorkCompleted();
}

bool UsbAsyncApiTransferFunction::ConvertDirectionSafely(
    const Direction& input, UsbDevice::TransferDirection* output) {
  const bool converted = ConvertDirection(input, output);
  if (!converted)
    SetError(kErrorConvertDirection);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRequestTypeSafely(
    const RequestType& input, UsbDevice::TransferRequestType* output) {
  const bool converted = ConvertRequestType(input, output);
  if (!converted)
    SetError(kErrorConvertRequestType);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRecipientSafely(
    const Recipient& input, UsbDevice::TransferRecipient* output) {
  const bool converted = ConvertRecipient(input, output);
  if (!converted)
    SetError(kErrorConvertRecipient);
  return converted;
}

UsbFindDevicesFunction::UsbFindDevicesFunction() {}

UsbFindDevicesFunction::~UsbFindDevicesFunction() {}

void UsbFindDevicesFunction::SetDeviceForTest(UsbDevice* device) {
  device_for_test_ = device;
}

bool UsbFindDevicesFunction::Prepare() {
  parameters_ = FindDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbFindDevicesFunction::AsyncWorkStart() {
  scoped_ptr<base::ListValue> result(new base::ListValue());

  if (device_for_test_) {
    UsbDeviceResource* const resource = new UsbDeviceResource(
        extension_->id(),
        device_for_test_);

    Device device;
    result->Append(PopulateDevice(manager_->Add(resource), 0, 0));
    SetResult(result.release());
    AsyncWorkCompleted();
    return;
  }

  const uint16_t vendor_id = parameters_->options.vendor_id;
  const uint16_t product_id = parameters_->options.product_id;
  UsbDevicePermission::CheckParam param(vendor_id, product_id);
  if (!GetExtension()->CheckAPIPermissionWithParam(
        APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  UsbService* const service = UsbServiceFactory::GetInstance()->GetForProfile(
      profile());
  if (!service) {
    LOG(WARNING) << "Could not get UsbService for active profile.";
    CompleteWithError(kErrorNoDevice);
    return;
  }

  vector<scoped_refptr<UsbDevice> > devices;
  service->FindDevices(vendor_id, product_id, &devices);
  for (size_t i = 0; i < devices.size(); ++i) {
    UsbDevice* const device = devices[i];
    UsbDeviceResource* const resource = new UsbDeviceResource(
        extension_->id(), device);

    Device js_device;
    result->Append(PopulateDevice(manager_->Add(resource),
                                  vendor_id,
                                  product_id));
  }

  SetResult(result.release());
  AsyncWorkCompleted();
}

UsbCloseDeviceFunction::UsbCloseDeviceFunction() {}

UsbCloseDeviceFunction::~UsbCloseDeviceFunction() {}

bool UsbCloseDeviceFunction::Prepare() {
  parameters_ = CloseDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbCloseDeviceFunction::AsyncWorkStart() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!device) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  device->device()->Close(base::Bind(&UsbCloseDeviceFunction::OnCompleted,
                                     this));
  RemoveUsbDeviceResource(parameters_->device.handle);
}

void UsbCloseDeviceFunction::OnCompleted() {
  AsyncWorkCompleted();
}

UsbClaimInterfaceFunction::UsbClaimInterfaceFunction() {}

UsbClaimInterfaceFunction::~UsbClaimInterfaceFunction() {}

bool UsbClaimInterfaceFunction::Prepare() {
  parameters_ = ClaimInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbClaimInterfaceFunction::AsyncWorkStart() {
  UsbDeviceResource* device = GetUsbDeviceResource(parameters_->device.handle);
  if (!device) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  device->device()->ClaimInterface(parameters_->interface_number, base::Bind(
      &UsbClaimInterfaceFunction::OnCompleted, this));
}

void UsbClaimInterfaceFunction::OnCompleted(bool success) {
  if (!success)
    SetError(kErrorCannotClaimInterface);
  AsyncWorkCompleted();
}

UsbReleaseInterfaceFunction::UsbReleaseInterfaceFunction() {}

UsbReleaseInterfaceFunction::~UsbReleaseInterfaceFunction() {}

bool UsbReleaseInterfaceFunction::Prepare() {
  parameters_ = ReleaseInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbReleaseInterfaceFunction::AsyncWorkStart() {
  UsbDeviceResource* device = GetUsbDeviceResource(parameters_->device.handle);
  if (!device) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  device->device()->ReleaseInterface(parameters_->interface_number, base::Bind(
      &UsbReleaseInterfaceFunction::OnCompleted, this));
}

void UsbReleaseInterfaceFunction::OnCompleted(bool success) {
  if (!success)
    SetError(kErrorCannotReleaseInterface);
  AsyncWorkCompleted();
}

UsbSetInterfaceAlternateSettingFunction::
    UsbSetInterfaceAlternateSettingFunction() {}

UsbSetInterfaceAlternateSettingFunction::
    ~UsbSetInterfaceAlternateSettingFunction() {}

bool UsbSetInterfaceAlternateSettingFunction::Prepare() {
  parameters_ = SetInterfaceAlternateSetting::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbSetInterfaceAlternateSettingFunction::AsyncWorkStart() {
  UsbDeviceResource* device = GetUsbDeviceResource(parameters_->device.handle);
  if (!device) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  device->device()->SetInterfaceAlternateSetting(
      parameters_->interface_number,
      parameters_->alternate_setting,
      base::Bind(&UsbSetInterfaceAlternateSettingFunction::OnCompleted, this));
}

void UsbSetInterfaceAlternateSettingFunction::OnCompleted(bool success) {
  if (!success)
    SetError(kErrorCannotSetInterfaceAlternateSetting);
  AsyncWorkCompleted();
}

UsbControlTransferFunction::UsbControlTransferFunction() {}

UsbControlTransferFunction::~UsbControlTransferFunction() {}

bool UsbControlTransferFunction::Prepare() {
  parameters_ = ControlTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbControlTransferFunction::AsyncWorkStart() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!device) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const ControlTransferInfo& transfer = parameters_->transfer_info;

  UsbDevice::TransferDirection direction;
  UsbDevice::TransferRequestType request_type;
  UsbDevice::TransferRecipient recipient;
  size_t size = 0;

  if (!ConvertDirectionSafely(transfer.direction, &direction) ||
      !ConvertRequestTypeSafely(transfer.request_type, &request_type) ||
      !ConvertRecipientSafely(transfer.recipient, &recipient)) {
    AsyncWorkCompleted();
    return;
  }

  if (!GetTransferSize(transfer, &size)) {
    CompleteWithError(kErrorInvalidTransferLength);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateBufferForTransfer(
      transfer, direction, size);
  if (!buffer) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  device->device()->ControlTransfer(direction, request_type, recipient,
      transfer.request, transfer.value, transfer.index, buffer, size, 0,
      base::Bind(&UsbControlTransferFunction::OnCompleted, this));
}

UsbBulkTransferFunction::UsbBulkTransferFunction() {}

UsbBulkTransferFunction::~UsbBulkTransferFunction() {}

bool UsbBulkTransferFunction::Prepare() {
  parameters_ = BulkTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbBulkTransferFunction::AsyncWorkStart() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!device) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const GenericTransferInfo& transfer = parameters_->transfer_info;

  UsbDevice::TransferDirection direction;
  size_t size = 0;

  if (!ConvertDirectionSafely(transfer.direction, &direction)) {
    AsyncWorkCompleted();
    return;
  }

  if (!GetTransferSize(transfer, &size)) {
    CompleteWithError(kErrorInvalidTransferLength);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateBufferForTransfer(
      transfer, direction, size);
  if (!buffer) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  device->device()->BulkTransfer(direction, transfer.endpoint, buffer, size, 0,
      base::Bind(&UsbBulkTransferFunction::OnCompleted, this));
}

UsbInterruptTransferFunction::UsbInterruptTransferFunction() {}

UsbInterruptTransferFunction::~UsbInterruptTransferFunction() {}

bool UsbInterruptTransferFunction::Prepare() {
  parameters_ = InterruptTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbInterruptTransferFunction::AsyncWorkStart() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!device) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const GenericTransferInfo& transfer = parameters_->transfer_info;

  UsbDevice::TransferDirection direction;
  size_t size = 0;

  if (!ConvertDirectionSafely(transfer.direction, &direction)) {
    AsyncWorkCompleted();
    return;
  }

  if (!GetTransferSize(transfer, &size)) {
    CompleteWithError(kErrorInvalidTransferLength);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateBufferForTransfer(
      transfer, direction, size);
  if (!buffer) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  device->device()->InterruptTransfer(direction, transfer.endpoint, buffer,
      size, 0, base::Bind(&UsbInterruptTransferFunction::OnCompleted, this));
}

UsbIsochronousTransferFunction::UsbIsochronousTransferFunction() {}

UsbIsochronousTransferFunction::~UsbIsochronousTransferFunction() {}

bool UsbIsochronousTransferFunction::Prepare() {
  parameters_ = IsochronousTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbIsochronousTransferFunction::AsyncWorkStart() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!device) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const IsochronousTransferInfo& transfer = parameters_->transfer_info;
  const GenericTransferInfo& generic_transfer = transfer.transfer_info;

  size_t size = 0;
  UsbDevice::TransferDirection direction;

  if (!ConvertDirectionSafely(generic_transfer.direction, &direction)) {
    AsyncWorkCompleted();
    return;
  }

  if (!GetTransferSize(generic_transfer, &size)) {
    CompleteWithError(kErrorInvalidTransferLength);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateBufferForTransfer(
      generic_transfer, direction, size);
  if (!buffer) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  device->device()->IsochronousTransfer(direction, generic_transfer.endpoint,
      buffer, size, transfer.packets, transfer.packet_length, 0, base::Bind(
          &UsbIsochronousTransferFunction::OnCompleted, this));
}

}  // namespace extensions
