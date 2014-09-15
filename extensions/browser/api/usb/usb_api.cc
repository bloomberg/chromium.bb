// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb/usb_api.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "device/core/device_client.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_service.h"
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
namespace GetConfiguration = usb::GetConfiguration;
namespace ListInterfaces = usb::ListInterfaces;
namespace OpenDevice = usb::OpenDevice;
namespace ReleaseInterface = usb::ReleaseInterface;
namespace RequestAccess = usb::RequestAccess;
namespace ResetDevice = usb::ResetDevice;
namespace SetInterfaceAlternateSetting = usb::SetInterfaceAlternateSetting;

using content::BrowserThread;
using usb::ConfigDescriptor;
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
using device::UsbConfigDescriptor;
using device::UsbDevice;
using device::UsbDeviceFilter;
using device::UsbDeviceHandle;
using device::UsbEndpointDescriptor;
using device::UsbEndpointDirection;
using device::UsbInterfaceDescriptor;
using device::UsbService;
using device::UsbSynchronizationType;
using device::UsbTransferStatus;
using device::UsbTransferType;
using device::UsbUsageType;

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
const char kErrorCannotClaimInterface[] = "Error claiming interface.";
const char kErrorCannotReleaseInterface[] = "Error releasing interface.";
const char kErrorCannotSetInterfaceAlternateSetting[] =
    "Error setting alternate interface setting.";
const char kErrorConvertDirection[] = "Invalid transfer direction.";
const char kErrorConvertRecipient[] = "Invalid transfer recipient.";
const char kErrorConvertRequestType[] = "Invalid request type.";
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

