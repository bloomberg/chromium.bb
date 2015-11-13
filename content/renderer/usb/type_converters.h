// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/type_converter.h"

#include "device/devices_app/usb/public/interfaces/device.mojom.h"
#include "device/devices_app/usb/public/interfaces/device_manager.mojom.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDevice.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceFilter.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceInfo.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceRequestOptions.h"

namespace mojo {

template <>
struct TypeConverter<blink::WebUSBDevice::TransferDirection,
                     device::usb::TransferDirection> {
  static blink::WebUSBDevice::TransferDirection Convert(
      const device::usb::TransferDirection& direction);
};

template <>
struct TypeConverter<device::usb::TransferDirection,
                     blink::WebUSBDevice::TransferDirection> {
  static device::usb::TransferDirection Convert(
      const blink::WebUSBDevice::TransferDirection& direction);
};

template <>
struct TypeConverter<device::usb::ControlTransferType,
                     blink::WebUSBDevice::RequestType> {
  static device::usb::ControlTransferType Convert(
      const blink::WebUSBDevice::RequestType& direction);
};

template <>
struct TypeConverter<device::usb::ControlTransferRecipient,
                     blink::WebUSBDevice::RequestRecipient> {
  static device::usb::ControlTransferRecipient Convert(
      const blink::WebUSBDevice::RequestRecipient& direction);
};

template <>
struct TypeConverter<device::usb::ControlTransferParamsPtr,
                     blink::WebUSBDevice::ControlTransferParameters> {
  static device::usb::ControlTransferParamsPtr Convert(
      const blink::WebUSBDevice::ControlTransferParameters& parameters);
};

template <>
struct TypeConverter<blink::WebUSBDeviceInfo::Endpoint::Type,
                     device::usb::EndpointType> {
  static blink::WebUSBDeviceInfo::Endpoint::Type Convert(
      const device::usb::EndpointType& endpoint_type);
};

template <>
struct TypeConverter<blink::WebUSBDeviceInfo::Endpoint,
                     device::usb::EndpointInfoPtr> {
  static blink::WebUSBDeviceInfo::Endpoint Convert(
      const device::usb::EndpointInfoPtr& info);
};

template <>
struct TypeConverter<blink::WebUSBDeviceInfo::AlternateInterface,
                     device::usb::AlternateInterfaceInfoPtr> {
  static blink::WebUSBDeviceInfo::AlternateInterface Convert(
      const device::usb::AlternateInterfaceInfoPtr& info);
};

template <>
struct TypeConverter<blink::WebUSBDeviceInfo::Interface,
                     device::usb::InterfaceInfoPtr> {
  static blink::WebUSBDeviceInfo::Interface Convert(
      const device::usb::InterfaceInfoPtr& info);
};

template <>
struct TypeConverter<blink::WebUSBDeviceInfo::Configuration,
                     device::usb::ConfigurationInfoPtr> {
  static blink::WebUSBDeviceInfo::Configuration Convert(
      const device::usb::ConfigurationInfoPtr& info);
};

template <>
struct TypeConverter<blink::WebUSBDeviceInfo, device::usb::DeviceInfoPtr> {
  static blink::WebUSBDeviceInfo Convert(
      const device::usb::DeviceInfoPtr& info);
};

template <>
struct TypeConverter<device::usb::DeviceFilterPtr, blink::WebUSBDeviceFilter> {
  static device::usb::DeviceFilterPtr Convert(
      const blink::WebUSBDeviceFilter& web_filter);
};

template <>
struct TypeConverter<device::usb::EnumerationOptionsPtr,
                     blink::WebUSBDeviceRequestOptions> {
  static device::usb::EnumerationOptionsPtr Convert(
      const blink::WebUSBDeviceRequestOptions& web_options);
};

}  // namespace mojo
