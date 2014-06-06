// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb/usb_api.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "components/usb_service/usb_device_handle.h"
#include "components/usb_service/usb_service.h"
#include "extensions/browser/api/usb/usb_device_resource.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/usb.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"

namespace usb = extensions::core_api::usb;
namespace BulkTransfer = usb::BulkTransfer;
namespace ClaimInterface = usb::ClaimInterface;
namespace CloseDevice = usb::CloseDevice;
namespace ControlTransfer = usb::ControlTransfer;
namespace FindDevices = usb::FindDevices;
namespace GetDevices = usb::GetDevices;
namespace InterruptTransfer = usb::InterruptTransfer;
namespace IsochronousTransfer = usb::IsochronousTransfer;
namespace ListInterfaces = usb::ListInterfaces;
namespace OpenDevice = usb::OpenDevice;
namespace ReleaseInterface = usb::ReleaseInterface;
namespace RequestAccess = usb::RequestAccess;
namespace ResetDevice = usb::ResetDevice;
namespace SetInterfaceAlternateSetting = usb::SetInterfaceAlternateSetting;

using content::BrowserThread;
using std::string;
using std::vector;
using usb::ControlTransferInfo;
using usb::ConnectionHandle;
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
using usb_service::UsbConfigDescriptor;
using usb_service::UsbDevice;
using usb_service::UsbDeviceHandle;
using usb_service::UsbEndpointDescriptor;
using usb_service::UsbEndpointDirection;
using usb_service::UsbInterfaceAltSettingDescriptor;
using usb_service::UsbInterfaceDescriptor;
using usb_service::UsbService;
using usb_service::UsbSynchronizationType;
using usb_service::UsbTransferStatus;
using usb_service::UsbTransferType;
using usb_service::UsbUsageType;

typedef std::vector<scoped_refptr<UsbDevice> > DeviceVector;
typedef scoped_ptr<DeviceVector> ScopedDeviceVector;