bool ConvertDirectionFromApi(const Direction& input,
                             UsbEndpointDirection* output) {
  switch (input) {
    case usb::DIRECTION_IN:
      *output = device::USB_DIRECTION_INBOUND;
      return true;
    case usb::DIRECTION_OUT:
      *output = device::USB_DIRECTION_OUTBOUND;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertRequestTypeFromApi(const RequestType& input,
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

bool ConvertRecipientFromApi(const Recipient& input,
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

  if (direction == device::USB_DIRECTION_INBOUND) {
    return buffer;
  } else if (direction == device::USB_DIRECTION_OUTBOUND) {
    if (input.data.get() && size <= input.data->size()) {
      memcpy(buffer->data(), input.data->data(), size);
      return buffer;
    }
  }
  NOTREACHED();
  return NULL;
}

const char* ConvertTransferStatusToApi(const UsbTransferStatus status) {
  switch (status) {
    case device::USB_TRANSFER_COMPLETED:
      return "";
    case device::USB_TRANSFER_ERROR:
      return kErrorGeneric;
    case device::USB_TRANSFER_TIMEOUT:
      return kErrorTimeout;
    case device::USB_TRANSFER_CANCELLED:
      return kErrorCancelled;
    case device::USB_TRANSFER_STALLED:
      return kErrorStalled;
    case device::USB_TRANSFER_DISCONNECT:
      return kErrorDisconnect;
    case device::USB_TRANSFER_OVERFLOW:
      return kErrorOverflow;
    case device::USB_TRANSFER_LENGTH_SHORT:
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
  (*i)->RequestUsbAccess(interface_id,
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
  (*i)->RequestUsbAccess(interface_id,
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

TransferType ConvertTransferTypeToApi(const UsbTransferType& input) {
  switch (input) {
    case device::USB_TRANSFER_CONTROL:
      return usb::TRANSFER_TYPE_CONTROL;
    case device::USB_TRANSFER_INTERRUPT:
      return usb::TRANSFER_TYPE_INTERRUPT;
    case device::USB_TRANSFER_ISOCHRONOUS:
      return usb::TRANSFER_TYPE_ISOCHRONOUS;
    case device::USB_TRANSFER_BULK:
      return usb::TRANSFER_TYPE_BULK;
    default:
      NOTREACHED();
      return usb::TRANSFER_TYPE_NONE;
  }
}

Direction ConvertDirectionToApi(const UsbEndpointDirection& input) {
  switch (input) {
    case device::USB_DIRECTION_INBOUND:
      return usb::DIRECTION_IN;
    case device::USB_DIRECTION_OUTBOUND:
      return usb::DIRECTION_OUT;
    default:
      NOTREACHED();
      return usb::DIRECTION_NONE;
  }
}

SynchronizationType ConvertSynchronizationTypeToApi(
    const UsbSynchronizationType& input) {
  switch (input) {
    case device::USB_SYNCHRONIZATION_NONE:
      return usb::SYNCHRONIZATION_TYPE_NONE;
    case device::USB_SYNCHRONIZATION_ASYNCHRONOUS:
      return usb::SYNCHRONIZATION_TYPE_ASYNCHRONOUS;
    case device::USB_SYNCHRONIZATION_ADAPTIVE:
      return usb::SYNCHRONIZATION_TYPE_ADAPTIVE;
    case device::USB_SYNCHRONIZATION_SYNCHRONOUS:
      return usb::SYNCHRONIZATION_TYPE_SYNCHRONOUS;
    default:
      NOTREACHED();
      return usb::SYNCHRONIZATION_TYPE_NONE;
  }
}

UsageType ConvertUsageTypeToApi(const UsbUsageType& input) {
  switch (input) {
    case device::USB_USAGE_DATA:
      return usb::USAGE_TYPE_DATA;
    case device::USB_USAGE_FEEDBACK:
      return usb::USAGE_TYPE_FEEDBACK;
    case device::USB_USAGE_EXPLICIT_FEEDBACK:
      return usb::USAGE_TYPE_EXPLICITFEEDBACK;
    default:
      NOTREACHED();
      return usb::USAGE_TYPE_NONE;
  }
}

void ConvertEndpointDescriptor(const UsbEndpointDescriptor& input,
                               EndpointDescriptor* output) {
  output->address = input.address;
  output->type = ConvertTransferTypeToApi(input.transfer_type);
  output->direction = ConvertDirectionToApi(input.direction);
  output->maximum_packet_size = input.maximum_packet_size;
  output->synchronization =
      ConvertSynchronizationTypeToApi(input.synchronization_type);
  output->usage = ConvertUsageTypeToApi(input.usage_type);
  output->polling_interval.reset(new int(input.polling_interval));
  if (input.extra_data.size() > 0) {
    output->extra_data =
        std::string(reinterpret_cast<const char*>(&input.extra_data[0]),
                    input.extra_data.size());
  }
}

void ConvertInterfaceDescriptor(const UsbInterfaceDescriptor& input,
                                InterfaceDescriptor* output) {
  output->interface_number = input.interface_number;
  output->alternate_setting = input.alternate_setting;
  output->interface_class = input.interface_class;
  output->interface_subclass = input.interface_subclass;
  output->interface_protocol = input.interface_protocol;
  for (UsbEndpointDescriptor::Iterator endpointIt = input.endpoints.begin();
       endpointIt != input.endpoints.end();
       ++endpointIt) {
    linked_ptr<EndpointDescriptor> endpoint(new EndpointDescriptor);
    ConvertEndpointDescriptor(*endpointIt, endpoint.get());
    output->endpoints.push_back(endpoint);
  }
  if (input.extra_data.size() > 0) {
    output->extra_data =
        std::string(reinterpret_cast<const char*>(&input.extra_data[0]),
                    input.extra_data.size());
  }
}

void ConvertConfigDescriptor(const UsbConfigDescriptor& input,
                             ConfigDescriptor* output) {
  output->configuration_value = input.configuration_value;
  output->self_powered = input.self_powered;
  output->remote_wakeup = input.remote_wakeup;
  output->max_power = input.maximum_power;
  for (UsbInterfaceDescriptor::Iterator interfaceIt = input.interfaces.begin();
       interfaceIt != input.interfaces.end();
       ++interfaceIt) {
    linked_ptr<InterfaceDescriptor> interface(new InterfaceDescriptor);
    ConvertInterfaceDescriptor(*interfaceIt, interface.get());
    output->interfaces.push_back(interface);
  }
  if (input.extra_data.size() > 0) {
    output->extra_data =
        std::string(reinterpret_cast<const char*>(&input.extra_data[0]),
                    input.extra_data.size());
  }
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

// static
void UsbAsyncApiFunction::CreateDeviceFilter(const usb::DeviceFilter& input,
                                             UsbDeviceFilter* output) {
  if (input.vendor_id) {
    output->SetVendorId(*input.vendor_id);
  }
  if (input.product_id) {
    output->SetProductId(*input.product_id);
  }
  if (input.interface_class) {
    output->SetInterfaceClass(*input.interface_class);
  }
  if (input.interface_subclass) {
    output->SetInterfaceSubclass(*input.interface_subclass);
  }
  if (input.interface_protocol) {
    output->SetInterfaceProtocol(*input.interface_protocol);
  }
}

bool UsbAsyncApiFunction::HasDevicePermission(scoped_refptr<UsbDevice> device) {
  UsbDevicePermission::CheckParam param(
      device->vendor_id(),
      device->product_id(),
      UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
  return extension()->permissions_data()->CheckAPIPermissionWithParam(
      APIPermission::kUsbDevice, &param);
}

scoped_refptr<UsbDevice> UsbAsyncApiFunction::GetDeviceOrCompleteWithError(
    const Device& input_device) {
  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (!service) {
    CompleteWithError(kErrorInitService);
    return NULL;
  }

  scoped_refptr<UsbDevice> device = service->GetDeviceById(input_device.device);
  if (!device.get()) {
    CompleteWithError(kErrorNoDevice);
    return NULL;
  }

  if (!HasDevicePermission(device)) {
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

  if (!resource->device().get() || !resource->device()->GetDevice().get()) {
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
  if (status != device::USB_TRANSFER_COMPLETED)
    SetError(ConvertTransferStatusToApi(status));

  SetResult(CreateTransferInfo(status, data, length));
  AsyncWorkCompleted();
}

bool UsbAsyncApiTransferFunction::ConvertDirectionSafely(
    const Direction& input,
    UsbEndpointDirection* output) {
  const bool converted = ConvertDirectionFromApi(input, output);
  if (!converted)
    SetError(kErrorConvertDirection);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRequestTypeSafely(
    const RequestType& input,
    UsbDeviceHandle::TransferRequestType* output) {
  const bool converted = ConvertRequestTypeFromApi(input, output);
  if (!converted)
    SetError(kErrorConvertRequestType);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRecipientSafely(
    const Recipient& input,
    UsbDeviceHandle::TransferRecipient* output) {
  const bool converted = ConvertRecipientFromApi(input, output);
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
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  UsbService* service = device::DeviceClient::Get()->GetUsbService();
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
    if (device_handle.get())
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
  std::vector<UsbDeviceFilter> filters;
  if (parameters_->options.filters) {
    filters.resize(parameters_->options.filters->size());
    for (size_t i = 0; i < parameters_->options.filters->size(); ++i) {
      CreateDeviceFilter(*parameters_->options.filters->at(i).get(),
                         &filters[i]);
    }
  }
  if (parameters_->options.vendor_id) {
    filters.resize(filters.size() + 1);
    filters.back().SetVendorId(*parameters_->options.vendor_id);
    if (parameters_->options.product_id) {
      filters.back().SetProductId(*parameters_->options.product_id);
    }
  }

  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (!service) {
    CompleteWithError(kErrorInitService);
    return;
  }

  DeviceVector devices;
  service->GetDevices(&devices);

  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (DeviceVector::iterator it = devices.begin(); it != devices.end(); ++it) {
    scoped_refptr<UsbDevice> device = *it;
    if ((filters.empty() || UsbDeviceFilter::MatchesAny(device, filters)) &&
        HasDevicePermission(device)) {
      result->Append(PopulateDevice(it->get()));
    }
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
      GetDeviceOrCompleteWithError(parameters_->device);
  if (!device.get())
    return;

  device->RequestUsbAccess(
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
      GetDeviceOrCompleteWithError(parameters_->device);
  if (!device.get())
    return;

  handle_ = device->Open();
  if (!handle_.get()) {
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

UsbGetConfigurationFunction::UsbGetConfigurationFunction() {
}

UsbGetConfigurationFunction::~UsbGetConfigurationFunction() {
}

bool UsbGetConfigurationFunction::Prepare() {
  parameters_ = GetConfiguration::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbGetConfigurationFunction::AsyncWorkStart() {
  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandleOrCompleteWithError(parameters_->handle);
  if (!device_handle.get()) {
    return;
  }

  ConfigDescriptor config;
  ConvertConfigDescriptor(device_handle->GetDevice()->GetConfiguration(),
                          &config);

  SetResult(config.ToValue().release());
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
  if (!device_handle.get()) {
    return;
  }

  ConfigDescriptor config;
  ConvertConfigDescriptor(device_handle->GetDevice()->GetConfiguration(),
                          &config);

  scoped_ptr<base::ListValue> result(new base::ListValue);
  for (size_t i = 0; i < config.interfaces.size(); ++i) {
    result->Append(config.interfaces[i]->ToValue().release());
  }

  SetResult(result.release());
  AsyncWorkCompleted();
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
  if (!device_handle.get())
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
  if (!device_handle.get())
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
  if (!device_handle.get())
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
  if (!device_handle.get())
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
  if (!device_handle.get())
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
  if (!device_handle.get())
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
  if (!device_handle.get())
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
  if (!device_handle.get())
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
  if (!device_handle.get())
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
