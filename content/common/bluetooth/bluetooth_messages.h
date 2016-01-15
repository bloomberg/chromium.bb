// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages for Web Bluetooth API.
// Multiply-included message file, hence no include guard.

// Web Bluetooth Security
// The security mechanisms of Bluetooth are described in the specification:
// https://webbluetoothchrome.github.io/web-bluetooth
//
// Exerpts:
//
// From: Security and privacy considerations
// http://webbluetoothchrome.github.io/web-bluetooth/#security-and-privacy-considerations
// """
// When a website requests access to devices using requestDevice, it gets the
// ability to access all GATT services mentioned in the call. The UA must inform
// the user what capabilities these services give the website before asking
// which devices to entrust to it. If any services in the list aren't known to
// the UA, the UA must assume they give the site complete control over the
// device and inform the user of this risk. The UA must also allow the user to
// inspect what sites have access to what devices and revoke these pairings.
//
// The UA must not allow the user to pair entire classes of devices with a
// website. It is possible to construct a class of devices for which each
// individual device sends the same Bluetooth-level identifying information. UAs
// are not required to attempt to detect this sort of forgery and may let a user
// pair this pseudo-device with a website.
//
// To help ensure that only the entity the user approved for access actually has
// access, this specification requires that only authenticated environments can
// access Bluetooth devices (requestDevice).
// """
//
// From: Per-origin Bluetooth device properties:
// """
// For each origin, the UA must maintain an allowed devices map, whose keys are
// the Bluetooth devices the origin is allowed to access, and whose values are
// pairs of a DOMString device id and an allowed services list consisting of
// UUIDs for GATT Primary Services the origin is allowed to access on the
// device.
//
// The UA may remove devices from the allowed devices map at any time based on
// signals from the user. This needs a definition involving removing
// BluetoothDevice instances from device instance maps and clearing out their
// [[representedDevice]] fields. For example, if the user chooses not to
// remember access, the UA might remove a device when the tab that was granted
// access to it is closed. Or the UA might provide a revocation UI that allows
// the user to explicitly remove a device even while a tab is actively using
// that device. If a device is removed from this list while a Promise is pending
// to do something with the device, it must be treated the same as if the device
// moved out of Bluetooth range.
// """
//
// From: Device Discovery: requestDevice
// http://webbluetoothchrome.github.io/web-bluetooth/#device-discovery
// """
// Even if scanResult is empty, display a prompt to the user requesting that the
// user select a device from it. The UA should show the user the human-readable
// name of each device. If this name is not available because the UA's Bluetooth
// system doesn't support privacy-enabled scans, the UA should allow the user to
// indicate interest and then perform a privacy-disabled scan to retrieve the
// name.
//
// The UA may allow the user to select a nearby device that does not match
// filters.
//
// Wait for the user to have selected a device or cancelled the prompt.
//
// If the user cancels the prompt, reject promise with a NotFoundError and abort
// these steps.
//
// Add device to the origin's allowed devices map. with the union of the service
// UUIDs from filters and options.optionalServices as allowed services.
//
// Get the BluetoothDevice representing device and resolve promise with the
// result.
// """

#include "ipc/ipc_message_macros.h"

#include <stdint.h>

#include "content/common/bluetooth/bluetooth_device.h"
#include "content/common/bluetooth/bluetooth_scan_filter.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothError.h"

#define IPC_MESSAGE_START BluetoothMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(
    device::BluetoothDevice::VendorIDSource,
    device::BluetoothDevice::VendorIDSource::VENDOR_ID_MAX_VALUE)

IPC_STRUCT_TRAITS_BEGIN(content::BluetoothDevice)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(tx_power)
  IPC_STRUCT_TRAITS_MEMBER(rssi)
  IPC_STRUCT_TRAITS_MEMBER(device_class)
  IPC_STRUCT_TRAITS_MEMBER(vendor_id_source)
  IPC_STRUCT_TRAITS_MEMBER(vendor_id)
  IPC_STRUCT_TRAITS_MEMBER(product_id)
  IPC_STRUCT_TRAITS_MEMBER(product_version)
  IPC_STRUCT_TRAITS_MEMBER(uuids)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebBluetoothError,
                          blink::WebBluetoothError::ENUM_MAX_VALUE)

IPC_STRUCT_TRAITS_BEGIN(content::BluetoothScanFilter)
IPC_STRUCT_TRAITS_MEMBER(services)
IPC_STRUCT_TRAITS_MEMBER(name)
IPC_STRUCT_TRAITS_MEMBER(namePrefix)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

// Informs the renderer that the device request |request_id| succeeded.
IPC_MESSAGE_CONTROL3(BluetoothMsg_RequestDeviceSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     content::BluetoothDevice /* device */)

// Informs the renderer that the device request |request_id| failed.
IPC_MESSAGE_CONTROL3(BluetoothMsg_RequestDeviceError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebBluetoothError /* result */)

// Informs the renderer that the connection request |request_id| succeeded.
IPC_MESSAGE_CONTROL3(BluetoothMsg_ConnectGATTSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     std::string /* device_id */)

