// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/usb/type_converters.h"

#include <stddef.h>

#include "base/logging.h"

namespace mojo {

// static
blink::WebUSBDevice::TransferDirection
TypeConverter<blink::WebUSBDevice::TransferDirection,
              device::usb::TransferDirection>::
    Convert(const device::usb::TransferDirection& direction) {
  switch (direction) {
    case device::usb::TRANSFER_DIRECTION_IN:
      return blink::WebUSBDevice::TransferDirection::In;
    case device::usb::TRANSFER_DIRECTION_OUT:
      return blink::WebUSBDevice::TransferDirection::Out;
    default:
      NOTREACHED();
      return blink::WebUSBDevice::TransferDirection::In;
  }
}

// static
device::usb::TransferDirection
TypeConverter<device::usb::TransferDirection,
              blink::WebUSBDevice::TransferDirection>::
    Convert(const blink::WebUSBDevice::TransferDirection& direction) {
  switch (direction) {
    case blink::WebUSBDevice::TransferDirection::In:
      return device::usb::TRANSFER_DIRECTION_IN;
    case blink::WebUSBDevice::TransferDirection::Out:
      return device::usb::TRANSFER_DIRECTION_OUT;
    default:
      NOTREACHED();
      return device::usb::TRANSFER_DIRECTION_IN;
  }
}

// static
device::usb::ControlTransferType
TypeConverter<device::usb::ControlTransferType,
              blink::WebUSBDevice::RequestType>::
    Convert(const blink::WebUSBDevice::RequestType& direction) {
  switch (direction) {
    case blink::WebUSBDevice::RequestType::Standard:
      return device::usb::CONTROL_TRANSFER_TYPE_STANDARD;
    case blink::WebUSBDevice::RequestType::Class:
      return device::usb::CONTROL_TRANSFER_TYPE_CLASS;
    case blink::WebUSBDevice::RequestType::Vendor:
      return device::usb::CONTROL_TRANSFER_TYPE_VENDOR;
    default:
      NOTREACHED();
      return device::usb::CONTROL_TRANSFER_TYPE_STANDARD;
  }
}

// static
device::usb::ControlTransferRecipient
TypeConverter<device::usb::ControlTransferRecipient,
              blink::WebUSBDevice::RequestRecipient>::
    Convert(const blink::WebUSBDevice::RequestRecipient& direction) {
  switch (direction) {
    case blink::WebUSBDevice::RequestRecipient::Device:
      return device::usb::CONTROL_TRANSFER_RECIPIENT_DEVICE;
    case blink::WebUSBDevice::RequestRecipient::Interface:
      return device::usb::CONTROL_TRANSFER_RECIPIENT_INTERFACE;
    case blink::WebUSBDevice::RequestRecipient::Endpoint:
      return device::usb::CONTROL_TRANSFER_RECIPIENT_ENDPOINT;
    case blink::WebUSBDevice::RequestRecipient::Other:
      return device::usb::CONTROL_TRANSFER_RECIPIENT_OTHER;
    default:
      NOTREACHED();
      return device::usb::CONTROL_TRANSFER_RECIPIENT_DEVICE;
  }
}

// static
device::usb::ControlTransferParamsPtr
TypeConverter<device::usb::ControlTransferParamsPtr,
              blink::WebUSBDevice::ControlTransferParameters>::
    Convert(const blink::WebUSBDevice::ControlTransferParameters& parameters) {
  device::usb::ControlTransferParamsPtr params =
      device::usb::ControlTransferParams::New();
  params->type =
      mojo::ConvertTo<device::usb::ControlTransferType>(parameters.type);
  params->recipient = mojo::ConvertTo<device::usb::ControlTransferRecipient>(
      parameters.recipient);
  params->request = parameters.request;
  params->value = parameters.value;
  params->index = parameters.index;
  return params;
}

// static
blink::WebUSBDeviceInfo::Endpoint::Type TypeConverter<
    blink::WebUSBDeviceInfo::Endpoint::Type,
    device::usb::EndpointType>::Convert(const device::usb::EndpointType&
                                            endpoint_type) {
  switch (endpoint_type) {
    case device::usb::ENDPOINT_TYPE_BULK:
      return blink::WebUSBDeviceInfo::Endpoint::Type::Bulk;
    case device::usb::ENDPOINT_TYPE_INTERRUPT:
      return blink::WebUSBDeviceInfo::Endpoint::Type::Interrupt;
    case device::usb::ENDPOINT_TYPE_ISOCHRONOUS:
      return blink::WebUSBDeviceInfo::Endpoint::Type::Isochronous;
    default:
      NOTREACHED();
      return blink::WebUSBDeviceInfo::Endpoint::Type::Bulk;
  }
}

// static
blink::WebUSBDeviceInfo::Endpoint
TypeConverter<blink::WebUSBDeviceInfo::Endpoint, device::usb::EndpointInfoPtr>::
    Convert(const device::usb::EndpointInfoPtr& info) {
  blink::WebUSBDeviceInfo::Endpoint endpoint;
  endpoint.endpointNumber = info->endpoint_number;
  endpoint.direction =
      mojo::ConvertTo<blink::WebUSBDevice::TransferDirection>(info->direction);
  endpoint.type =
      mojo::ConvertTo<blink::WebUSBDeviceInfo::Endpoint::Type>(info->type);
  endpoint.packetSize = info->packet_size;
  return endpoint;
}

// static
blink::WebUSBDeviceInfo::AlternateInterface
TypeConverter<blink::WebUSBDeviceInfo::AlternateInterface,
              device::usb::AlternateInterfaceInfoPtr>::
    Convert(const device::usb::AlternateInterfaceInfoPtr& info) {
  blink::WebUSBDeviceInfo::AlternateInterface alternate;
  alternate.alternateSetting = info->alternate_setting;
  alternate.classCode = info->class_code;
  alternate.subclassCode = info->subclass_code;
  alternate.protocolCode = info->protocol_code;
  if (!info->interface_name.is_null())
    alternate.interfaceName = blink::WebString::fromUTF8(info->interface_name);
  alternate.endpoints = blink::WebVector<blink::WebUSBDeviceInfo::Endpoint>(
      info->endpoints.size());
  for (size_t i = 0; i < info->endpoints.size(); ++i) {
    alternate.endpoints[i] =
        mojo::ConvertTo<blink::WebUSBDeviceInfo::Endpoint>(info->endpoints[i]);
  }
  return alternate;
}

// static
blink::WebUSBDeviceInfo::Interface TypeConverter<
    blink::WebUSBDeviceInfo::Interface,
    device::usb::InterfaceInfoPtr>::Convert(const device::usb::InterfaceInfoPtr&
                                                info) {
  blink::WebUSBDeviceInfo::Interface interface;
  interface.interfaceNumber = info->interface_number;
  interface.alternates =
      blink::WebVector<blink::WebUSBDeviceInfo::AlternateInterface>(
          info->alternates.size());
  for (size_t i = 0; i < info->alternates.size(); ++i) {
    interface.alternates[i] =
        mojo::ConvertTo<blink::WebUSBDeviceInfo::AlternateInterface>(
            info->alternates[i]);
  }
  return interface;
}

// static
blink::WebUSBDeviceInfo::Configuration
TypeConverter<blink::WebUSBDeviceInfo::Configuration,
              device::usb::ConfigurationInfoPtr>::
    Convert(const device::usb::ConfigurationInfoPtr& info) {
  blink::WebUSBDeviceInfo::Configuration config;
  config.configurationValue = info->configuration_value;
  if (!info->configuration_name.is_null()) {
    config.configurationName =
        blink::WebString::fromUTF8(info->configuration_name);
  }
  config.interfaces = blink::WebVector<blink::WebUSBDeviceInfo::Interface>(
      info->interfaces.size());
  for (size_t i = 0; i < info->interfaces.size(); ++i) {
    config.interfaces[i] = mojo::ConvertTo<blink::WebUSBDeviceInfo::Interface>(
        info->interfaces[i]);
  }
  return config;
}

// static
blink::WebUSBDeviceInfo
TypeConverter<blink::WebUSBDeviceInfo, device::usb::DeviceInfoPtr>::Convert(
    const device::usb::DeviceInfoPtr& info) {
  blink::WebUSBDeviceInfo device;
  device.guid = blink::WebString::fromUTF8(info->guid);
  device.usbVersionMajor = info->usb_version_major;
  device.usbVersionMinor = info->usb_version_minor;
  device.usbVersionSubminor = info->usb_version_subminor;
  device.deviceClass = info->class_code;
  device.deviceSubclass = info->subclass_code;
  device.deviceProtocol = info->protocol_code;
  device.vendorID = info->vendor_id;
  device.productID = info->product_id;
  device.deviceVersionMajor = info->device_version_major;
  device.deviceVersionMinor = info->device_version_minor;
  device.deviceVersionSubminor = info->device_version_subminor;
  if (!info->manufacturer_name.is_null()) {
    device.manufacturerName =
        blink::WebString::fromUTF8(info->manufacturer_name);
  }
  if (!info->product_name.is_null())
    device.productName = blink::WebString::fromUTF8(info->product_name);
  if (!info->serial_number.is_null())
    device.serialNumber = blink::WebString::fromUTF8(info->serial_number);
  device.configurations =
      blink::WebVector<blink::WebUSBDeviceInfo::Configuration>(
          info->configurations.size());
  for (size_t i = 0; i < info->configurations.size(); ++i) {
    device.configurations[i] =
        mojo::ConvertTo<blink::WebUSBDeviceInfo::Configuration>(
            info->configurations[i]);
  }
  return device;
}

// static
device::usb::DeviceFilterPtr
TypeConverter<device::usb::DeviceFilterPtr, blink::WebUSBDeviceFilter>::Convert(
    const blink::WebUSBDeviceFilter& web_filter) {
  device::usb::DeviceFilterPtr filter = device::usb::DeviceFilter::New();
  filter->has_vendor_id = web_filter.hasVendorID;
  filter->vendor_id = web_filter.vendorID;
  filter->has_product_id = web_filter.hasProductID;
  filter->product_id = web_filter.productID;
  filter->has_class_code = web_filter.hasClassCode;
  filter->class_code = web_filter.classCode;
  filter->has_subclass_code = web_filter.hasSubclassCode;
  filter->subclass_code = web_filter.subclassCode;
  filter->has_protocol_code = web_filter.hasProtocolCode;
  filter->protocol_code = web_filter.protocolCode;
  return filter;
}

// static
device::usb::EnumerationOptionsPtr
TypeConverter<device::usb::EnumerationOptionsPtr,
              blink::WebUSBDeviceRequestOptions>::
    Convert(const blink::WebUSBDeviceRequestOptions& web_options) {
  device::usb::EnumerationOptionsPtr options =
      device::usb::EnumerationOptions::New();
  options->filters = mojo::Array<device::usb::DeviceFilterPtr>::New(
      web_options.filters.size());
  for (size_t i = 0; i < web_options.filters.size(); ++i) {
    options->filters[i] =
        device::usb::DeviceFilter::From(web_options.filters[i]);
  }
  return options;
}

}  // namespace mojo