namespace {

const char kDataKey[] = "data";
const char kResultCodeKey[] = "resultCode";

const char kErrorInitService[] = "Failed to initialize USB service.";

const char kErrorOpen[] = "Failed to open device.";
const char kErrorCancelled[] = "Transfer was cancelled.";
const char kErrorDisconnect[] = "Device disconnected.";
const char kErrorGeneric[] = "Transfer failed.";
#if !defined(OS_CHROMEOS)
const char kErrorNotSupported[] = "Not supported on this platform.";
#endif
const char kErrorOverflow[] = "Inbound transfer overflow.";
const char kErrorStalled[] = "Transfer stalled.";
const char kErrorTimeout[] = "Transfer timed out.";
const char kErrorTransferLength[] = "Transfer length is insufficient.";

const char kErrorCannotListInterfaces[] = "Error listing interfaces.";
const char kErrorCannotClaimInterface[] = "Error claiming interface.";
const char kErrorCannotReleaseInterface[] = "Error releasing interface.";
const char kErrorCannotSetInterfaceAlternateSetting[] =
    "Error setting alternate interface setting.";
const char kErrorConvertDirection[] = "Invalid transfer direction.";
const char kErrorConvertRecipient[] = "Invalid transfer recipient.";
const char kErrorConvertRequestType[] = "Invalid request type.";
const char kErrorConvertSynchronizationType[] = "Invalid synchronization type";
const char kErrorConvertTransferType[] = "Invalid endpoint type.";
const char kErrorConvertUsageType[] = "Invalid usage type.";
const char kErrorMalformedParameters[] = "Error parsing parameters.";
const char kErrorNoDevice[] = "No such device.";
const char kErrorPermissionDenied[] = "Permission to access device was denied";
const char kErrorInvalidTransferLength[] =
    "Transfer length must be a positive number less than 104,857,600.";
const char kErrorInvalidNumberOfPackets[] =
    "Number of packets must be a positive number less than 4,194,304.";
const char kErrorInvalidPacketLength[] =
    "Packet length must be a positive number less than 65,536.";
const char kErrorResetDevice[] =
    "Error resetting the device. The device has been closed.";

const size_t kMaxTransferLength = 100 * 1024 * 1024;
const int kMaxPackets = 4 * 1024 * 1024;
const int kMaxPacketLength = 64 * 1024;

bool ConvertDirectionToApi(const UsbEndpointDirection& input,
                           Direction* output) {
  switch (input) {
    case usb_service::USB_DIRECTION_INBOUND:
      *output = usb::DIRECTION_IN;
      return true;
    case usb_service::USB_DIRECTION_OUTBOUND:
      *output = usb::DIRECTION_OUT;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertSynchronizationTypeToApi(const UsbSynchronizationType& input,
                                     usb::SynchronizationType* output) {
  switch (input) {
    case usb_service::USB_SYNCHRONIZATION_NONE:
      *output = usb::SYNCHRONIZATION_TYPE_NONE;
      return true;
    case usb_service::USB_SYNCHRONIZATION_ASYNCHRONOUS:
      *output = usb::SYNCHRONIZATION_TYPE_ASYNCHRONOUS;
      return true;
    case usb_service::USB_SYNCHRONIZATION_ADAPTIVE:
      *output = usb::SYNCHRONIZATION_TYPE_ADAPTIVE;
      return true;
    case usb_service::USB_SYNCHRONIZATION_SYNCHRONOUS:
      *output = usb::SYNCHRONIZATION_TYPE_SYNCHRONOUS;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertTransferTypeToApi(const UsbTransferType& input,
                              usb::TransferType* output) {
  switch (input) {
    case usb_service::USB_TRANSFER_CONTROL:
      *output = usb::TRANSFER_TYPE_CONTROL;
      return true;
    case usb_service::USB_TRANSFER_INTERRUPT:
      *output = usb::TRANSFER_TYPE_INTERRUPT;
      return true;
    case usb_service::USB_TRANSFER_ISOCHRONOUS:
      *output = usb::TRANSFER_TYPE_ISOCHRONOUS;
      return true;
    case usb_service::USB_TRANSFER_BULK:
      *output = usb::TRANSFER_TYPE_BULK;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertUsageTypeToApi(const UsbUsageType& input, usb::UsageType* output) {
  switch (input) {
    case usb_service::USB_USAGE_DATA:
      *output = usb::USAGE_TYPE_DATA;
      return true;
    case usb_service::USB_USAGE_FEEDBACK:
      *output = usb::USAGE_TYPE_FEEDBACK;
      return true;
    case usb_service::USB_USAGE_EXPLICIT_FEEDBACK:
      *output = usb::USAGE_TYPE_EXPLICITFEEDBACK;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertDirection(const Direction& input, UsbEndpointDirection* output) {
  switch (input) {
    case usb::DIRECTION_IN:
      *output = usb_service::USB_DIRECTION_INBOUND;
      return true;
    case usb::DIRECTION_OUT:
      *output = usb_service::USB_DIRECTION_OUTBOUND;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertRequestType(const RequestType& input,
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

bool ConvertRecipient(const Recipient& input,
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

template <class T>
bool GetTransferSize(const T& input, size_t* output) {
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

template <class T>
scoped_refptr<net::IOBuffer> CreateBufferForTransfer(
    const T& input,
    UsbEndpointDirection direction,
    size_t size) {
  if (size >= kMaxTransferLength)
    return NULL;

  // Allocate a |size|-bytes buffer, or a one-byte buffer if |size| is 0. This
  // is due to an impedance mismatch between IOBuffer and URBs. An IOBuffer
  // cannot represent a zero-length buffer, while an URB can.
  scoped_refptr<net::IOBuffer> buffer =
      new net::IOBuffer(std::max(static_cast<size_t>(1), size));

  if (direction == usb_service::USB_DIRECTION_INBOUND) {
    return buffer;
  } else if (direction == usb_service::USB_DIRECTION_OUTBOUND) {
    if (input.data.get() && size <= input.data->size()) {
      memcpy(buffer->data(), input.data->data(), size);
      return buffer;
    }
  }
  NOTREACHED();
  return NULL;
}

const char* ConvertTransferStatusToErrorString(const UsbTransferStatus status) {
  switch (status) {
    case usb_service::USB_TRANSFER_COMPLETED:
      return "";
    case usb_service::USB_TRANSFER_ERROR:
      return kErrorGeneric;
    case usb_service::USB_TRANSFER_TIMEOUT:
      return kErrorTimeout;
    case usb_service::USB_TRANSFER_CANCELLED:
      return kErrorCancelled;
    case usb_service::USB_TRANSFER_STALLED:
      return kErrorStalled;
    case usb_service::USB_TRANSFER_DISCONNECT:
      return kErrorDisconnect;
    case usb_service::USB_TRANSFER_OVERFLOW:
      return kErrorOverflow;
    case usb_service::USB_TRANSFER_LENGTH_SHORT:
      return kErrorTransferLength;
    default:
      NOTREACHED();
      return "";
  }
}

#if defined(OS_CHROMEOS)
void RequestUsbDevicesAccessHelper(
    ScopedDeviceVector devices,
    std::vector<scoped_refptr<UsbDevice> >::iterator i,
    int interface_id,
    const base::Callback<void(ScopedDeviceVector result)>& callback,
    bool success) {
  if (success) {
    ++i;
  } else {
    i = devices->erase(i);
  }
  if (i == devices->end()) {
    callback.Run(devices.Pass());
    return;
  }
  (*i)->RequestUsbAcess(interface_id,
                        base::Bind(RequestUsbDevicesAccessHelper,
                                   base::Passed(devices.Pass()),
                                   i,
                                   interface_id,
                                   callback));
}

void RequestUsbDevicesAccess(
    ScopedDeviceVector devices,
    int interface_id,
    const base::Callback<void(ScopedDeviceVector result)>& callback) {
  if (devices->empty()) {
    callback.Run(devices.Pass());
    return;
  }
  std::vector<scoped_refptr<UsbDevice> >::iterator i = devices->begin();
  (*i)->RequestUsbAcess(interface_id,
                        base::Bind(RequestUsbDevicesAccessHelper,
                                   base::Passed(devices.Pass()),
                                   i,
                                   interface_id,
                                   callback));
}
#endif  // OS_CHROMEOS

base::DictionaryValue* CreateTransferInfo(UsbTransferStatus status,
                                          scoped_refptr<net::IOBuffer> data,
                                          size_t length) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kResultCodeKey, status);
  result->Set(kDataKey,
              base::BinaryValue::CreateWithCopiedBuffer(data->data(), length));
  return result;
}

base::Value* PopulateConnectionHandle(int handle,
                                      int vendor_id,
                                      int product_id) {
  ConnectionHandle result;
  result.handle = handle;
  result.vendor_id = vendor_id;
  result.product_id = product_id;
  return result.ToValue().release();
}

base::Value* PopulateDevice(UsbDevice* device) {
  Device result;
  result.device = device->unique_id();
  result.vendor_id = device->vendor_id();
  result.product_id = device->product_id();
  return result.ToValue().release();
}

base::Value* PopulateInterfaceDescriptor(
    int interface_number,
    int alternate_setting,
    int interface_class,
    int interface_subclass,
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

UsbAsyncApiFunction::UsbAsyncApiFunction() : manager_(NULL) {
}

UsbAsyncApiFunction::~UsbAsyncApiFunction() {
}

bool UsbAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<UsbDeviceResource>::Get(browser_context());
  set_work_thread_id(BrowserThread::FILE);
  return manager_ != NULL;
}

bool UsbAsyncApiFunction::Respond() {
  return error_.empty();
}

scoped_refptr<UsbDevice> UsbAsyncApiFunction::GetDeviceOrOrCompleteWithError(
    const Device& input_device) {
  const uint16_t vendor_id = input_device.vendor_id;
  const uint16_t product_id = input_device.product_id;
  UsbDevicePermission::CheckParam param(
      vendor_id, product_id, UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
  if (!GetExtension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return NULL;
  }

  UsbService* service = UsbService::GetInstance();
  if (!service) {
    CompleteWithError(kErrorInitService);
    return NULL;
  }
  scoped_refptr<UsbDevice> device;

  device = service->GetDeviceById(input_device.device);

  if (!device) {
    CompleteWithError(kErrorNoDevice);
    return NULL;
  }

  if (device->vendor_id() != input_device.vendor_id ||
      device->product_id() != input_device.product_id) {
    // Must act as if there is no such a device.
    // Otherwise can be used to finger print unauthorized devices.
    CompleteWithError(kErrorNoDevice);
    return NULL;
  }

  return device;
}

scoped_refptr<UsbDeviceHandle>
UsbAsyncApiFunction::GetDeviceHandleOrCompleteWithError(
    const ConnectionHandle& input_device_handle) {
  UsbDeviceResource* resource =
      manager_->Get(extension_->id(), input_device_handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return NULL;
  }

  if (!resource->device() || !resource->device()->GetDevice()) {
    CompleteWithError(kErrorDisconnect);
    manager_->Remove(extension_->id(), input_device_handle.handle);
    return NULL;
  }

  if (resource->device()->GetDevice()->vendor_id() !=
          input_device_handle.vendor_id ||
      resource->device()->GetDevice()->product_id() !=
          input_device_handle.product_id) {
    CompleteWithError(kErrorNoDevice);
    return NULL;
  }

  return resource->device();
}

void UsbAsyncApiFunction::RemoveUsbDeviceResource(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

void UsbAsyncApiFunction::CompleteWithError(const std::string& error) {
  SetError(error);
  AsyncWorkCompleted();
}

UsbAsyncApiTransferFunction::UsbAsyncApiTransferFunction() {
}

UsbAsyncApiTransferFunction::~UsbAsyncApiTransferFunction() {
}

void UsbAsyncApiTransferFunction::OnCompleted(UsbTransferStatus status,
                                              scoped_refptr<net::IOBuffer> data,
                                              size_t length) {
  if (status != usb_service::USB_TRANSFER_COMPLETED)
    SetError(ConvertTransferStatusToErrorString(status));

  SetResult(CreateTransferInfo(status, data, length));
  AsyncWorkCompleted();
}

bool UsbAsyncApiTransferFunction::ConvertDirectionSafely(
    const Direction& input,
    UsbEndpointDirection* output) {
  const bool converted = ConvertDirection(input, output);
  if (!converted)
    SetError(kErrorConvertDirection);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRequestTypeSafely(
    const RequestType& input,
    UsbDeviceHandle::TransferRequestType* output) {
  const bool converted = ConvertRequestType(input, output);
  if (!converted)
    SetError(kErrorConvertRequestType);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRecipientSafely(
    const Recipient& input,
    UsbDeviceHandle::TransferRecipient* output) {
  const bool converted = ConvertRecipient(input, output);
  if (!converted)
    SetError(kErrorConvertRecipient);
  return converted;
}

UsbFindDevicesFunction::UsbFindDevicesFunction() {
}

UsbFindDevicesFunction::~UsbFindDevicesFunction() {
}

bool UsbFindDevicesFunction::Prepare() {
  parameters_ = FindDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbFindDevicesFunction::AsyncWorkStart() {
  scoped_ptr<base::ListValue> result(new base::ListValue());
  const uint16_t vendor_id = parameters_->options.vendor_id;
  const uint16_t product_id = parameters_->options.product_id;
  int interface_id = parameters_->options.interface_id.get()
                         ? *parameters_->options.interface_id.get()
                         : UsbDevicePermissionData::ANY_INTERFACE;
  UsbDevicePermission::CheckParam param(vendor_id, product_id, interface_id);
  if (!GetExtension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  UsbService* service = UsbService::GetInstance();
  if (!service) {
    CompleteWithError(kErrorInitService);
    return;
  }

  ScopedDeviceVector devices(new DeviceVector());
  service->GetDevices(devices.get());

  for (DeviceVector::iterator it = devices->begin(); it != devices->end();) {
    if ((*it)->vendor_id() != vendor_id || (*it)->product_id() != product_id) {
      it = devices->erase(it);
    } else {
      ++it;
    }
  }

#if defined(OS_CHROMEOS)
  RequestUsbDevicesAccess(
      devices.Pass(),
      interface_id,
      base::Bind(&UsbFindDevicesFunction::OpenDevices, this));
#else
  OpenDevices(devices.Pass());
#endif  // OS_CHROMEOS
}

void UsbFindDevicesFunction::OpenDevices(ScopedDeviceVector devices) {
  base::ListValue* result = new base::ListValue();

  for (size_t i = 0; i < devices->size(); ++i) {
    scoped_refptr<UsbDeviceHandle> device_handle = devices->at(i)->Open();
    if (device_handle)
      device_handles_.push_back(device_handle);
  }

  for (size_t i = 0; i < device_handles_.size(); ++i) {
    UsbDeviceHandle* const device_handle = device_handles_[i].get();
    UsbDeviceResource* const resource =
        new UsbDeviceResource(extension_->id(), device_handle);

    result->Append(PopulateConnectionHandle(manager_->Add(resource),
                                            parameters_->options.vendor_id,
                                            parameters_->options.product_id));
  }

  SetResult(result);
  AsyncWorkCompleted();
}

UsbGetDevicesFunction::UsbGetDevicesFunction() {
}

UsbGetDevicesFunction::~UsbGetDevicesFunction() {
}

bool UsbGetDevicesFunction::Prepare() {
  parameters_ = GetDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbGetDevicesFunction::AsyncWorkStart() {
  scoped_ptr<base::ListValue> result(new base::ListValue());

  const uint16_t vendor_id = parameters_->options.vendor_id;
  const uint16_t product_id = parameters_->options.product_id;
  UsbDevicePermission::CheckParam param(
      vendor_id, product_id, UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
  if (!GetExtension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  UsbService* service = UsbService::GetInstance();
  if (!service) {
    CompleteWithError(kErrorInitService);
    return;
  }

  DeviceVector devices;
  service->GetDevices(&devices);

  for (DeviceVector::iterator it = devices.begin(); it != devices.end();) {
    if ((*it)->vendor_id() != vendor_id || (*it)->product_id() != product_id) {
      it = devices.erase(it);
    } else {
      ++it;
    }
  }

  for (size_t i = 0; i < devices.size(); ++i) {
    result->Append(PopulateDevice(devices[i].get()));
  }

  SetResult(result.release());
  AsyncWorkCompleted();
}

UsbRequestAccessFunction::UsbRequestAccessFunction() {
}

UsbRequestAccessFunction::~UsbRequestAccessFunction() {
}

bool UsbRequestAccessFunction::Prepare() {
  parameters_ = RequestAccess::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbRequestAccessFunction::AsyncWorkStart() {
#if defined(OS_CHROMEOS)
  scoped_refptr<UsbDevice> device =
      GetDeviceOrOrCompleteWithError(parameters_->device);
  if (!device)
    return;

  device->RequestUsbAcess(
      parameters_->interface_id,
      base::Bind(&UsbRequestAccessFunction::OnCompleted, this));
#else
  SetResult(new base::FundamentalValue(false));
  CompleteWithError(kErrorNotSupported);
#endif  // OS_CHROMEOS
}

void UsbRequestAccessFunction::OnCompleted(bool success) {
  SetResult(new base::FundamentalValue(success));
  AsyncWorkCompleted();
}

UsbOpenDeviceFunction::UsbOpenDeviceFunction() {
}

UsbOpenDeviceFunction::~UsbOpenDeviceFunction() {
}

bool UsbOpenDeviceFunction::Prepare() {
  parameters_ = OpenDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbOpenDeviceFunction::AsyncWorkStart() {
  scoped_refptr<UsbDevice> device =
      GetDeviceOrOrCompleteWithError(parameters_->device);
  if (!device)
    return;

  handle_ = device->Open();
  if (!handle_) {
    SetError(kErrorOpen);
    AsyncWorkCompleted();
    return;
  }

  SetResult(PopulateConnectionHandle(
      manager_->Add(new UsbDeviceResource(extension_->id(), handle_)),
      handle_->GetDevice()->vendor_id(),
      handle_->GetDevice()->product_id()));
  AsyncWorkCompleted();
}

UsbListInterfacesFunction::UsbListInterfacesFunction() {
}

UsbListInterfacesFunction::~UsbListInterfacesFunction() {
}

bool UsbListInterfacesFunction::Prepare() {
  parameters_ = ListInterfaces::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbListInterfacesFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

  scoped_refptr<UsbConfigDescriptor> config =
      device_handle->GetDevice()->ListInterfaces();

  if (!config) {
    SetError(kErrorCannotListInterfaces);
    AsyncWorkCompleted();
    return;
  }

  result_.reset(new base::ListValue());

  for (size_t i = 0, num_interfaces = config->GetNumInterfaces();
       i < num_interfaces;
       ++i) {
    scoped_refptr<const UsbInterfaceDescriptor> usb_interface(
        config->GetInterface(i));
    for (size_t j = 0, num_descriptors = usb_interface->GetNumAltSettings();
         j < num_descriptors;
         ++j) {
      scoped_refptr<const UsbInterfaceAltSettingDescriptor> descriptor =
          usb_interface->GetAltSetting(j);
      std::vector<linked_ptr<EndpointDescriptor> > endpoints;
      for (size_t k = 0, num_endpoints = descriptor->GetNumEndpoints();
           k < num_endpoints;
           k++) {
        scoped_refptr<const UsbEndpointDescriptor> endpoint =
            descriptor->GetEndpoint(k);
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

      result_->Append(
          PopulateInterfaceDescriptor(descriptor->GetInterfaceNumber(),
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
    usb::Direction* output) {
  const bool converted = ConvertDirectionToApi(input, output);
  if (!converted)
    SetError(kErrorConvertDirection);
  return converted;
}

bool UsbListInterfacesFunction::ConvertSynchronizationTypeSafely(
    const UsbSynchronizationType& input,
    usb::SynchronizationType* output) {
  const bool converted = ConvertSynchronizationTypeToApi(input, output);
  if (!converted)
    SetError(kErrorConvertSynchronizationType);
  return converted;
}

bool UsbListInterfacesFunction::ConvertTransferTypeSafely(
    const UsbTransferType& input,
    usb::TransferType* output) {
  const bool converted = ConvertTransferTypeToApi(input, output);
  if (!converted)
    SetError(kErrorConvertTransferType);
  return converted;
}

bool UsbListInterfacesFunction::ConvertUsageTypeSafely(
    const UsbUsageType& input,
    usb::UsageType* output) {
  const bool converted = ConvertUsageTypeToApi(input, output);
  if (!converted)
    SetError(kErrorConvertUsageType);
  return converted;
}

UsbCloseDeviceFunction::UsbCloseDeviceFunction() {
}

UsbCloseDeviceFunction::~UsbCloseDeviceFunction() {
}

bool UsbCloseDeviceFunction::Prepare() {
  parameters_ = CloseDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbCloseDeviceFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

  device_handle->Close();
  RemoveUsbDeviceResource(parameters_->handle.handle);
  AsyncWorkCompleted();
}

UsbClaimInterfaceFunction::UsbClaimInterfaceFunction() {
}

UsbClaimInterfaceFunction::~UsbClaimInterfaceFunction() {
}

bool UsbClaimInterfaceFunction::Prepare() {
  parameters_ = ClaimInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbClaimInterfaceFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

  bool success = device_handle->ClaimInterface(parameters_->interface_number);

  if (!success)
    SetError(kErrorCannotClaimInterface);
  AsyncWorkCompleted();
}

UsbReleaseInterfaceFunction::UsbReleaseInterfaceFunction() {
}

UsbReleaseInterfaceFunction::~UsbReleaseInterfaceFunction() {
}

bool UsbReleaseInterfaceFunction::Prepare() {
  parameters_ = ReleaseInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbReleaseInterfaceFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

  bool success = device_handle->ReleaseInterface(parameters_->interface_number);
  if (!success)
    SetError(kErrorCannotReleaseInterface);
  AsyncWorkCompleted();
}

UsbSetInterfaceAlternateSettingFunction::
    UsbSetInterfaceAlternateSettingFunction() {
}

UsbSetInterfaceAlternateSettingFunction::
    ~UsbSetInterfaceAlternateSettingFunction() {
}

bool UsbSetInterfaceAlternateSettingFunction::Prepare() {
  parameters_ = SetInterfaceAlternateSetting::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbSetInterfaceAlternateSettingFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

  bool success = device_handle->SetInterfaceAlternateSetting(
      parameters_->interface_number, parameters_->alternate_setting);
  if (!success)
    SetError(kErrorCannotSetInterfaceAlternateSetting);

  AsyncWorkCompleted();
}

UsbControlTransferFunction::UsbControlTransferFunction() {
}

UsbControlTransferFunction::~UsbControlTransferFunction() {
}

bool UsbControlTransferFunction::Prepare() {
  parameters_ = ControlTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbControlTransferFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

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

  scoped_refptr<net::IOBuffer> buffer =
      CreateBufferForTransfer(transfer, direction, size);
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  device_handle->ControlTransfer(
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

UsbBulkTransferFunction::UsbBulkTransferFunction() {
}

UsbBulkTransferFunction::~UsbBulkTransferFunction() {
}

bool UsbBulkTransferFunction::Prepare() {
  parameters_ = BulkTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbBulkTransferFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

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

  scoped_refptr<net::IOBuffer> buffer =
      CreateBufferForTransfer(transfer, direction, size);
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  device_handle->BulkTransfer(
      direction,
      transfer.endpoint,
      buffer.get(),
      size,
      0,
      base::Bind(&UsbBulkTransferFunction::OnCompleted, this));
}

UsbInterruptTransferFunction::UsbInterruptTransferFunction() {
}

UsbInterruptTransferFunction::~UsbInterruptTransferFunction() {
}

bool UsbInterruptTransferFunction::Prepare() {
  parameters_ = InterruptTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbInterruptTransferFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

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

  scoped_refptr<net::IOBuffer> buffer =
      CreateBufferForTransfer(transfer, direction, size);
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  device_handle->InterruptTransfer(
      direction,
      transfer.endpoint,
      buffer.get(),
      size,
      0,
      base::Bind(&UsbInterruptTransferFunction::OnCompleted, this));
}

UsbIsochronousTransferFunction::UsbIsochronousTransferFunction() {
}

UsbIsochronousTransferFunction::~UsbIsochronousTransferFunction() {
}

bool UsbIsochronousTransferFunction::Prepare() {
  parameters_ = IsochronousTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbIsochronousTransferFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

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

  scoped_refptr<net::IOBuffer> buffer =
      CreateBufferForTransfer(generic_transfer, direction, size);
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  device_handle->IsochronousTransfer(
      direction,
      generic_transfer.endpoint,
      buffer.get(),
      size,
      packets,
      packet_length,
      0,
      base::Bind(&UsbIsochronousTransferFunction::OnCompleted, this));
}

UsbResetDeviceFunction::UsbResetDeviceFunction() {
}

UsbResetDeviceFunction::~UsbResetDeviceFunction() {
}

bool UsbResetDeviceFunction::Prepare() {
  parameters_ = ResetDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbResetDeviceFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle)
    return;

  bool success = device_handle->ResetDevice();
  if (!success) {
    device_handle->Close();
    RemoveUsbDeviceResource(parameters_->handle.handle);
    SetResult(new base::FundamentalValue(false));
    CompleteWithError(kErrorResetDevice);
    return;
  }

  SetResult(new base::FundamentalValue(true));
  AsyncWorkCompleted();
}

}  // namespace extensions