// Informs the renderer that the connection request |request_id| failed.
IPC_MESSAGE_CONTROL3(BluetoothMsg_ConnectGATTError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebBluetoothError /* result */)

// Informs the renderer that primary service request |request_id| succeeded.
IPC_MESSAGE_CONTROL3(BluetoothMsg_GetPrimaryServiceSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     std::string /* service_instance_id */)

// Informs the renderer that the primary service request |request_id| failed.
IPC_MESSAGE_CONTROL3(BluetoothMsg_GetPrimaryServiceError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebBluetoothError /* result */)

// Informs the renderer that characteristic request |request_id| succeeded.
IPC_MESSAGE_CONTROL4(BluetoothMsg_GetCharacteristicSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     std::string /* characteristic_instance_id */,
                     uint32_t /* characteristic_properties */)

// Informs the renderer that the characteristic request |request_id| failed.
IPC_MESSAGE_CONTROL3(BluetoothMsg_GetCharacteristicError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebBluetoothError /* result */)

// Informs the renderer that the value has been read.
IPC_MESSAGE_CONTROL3(BluetoothMsg_ReadCharacteristicValueSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     std::vector<uint8_t> /* value */)

// Informs the renderer that an error occurred while reading the value.
IPC_MESSAGE_CONTROL3(BluetoothMsg_ReadCharacteristicValueError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebBluetoothError /* result */)

// Informs the renderer that the value has been successfully written to
// the characteristic.
IPC_MESSAGE_CONTROL2(BluetoothMsg_WriteCharacteristicValueSuccess,
                     int /* thread_id */,
                     int /* request_id */)

// Informs the renderer that an error occurred while writing a value to a
// characteristic.
IPC_MESSAGE_CONTROL3(BluetoothMsg_WriteCharacteristicValueError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebBluetoothError /* result */)

// Informs the renderer that the user has successfully subscribed to
// notifications from the device.
IPC_MESSAGE_CONTROL2(BluetoothMsg_StartNotificationsSuccess,
                     int /* thread_id */,
                     int /* request_id */)

// Informs the renderer that an error ocurred when trying to subscribe to
// notifications from the device.
IPC_MESSAGE_CONTROL3(BluetoothMsg_StartNotificationsError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebBluetoothError)

// Informs the renderer that the user has successfully unsubscribed from
// notifications.
IPC_MESSAGE_CONTROL2(BluetoothMsg_StopNotificationsSuccess,
                     int /* thread_id */,
                     int /* request_id */)

// Informs the renderer that a characteristic's value changed.
IPC_MESSAGE_CONTROL3(BluetoothMsg_CharacteristicValueChanged,
                     int /* thread_id */,
                     std::string /* characteristic_instance_id */,
                     std::vector<uint8_t> /* value */)

// Messages sent from the renderer to the browser.

// Requests a bluetooth device from the browser.
IPC_MESSAGE_CONTROL5(BluetoothHostMsg_RequestDevice,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::vector<content::BluetoothScanFilter>,
                     std::vector<device::BluetoothUUID> /* optional_services */)

// Connects to a bluetooth device.
IPC_MESSAGE_CONTROL4(BluetoothHostMsg_ConnectGATT,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::string /* device_id */)

// Gets primary service from bluetooth device.
IPC_MESSAGE_CONTROL5(BluetoothHostMsg_GetPrimaryService,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::string /* device_id */,
                     std::string /* service_uuid */)

// Gets a GATT Characteristic within a GATT Service.
IPC_MESSAGE_CONTROL5(BluetoothHostMsg_GetCharacteristic,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::string /* service_instance_id */,
                     std::string /* characteristic_uuid */)

// Reads the characteristics value from a bluetooth device.
IPC_MESSAGE_CONTROL4(BluetoothHostMsg_ReadValue,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::string /* characteristic_instance_id */)

// Writes a value to a bluetooth device's characteristic.
IPC_MESSAGE_CONTROL5(BluetoothHostMsg_WriteValue,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::string /* characteristic_instance_id */,
                     std::vector<uint8_t> /* value */)

// Subscribes to notifications from a device's characteristic.
IPC_MESSAGE_CONTROL4(BluetoothHostMsg_StartNotifications,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::string /* characteristic_instance_id */)

// Unsubscribes from notifications from a device's characteristic.
IPC_MESSAGE_CONTROL4(BluetoothHostMsg_StopNotifications,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::string /* characteristic_instance_id */)

// Register to receive characteristic value changed events.
IPC_MESSAGE_CONTROL3(BluetoothHostMsg_RegisterCharacteristic,
                     int /* thread_id */,
                     int /* frame_routing_id */,
                     std::string /* characteristics_instance_id */)

// Unregister from characteristic value changed events.
IPC_MESSAGE_CONTROL3(BluetoothHostMsg_UnregisterCharacteristic,
                     int /* thread_id */,
                     int /* frame_routing_id */,
                     std::string /* characteristics_instance_id */)
