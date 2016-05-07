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

IPC_STRUCT_TRAITS_BEGIN(content::BluetoothDevice)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(name)
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
IPC_MESSAGE_CONTROL2(BluetoothMsg_GATTServerConnectSuccess,
                     int /* thread_id */,
                     int /* request_id */)

// Informs the renderer that the connection request |request_id| failed.
IPC_MESSAGE_CONTROL3(BluetoothMsg_GATTServerConnectError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebBluetoothError /* result */)

// Messages sent from the renderer to the browser.

// Requests a bluetooth device from the browser.
IPC_MESSAGE_CONTROL5(BluetoothHostMsg_RequestDevice,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::vector<content::BluetoothScanFilter>,
                     std::vector<device::BluetoothUUID> /* optional_services */)

// Connects to a bluetooth device.
IPC_MESSAGE_CONTROL4(BluetoothHostMsg_GATTServerConnect,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* frame_routing_id */,
                     std::string /* device_id */)

// Disconnect from a device.
IPC_MESSAGE_CONTROL3(BluetoothHostMsg_GATTServerDisconnect,
                     int /* thread_id */,
                     int /* frame_routing_id */,
                     std::string /* device_id */)
