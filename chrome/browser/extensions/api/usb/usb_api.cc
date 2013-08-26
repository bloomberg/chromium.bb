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
#include "chrome/browser/usb/usb_device_handle.h"
#include "chrome/browser/usb/usb_service.h"
#include "chrome/common/extensions/api/usb.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"

namespace BulkTransfer = extensions::api::usb::BulkTransfer;
namespace ClaimInterface = extensions::api::usb::ClaimInterface;
namespace ListInterfaces = extensions::api::usb::ListInterfaces;
namespace CloseDevice = extensions::api::usb::CloseDevice;
namespace ControlTransfer = extensions::api::usb::ControlTransfer;
namespace FindDevices = extensions::api::usb::FindDevices;
namespace InterruptTransfer = extensions::api::usb::InterruptTransfer;
namespace IsochronousTransfer = extensions::api::usb::IsochronousTransfer;
namespace ReleaseInterface = extensions::api::usb::ReleaseInterface;
namespace ResetDevice = extensions::api::usb::ResetDevice;
namespace SetInterfaceAlternateSetting =
    extensions::api::usb::SetInterfaceAlternateSetting;
namespace usb = extensions::api::usb;

using content::BrowserThread;
using std::string;
using std::vector;
using usb::ControlTransferInfo;
using usb::Device;
using usb::Direction;
using usb::EndpointDescriptor;
using usb::GenericTransferInfo;
using usb::InterfaceDescriptor;
using usb::IsochronousTransferInfo;
using usb::Recipient;
using usb::RequestType;
using usb::SynchronizationType;
using usb::TransferType;
using usb::UsageType;

