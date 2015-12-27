// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/devices_app/usb/type_converters.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "device/usb/usb_device.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/public/cpp/bindings/array.h"

namespace mojo {

// static
device::UsbDeviceFilter
TypeConverter<device::UsbDeviceFilter, device::usb::DeviceFilterPtr>::Convert(
    const device::usb::DeviceFilterPtr& mojo_filter) {
  device::UsbDeviceFilter filter;
  if (mojo_filter->has_vendor_id)
    filter.SetVendorId(mojo_filter->vendor_id);
  if (mojo_filter->has_product_id)
    filter.SetProductId(mojo_filter->product_id);
  if (mojo_filter->has_class_code)
    filter.SetInterfaceClass(mojo_filter->class_code);
  if (mojo_filter->has_subclass_code)
    filter.SetInterfaceSubclass(mojo_filter->subclass_code);
  if (mojo_filter->has_protocol_code)
    filter.SetInterfaceProtocol(mojo_filter->protocol_code);
  return filter;
}

// static
device::usb::TransferDirection
TypeConverter<device::usb::TransferDirection, device::UsbEndpointDirection>::
    Convert(const device::UsbEndpointDirection& direction) {
  if (direction == device::USB_DIRECTION_INBOUND)
    return device::usb::TRANSFER_DIRECTION_IN;
  DCHECK(direction == device::USB_DIRECTION_OUTBOUND);
  return device::usb::TRANSFER_DIRECTION_OUT;
}

// static
device::usb::TransferStatus
TypeConverter<device::usb::TransferStatus, device::UsbTransferStatus>::Convert(
    const device::UsbTransferStatus& status) {
  switch (status) {
    case device::USB_TRANSFER_COMPLETED:
      return device::usb::TRANSFER_STATUS_COMPLETED;
    case device::USB_TRANSFER_ERROR:
      return device::usb::TRANSFER_STATUS_ERROR;
    case device::USB_TRANSFER_TIMEOUT:
      return device::usb::TRANSFER_STATUS_TIMEOUT;
    case device::USB_TRANSFER_CANCELLED:
      return device::usb::TRANSFER_STATUS_CANCELLED;
    case device::USB_TRANSFER_STALLED:
      return device::usb::TRANSFER_STATUS_STALLED;
    case device::USB_TRANSFER_DISCONNECT:
      return device::usb::TRANSFER_STATUS_DISCONNECT;
    case device::USB_TRANSFER_OVERFLOW:
      return device::usb::TRANSFER_STATUS_BABBLE;
    case device::USB_TRANSFER_LENGTH_SHORT:
      return device::usb::TRANSFER_STATUS_SHORT_PACKET;
    default:
      NOTREACHED();
      return device::usb::TRANSFER_STATUS_ERROR;
  }
}

// static
device::UsbDeviceHandle::TransferRequestType
TypeConverter<device::UsbDeviceHandle::TransferRequestType,
              device::usb::ControlTransferType>::
    Convert(const device::usb::ControlTransferType& type) {
  switch (type) {
    case device::usb::CONTROL_TRANSFER_TYPE_STANDARD:
      return device::UsbDeviceHandle::STANDARD;
    case device::usb::CONTROL_TRANSFER_TYPE_CLASS:
      return device::UsbDeviceHandle::CLASS;
    case device::usb::CONTROL_TRANSFER_TYPE_VENDOR:
      return device::UsbDeviceHandle::VENDOR;
    case device::usb::CONTROL_TRANSFER_TYPE_RESERVED:
      return device::UsbDeviceHandle::RESERVED;
    default:
      NOTREACHED();
      return device::UsbDeviceHandle::RESERVED;
  }
}

// static
device::UsbDeviceHandle::TransferRecipient
TypeConverter<device::UsbDeviceHandle::TransferRecipient,
              device::usb::ControlTransferRecipient>::
    Convert(const device::usb::ControlTransferRecipient& recipient) {
  switch (recipient) {
    case device::usb::CONTROL_TRANSFER_RECIPIENT_DEVICE:
      return device::UsbDeviceHandle::DEVICE;
    case device::usb::CONTROL_TRANSFER_RECIPIENT_INTERFACE:
      return device::UsbDeviceHandle::INTERFACE;
    case device::usb::CONTROL_TRANSFER_RECIPIENT_ENDPOINT:
      return device::UsbDeviceHandle::ENDPOINT;
    case device::usb::CONTROL_TRANSFER_RECIPIENT_OTHER:
      return device::UsbDeviceHandle::OTHER;
    default:
      NOTREACHED();
      return device::UsbDeviceHandle::OTHER;
  }
}

// static
device::usb::EndpointType
TypeConverter<device::usb::EndpointType, device::UsbTransferType>::Convert(
    const device::UsbTransferType& type) {
  switch (type) {
    case device::USB_TRANSFER_ISOCHRONOUS:
      return device::usb::ENDPOINT_TYPE_ISOCHRONOUS;
    case device::USB_TRANSFER_BULK:
      return device::usb::ENDPOINT_TYPE_BULK;
    case device::USB_TRANSFER_INTERRUPT:
      return device::usb::ENDPOINT_TYPE_INTERRUPT;
    // Note that we do not expose control transfer in the public interface
    // because control endpoints are implied rather than explicitly enumerated
    // there.
    default:
      NOTREACHED();
      return device::usb::ENDPOINT_TYPE_BULK;
  }
}

// static
device::usb::EndpointInfoPtr
TypeConverter<device::usb::EndpointInfoPtr, device::UsbEndpointDescriptor>::
    Convert(const device::UsbEndpointDescriptor& endpoint) {
  device::usb::EndpointInfoPtr info = device::usb::EndpointInfo::New();
  info->endpoint_number = endpoint.address;
  info->direction =
      ConvertTo<device::usb::TransferDirection>(endpoint.direction);
  info->type = ConvertTo<device::usb::EndpointType>(endpoint.transfer_type);
  info->packet_size = static_cast<uint32_t>(endpoint.maximum_packet_size);
  return info;
}

// static
device::usb::AlternateInterfaceInfoPtr
TypeConverter<device::usb::AlternateInterfaceInfoPtr,
              device::UsbInterfaceDescriptor>::
    Convert(const device::UsbInterfaceDescriptor& interface) {
  device::usb::AlternateInterfaceInfoPtr info =
      device::usb::AlternateInterfaceInfo::New();
  info->alternate_setting = interface.alternate_setting;
  info->class_code = interface.interface_class;
  info->subclass_code = interface.interface_subclass;
  info->protocol_code = interface.interface_protocol;

  // Filter out control endpoints for the public interface.
  info->endpoints = mojo::Array<device::usb::EndpointInfoPtr>::New(0);
  for (const auto& endpoint : interface.endpoints) {
    if (endpoint.transfer_type != device::USB_TRANSFER_CONTROL)
      info->endpoints.push_back(device::usb::EndpointInfo::From(endpoint));
  }

  return info;
}

// static
mojo::Array<device::usb::InterfaceInfoPtr>
TypeConverter<mojo::Array<device::usb::InterfaceInfoPtr>,
              std::vector<device::UsbInterfaceDescriptor>>::
    Convert(const std::vector<device::UsbInterfaceDescriptor>& interfaces) {
  auto infos = mojo::Array<device::usb::InterfaceInfoPtr>::New(0);

  // Aggregate each alternate setting into an InterfaceInfo corresponding to its
  // interface number.
  std::map<uint8_t, device::usb::InterfaceInfo*> interface_map;
  for (size_t i = 0; i < interfaces.size(); ++i) {
    auto alternate = device::usb::AlternateInterfaceInfo::From(interfaces[i]);
    auto iter = interface_map.find(interfaces[i].interface_number);
    if (iter == interface_map.end()) {
      // This is the first time we're seeing an alternate with this interface
      // number, so add a new InterfaceInfo to the array and map the number.
      auto info = device::usb::InterfaceInfo::New();
      iter = interface_map.insert(std::make_pair(interfaces[i].interface_number,
                                                 info.get())).first;
      infos.push_back(std::move(info));
    }
    iter->second->alternates.push_back(std::move(alternate));
  }

  return infos;
}

// static
device::usb::ConfigurationInfoPtr
TypeConverter<device::usb::ConfigurationInfoPtr, device::UsbConfigDescriptor>::
    Convert(const device::UsbConfigDescriptor& config) {
  device::usb::ConfigurationInfoPtr info =
      device::usb::ConfigurationInfo::New();
  info->configuration_value = config.configuration_value;
  info->interfaces =
      mojo::Array<device::usb::InterfaceInfoPtr>::From(config.interfaces);
  return info;
}

// static
device::usb::WebUsbFunctionSubsetPtr TypeConverter<
    device::usb::WebUsbFunctionSubsetPtr,
    device::WebUsbFunctionSubset>::Convert(const device::WebUsbFunctionSubset&
                                               function) {
  device::usb::WebUsbFunctionSubsetPtr info =
      device::usb::WebUsbFunctionSubset::New();
  info->first_interface = function.first_interface;
  info->origins = mojo::Array<mojo::String>::From(function.origins);
  return info;
}

// static
device::usb::WebUsbConfigurationSubsetPtr
TypeConverter<device::usb::WebUsbConfigurationSubsetPtr,
              device::WebUsbConfigurationSubset>::
    Convert(const device::WebUsbConfigurationSubset& config) {
  device::usb::WebUsbConfigurationSubsetPtr info =
      device::usb::WebUsbConfigurationSubset::New();
  info->configuration_value = config.configuration_value;
  info->origins = mojo::Array<mojo::String>::From(config.origins);
  info->functions =
      mojo::Array<device::usb::WebUsbFunctionSubsetPtr>::From(config.functions);
  return info;
}

// static
device::usb::WebUsbDescriptorSetPtr TypeConverter<
    device::usb::WebUsbDescriptorSetPtr,
    device::WebUsbDescriptorSet>::Convert(const device::WebUsbDescriptorSet&
                                              set) {
  device::usb::WebUsbDescriptorSetPtr info =
      device::usb::WebUsbDescriptorSet::New();
  info->origins = mojo::Array<mojo::String>::From(set.origins);
  info->configurations =
      mojo::Array<device::usb::WebUsbConfigurationSubsetPtr>::From(
          set.configurations);
  return info;
}

// static
device::usb::DeviceInfoPtr
TypeConverter<device::usb::DeviceInfoPtr, device::UsbDevice>::Convert(
    const device::UsbDevice& device) {
  device::usb::DeviceInfoPtr info = device::usb::DeviceInfo::New();
  info->guid = device.guid();
  info->vendor_id = device.vendor_id();
  info->product_id = device.product_id();
  info->manufacturer_name = base::UTF16ToUTF8(device.manufacturer_string());
  info->product_name = base::UTF16ToUTF8(device.product_string());
  info->serial_number = base::UTF16ToUTF8(device.serial_number());
  info->configurations = mojo::Array<device::usb::ConfigurationInfoPtr>::From(
      device.configurations());
  if (device.webusb_allowed_origins()) {
    info->webusb_allowed_origins = device::usb::WebUsbDescriptorSet::From(
        *device.webusb_allowed_origins());
  }
  return info;
}

}  // namespace mojo