namespace {

static const char kDataKey[] = "data";
static const char kResultCodeKey[] = "resultCode";

static const char kErrorCancelled[] = "Transfer was cancelled.";
static const char kErrorDisconnect[] = "Device disconnected.";
static const char kErrorGeneric[] = "Transfer failed.";
static const char kErrorOverflow[] = "Inbound transfer overflow.";
static const char kErrorStalled[] = "Transfer stalled.";
static const char kErrorTimeout[] = "Transfer timed out.";
static const char kErrorTransferLength[] = "Transfer length is insufficient.";

static const char kErrorCannotListInterfaces[] = "Error listing interfaces.";
static const char kErrorCannotClaimInterface[] = "Error claiming interface.";
static const char kErrorCannotReleaseInterface[] = "Error releasing interface.";
static const char kErrorCannotSetInterfaceAlternateSetting[] =
    "Error setting alternate interface setting.";
static const char kErrorConvertDirection[] = "Invalid transfer direction.";
static const char kErrorConvertRecipient[] = "Invalid transfer recipient.";
static const char kErrorConvertRequestType[] = "Invalid request type.";
static const char kErrorConvertSynchronizationType[] =
    "Invalid synchronization type";
static const char kErrorConvertTransferType[] = "Invalid endpoint type.";
static const char kErrorConvertUsageType[] = "Invalid usage type.";
static const char kErrorMalformedParameters[] = "Error parsing parameters.";
static const char kErrorNoDevice[] = "No such device.";
static const char kErrorPermissionDenied[] =
    "Permission to access device was denied";
static const char kErrorInvalidTransferLength[] = "Transfer length must be a "
    "positive number less than 104,857,600.";
static const char kErrorInvalidNumberOfPackets[] = "Number of packets must be "
    "a positive number less than 4,194,304.";
static const char kErrorInvalidPacketLength[] = "Packet length must be a "
    "positive number less than 65,536.";
static const char kErrorResetDevice[] =
    "Error resetting the device. The device has been closed.";

static const size_t kMaxTransferLength = 100 * 1024 * 1024;
static const int kMaxPackets = 4 * 1024 * 1024;
static const int kMaxPacketLength = 64 * 1024;

static UsbDevice* g_device_for_test = NULL;

static bool ConvertDirectionToApi(const UsbEndpointDirection& input,
                                  Direction* output) {
  switch (input) {
    case USB_DIRECTION_INBOUND:
      *output = usb::DIRECTION_IN;
      return true;
    case USB_DIRECTION_OUTBOUND:
      *output = usb::DIRECTION_OUT;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertSynchronizationTypeToApi(
    const UsbSynchronizationType& input,
    extensions::api::usb::SynchronizationType* output) {
  switch (input) {
    case USB_SYNCHRONIZATION_NONE:
      *output = usb::SYNCHRONIZATION_TYPE_NONE;
      return true;
    case USB_SYNCHRONIZATION_ASYNCHRONOUS:
      *output = usb::SYNCHRONIZATION_TYPE_ASYNCHRONOUS;
      return true;
    case USB_SYNCHRONIZATION_ADAPTIVE:
      *output = usb::SYNCHRONIZATION_TYPE_ADAPTIVE;
      return true;
    case USB_SYNCHRONIZATION_SYNCHRONOUS:
      *output = usb::SYNCHRONIZATION_TYPE_SYNCHRONOUS;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertTransferTypeToApi(
    const UsbTransferType& input,
    extensions::api::usb::TransferType* output) {
  switch (input) {
    case USB_TRANSFER_CONTROL:
      *output = usb::TRANSFER_TYPE_CONTROL;
      return true;
    case USB_TRANSFER_INTERRUPT:
      *output = usb::TRANSFER_TYPE_INTERRUPT;
      return true;
    case USB_TRANSFER_ISOCHRONOUS:
      *output = usb::TRANSFER_TYPE_ISOCHRONOUS;
      return true;
    case USB_TRANSFER_BULK:
      *output = usb::TRANSFER_TYPE_BULK;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertUsageTypeToApi(const UsbUsageType& input,
    extensions::api::usb::UsageType* output) {
  switch (input) {
    case USB_USAGE_DATA:
      *output = usb::USAGE_TYPE_DATA;
      return true;
    case USB_USAGE_FEEDBACK:
      *output = usb::USAGE_TYPE_FEEDBACK;
      return true;
    case USB_USAGE_EXPLICIT_FEEDBACK:
      *output = usb::USAGE_TYPE_EXPLICITFEEDBACK;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertDirection(const Direction& input,
                             UsbEndpointDirection* output) {
  switch (input) {
    case usb::DIRECTION_IN:
      *output = USB_DIRECTION_INBOUND;
      return true;
    case usb::DIRECTION_OUT:
      *output = USB_DIRECTION_OUTBOUND;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertRequestType(const RequestType& input,
                               UsbDeviceHandle::TransferRequestType* output) {
  switch (input) {
    case usb::REQUEST_TYPE_STANDARD:
      *output = UsbDeviceHandle::STANDARD;
      return true;
    case usb::REQUEST_TYPE_CLASS:
      *output = UsbDeviceHandle::CLASS;
      return true;
    case usb::REQUEST_TYPE_VENDOR:
      *output = UsbDeviceHandle::VENDOR;
      return true;
    case usb::REQUEST_TYPE_RESERVED:
      *output = UsbDeviceHandle::RESERVED;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertRecipient(const Recipient& input,
                             UsbDeviceHandle::TransferRecipient* output) {
  switch (input) {
    case usb::RECIPIENT_DEVICE:
      *output = UsbDeviceHandle::DEVICE;
      return true;
    case usb::RECIPIENT_INTERFACE:
      *output = UsbDeviceHandle::INTERFACE;
      return true;
    case usb::RECIPIENT_ENDPOINT:
      *output = UsbDeviceHandle::ENDPOINT;
      return true;
    case usb::RECIPIENT_OTHER:
      *output = UsbDeviceHandle::OTHER;
      return true;
    default:
      NOTREACHED();
      return false;
  }
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
    const T& input, UsbEndpointDirection direction, size_t size) {

  if (size >= kMaxTransferLength)
    return NULL;

  // Allocate a |size|-bytes buffer, or a one-byte buffer if |size| is 0. This
  // is due to an impedance mismatch between IOBuffer and URBs. An IOBuffer
  // cannot represent a zero-length buffer, while an URB can.
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(std::max(
      static_cast<size_t>(1), size));

  if (direction == USB_DIRECTION_INBOUND) {
    return buffer;
  } else if (direction == USB_DIRECTION_OUTBOUND) {
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
    default:
      NOTREACHED();
      return "";
  }
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

static base::Value* PopulateInterfaceDescriptor(int interface_number,
        int alternate_setting, int interface_class, int interface_subclass,
        int interface_protocol,
        std::vector<linked_ptr<EndpointDescriptor> >* endpoints) {
  InterfaceDescriptor descriptor;
  descriptor.interface_number = interface_number;
  descriptor.alternate_setting = alternate_setting;
  descriptor.interface_class = interface_class;
  descriptor.interface_subclass = interface_subclass;
  descriptor.interface_protocol = interface_protocol;
  descriptor.endpoints = *endpoints;
  return descriptor.ToValue().release();
}

}  // namespace

namespace extensions {

UsbAsyncApiFunction::UsbAsyncApiFunction()
    : manager_(NULL) {
}

UsbAsyncApiFunction::~UsbAsyncApiFunction() {
}

bool UsbAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<UsbDeviceResource>::Get(profile());
  set_work_thread_id(BrowserThread::FILE);
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
    const Direction& input, UsbEndpointDirection* output) {
  const bool converted = ConvertDirection(input, output);
  if (!converted)
    SetError(kErrorConvertDirection);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRequestTypeSafely(
    const RequestType& input, UsbDeviceHandle::TransferRequestType* output) {
  const bool converted = ConvertRequestType(input, output);
  if (!converted)
    SetError(kErrorConvertRequestType);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRecipientSafely(
    const Recipient& input, UsbDeviceHandle::TransferRecipient* output) {
  const bool converted = ConvertRecipient(input, output);
  if (!converted)
    SetError(kErrorConvertRecipient);
  return converted;
}

UsbFindDevicesFunction::UsbFindDevicesFunction() {}

UsbFindDevicesFunction::~UsbFindDevicesFunction() {}

void UsbFindDevicesFunction::SetDeviceForTest(UsbDevice* device) {
  g_device_for_test = device;
}

bool UsbFindDevicesFunction::Prepare() {
  parameters_ = FindDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbFindDevicesFunction::AsyncWorkStart() {
  result_.reset(new base::ListValue());

  if (g_device_for_test) {
    UsbDeviceResource* const resource = new UsbDeviceResource(
        extension_->id(),
        g_device_for_test->Open());

    Device device;
    result_->Append(PopulateDevice(manager_->Add(resource), 0, 0));
    SetResult(result_.release());
    AsyncWorkCompleted();
    return;
  }

  const uint16_t vendor_id = parameters_->options.vendor_id;
  const uint16_t product_id = parameters_->options.product_id;
  int interface_id = parameters_->options.interface_id.get() ?
      *parameters_->options.interface_id.get() :
      UsbDevicePermissionData::ANY_INTERFACE;
  UsbDevicePermission::CheckParam param(vendor_id, product_id, interface_id);
  if (!PermissionsData::CheckAPIPermissionWithParam(
          GetExtension(), APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  UsbService *service = UsbService::GetInstance();
  service->FindDevices(
      vendor_id, product_id, interface_id,
      base::Bind(&UsbFindDevicesFunction::EnumerationCompletedFileThread,
                 this));
}

void UsbFindDevicesFunction::EnumerationCompletedFileThread(
    scoped_ptr<std::vector<scoped_refptr<UsbDevice> > > devices) {
  for (size_t i = 0; i < devices->size(); ++i) {
    scoped_refptr<UsbDeviceHandle> device_handle =
      devices->at(i)->Open();
    if (device_handle)
      device_handles_.push_back(device_handle);
  }

  for (size_t i = 0; i < device_handles_.size(); ++i) {
    UsbDeviceHandle* const device_handle = device_handles_[i].get();
    UsbDeviceResource* const resource =
        new UsbDeviceResource(extension_->id(), device_handle);

    result_->Append(PopulateDevice(manager_->Add(resource),
                                   parameters_->options.vendor_id,
                                   parameters_->options.product_id));
  }

  SetResult(result_.release());
  AsyncWorkCompleted();
}

UsbListInterfacesFunction::UsbListInterfacesFunction() {}

UsbListInterfacesFunction::~UsbListInterfacesFunction() {}

bool UsbListInterfacesFunction::Prepare() {
  parameters_ = ListInterfaces::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbListInterfacesFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  scoped_refptr<UsbConfigDescriptor> config =
      resource->device()->device()->ListInterfaces();

  if (!config) {
    SetError(kErrorCannotListInterfaces);
    AsyncWorkCompleted();
    return;
  }

  result_.reset(new base::ListValue());

  for (size_t i = 0, num_interfaces = config->GetNumInterfaces();
      i < num_interfaces; ++i) {
    scoped_refptr<const UsbInterfaceDescriptor>
        usb_interface(config->GetInterface(i));
    for (size_t j = 0, num_descriptors = usb_interface->GetNumAltSettings();
            j < num_descriptors; ++j) {
      scoped_refptr<const UsbInterfaceAltSettingDescriptor> descriptor
          = usb_interface->GetAltSetting(j);
      std::vector<linked_ptr<EndpointDescriptor> > endpoints;
      for (size_t k = 0, num_endpoints = descriptor->GetNumEndpoints();
          k < num_endpoints; k++) {
        scoped_refptr<const UsbEndpointDescriptor> endpoint
            = descriptor->GetEndpoint(k);
        linked_ptr<EndpointDescriptor> endpoint_desc(new EndpointDescriptor());

        TransferType type;
        Direction direction;
        SynchronizationType synchronization;
        UsageType usage;

        if (!ConvertTransferTypeSafely(endpoint->GetTransferType(), &type) ||
            !ConvertDirectionSafely(endpoint->GetDirection(), &direction) ||
            !ConvertSynchronizationTypeSafely(
                endpoint->GetSynchronizationType(), &synchronization) ||
            !ConvertUsageTypeSafely(endpoint->GetUsageType(), &usage)) {
          SetError(kErrorCannotListInterfaces);
          AsyncWorkCompleted();
          return;
        }

        endpoint_desc->address = endpoint->GetAddress();
        endpoint_desc->type = type;
        endpoint_desc->direction = direction;
        endpoint_desc->maximum_packet_size = endpoint->GetMaximumPacketSize();
        endpoint_desc->synchronization = synchronization;
        endpoint_desc->usage = usage;

        int* polling_interval = new int;
        endpoint_desc->polling_interval.reset(polling_interval);
        *polling_interval = endpoint->GetPollingInterval();

        endpoints.push_back(endpoint_desc);
      }

      result_->Append(PopulateInterfaceDescriptor(
          descriptor->GetInterfaceNumber(),
          descriptor->GetAlternateSetting(),
          descriptor->GetInterfaceClass(),
          descriptor->GetInterfaceSubclass(),
          descriptor->GetInterfaceProtocol(),
          &endpoints));
    }
  }

  SetResult(result_.release());
  AsyncWorkCompleted();
}

bool UsbListInterfacesFunction::ConvertDirectionSafely(
    const UsbEndpointDirection& input,
    extensions::api::usb::Direction* output) {
  const bool converted = ConvertDirectionToApi(input, output);
  if (!converted)
    SetError(kErrorConvertDirection);
  return converted;
}

bool UsbListInterfacesFunction::ConvertSynchronizationTypeSafely(
    const UsbSynchronizationType& input,
    extensions::api::usb::SynchronizationType* output) {
  const bool converted = ConvertSynchronizationTypeToApi(input, output);
  if (!converted)
    SetError(kErrorConvertSynchronizationType);
  return converted;
}

bool UsbListInterfacesFunction::ConvertTransferTypeSafely(
    const UsbTransferType& input,
    extensions::api::usb::TransferType* output) {
  const bool converted = ConvertTransferTypeToApi(input, output);
  if (!converted)
    SetError(kErrorConvertTransferType);
  return converted;
}

bool UsbListInterfacesFunction::ConvertUsageTypeSafely(
    const UsbUsageType& input,
    extensions::api::usb::UsageType* output) {
  const bool converted = ConvertUsageTypeToApi(input, output);
  if (!converted)
    SetError(kErrorConvertUsageType);
  return converted;
}

UsbCloseDeviceFunction::UsbCloseDeviceFunction() {}

UsbCloseDeviceFunction::~UsbCloseDeviceFunction() {}

bool UsbCloseDeviceFunction::Prepare() {
  parameters_ = CloseDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbCloseDeviceFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  resource->device()->Close();
  RemoveUsbDeviceResource(parameters_->device.handle);
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
  UsbDeviceResource* resource =
      GetUsbDeviceResource(parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  bool success =
      resource->device()->ClaimInterface(parameters_->interface_number);

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
  UsbDeviceResource* resource =
      GetUsbDeviceResource(parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }
  bool success =
      resource->device()->ReleaseInterface(parameters_->interface_number);
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
  UsbDeviceResource* resource =
      GetUsbDeviceResource(parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  bool success = resource->device()->SetInterfaceAlternateSetting(
      parameters_->interface_number,
      parameters_->alternate_setting);
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
  UsbDeviceResource* const resource = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const ControlTransferInfo& transfer = parameters_->transfer_info;

  UsbEndpointDirection direction;
  UsbDeviceHandle::TransferRequestType request_type;
  UsbDeviceHandle::TransferRecipient recipient;
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
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  resource->device()->ControlTransfer(
      direction,
      request_type,
      recipient,
      transfer.request,
      transfer.value,
      transfer.index,
      buffer.get(),
      size,
      0,
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
  UsbDeviceResource* const resource = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const GenericTransferInfo& transfer = parameters_->transfer_info;

  UsbEndpointDirection direction;
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
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  resource->device()
      ->BulkTransfer(direction,
                     transfer.endpoint,
                     buffer.get(),
                     size,
                     0,
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
  UsbDeviceResource* const resource = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const GenericTransferInfo& transfer = parameters_->transfer_info;

  UsbEndpointDirection direction;
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
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  resource->device()->InterruptTransfer(
      direction,
      transfer.endpoint,
      buffer.get(),
      size,
      0,
      base::Bind(&UsbInterruptTransferFunction::OnCompleted, this));
}

UsbIsochronousTransferFunction::UsbIsochronousTransferFunction() {}

UsbIsochronousTransferFunction::~UsbIsochronousTransferFunction() {}

bool UsbIsochronousTransferFunction::Prepare() {
  parameters_ = IsochronousTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbIsochronousTransferFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const IsochronousTransferInfo& transfer = parameters_->transfer_info;
  const GenericTransferInfo& generic_transfer = transfer.transfer_info;

  size_t size = 0;
  UsbEndpointDirection direction;

  if (!ConvertDirectionSafely(generic_transfer.direction, &direction)) {
    AsyncWorkCompleted();
    return;
  }
  if (!GetTransferSize(generic_transfer, &size)) {
    CompleteWithError(kErrorInvalidTransferLength);
    return;
  }
  if (transfer.packets < 0 || transfer.packets >= kMaxPackets) {
    CompleteWithError(kErrorInvalidNumberOfPackets);
    return;
  }
  unsigned int packets = transfer.packets;
  if (transfer.packet_length < 0 ||
      transfer.packet_length >= kMaxPacketLength) {
    CompleteWithError(kErrorInvalidPacketLength);
    return;
  }
  unsigned int packet_length = transfer.packet_length;
  const uint64 total_length = packets * packet_length;
  if (packets > size || total_length > size) {
    CompleteWithError(kErrorTransferLength);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateBufferForTransfer(
      generic_transfer, direction, size);
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  resource->device()->IsochronousTransfer(
      direction,
      generic_transfer.endpoint,
      buffer.get(),
      size,
      packets,
      packet_length,
      0,
      base::Bind(&UsbIsochronousTransferFunction::OnCompleted, this));
}

UsbResetDeviceFunction::UsbResetDeviceFunction() {}

UsbResetDeviceFunction::~UsbResetDeviceFunction() {}

bool UsbResetDeviceFunction::Prepare() {
  parameters_ = ResetDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbResetDeviceFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource = GetUsbDeviceResource(
      parameters_->device.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  bool success = resource->device()->ResetDevice();
  if (!success) {
    UsbDeviceResource* const resource = GetUsbDeviceResource(
        parameters_->device.handle);
    if (!resource) {
      CompleteWithError(kErrorNoDevice);
      return;
    }
    resource->device()->Close();
    RemoveUsbDeviceResource(parameters_->device.handle);
    SetError(kErrorResetDevice);
    SetResult(new base::FundamentalValue(false));
    AsyncWorkCompleted();
    return;
  }
  SetResult(new base::FundamentalValue(true));
  AsyncWorkCompleted();
}

}  // namespace extensions
