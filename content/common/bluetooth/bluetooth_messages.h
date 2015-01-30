// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages for Web Bluetooth API.
// Multiply-included message file, hence no include guard.

// Web Bluetooth Security
// The security mechanisms of Bluetooth are described in the specification:
// https://webbluetoothcg.github.io/web-bluetooth
//
// Exerpts:
//
// From: Security and privacy considerations
// http://webbluetoothcg.github.io/web-bluetooth/#security-and-privacy-considerations
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
// From: Device Discovery:
// """
// The UA must maintain an allowed devices list for each origin, storing a set
// of Bluetooth devices the origin is allowed to access. For each device in the
// allowed devices list for an origin, the UA must maintain an allowed services
// list consisting of UUIDs for GATT Primary Services the origin is allowed to
// access on the device. The UA may remove devices from the allowed devices list
// at any time based on signals from the user. For example, if the user chooses
// not to remember access, the UA might remove a device when the tab that was
// granted access to it is closed. Or the UA might provide a revocation UI that
// allows the user to explicitly remove a device even while a tab is actively
// using that device. If a device is removed from this list while a Promise is
// pending to do something with the device, it must be treated the same as if
// the device moved out of Bluetooth range.
// """
//
// From: Device Discovery: requestDevice
// http://webbluetoothcg.github.io/web-bluetooth/#device-discovery
// """
// Display a prompt to the user requesting that the user specify some devices
// from the result of the scan. The UA should show the user the human-readable
// name of each device. If this name is not available because the UA's Bluetooth
// system doesn't support privacy-enabled scans, the UA should allow the user to
// indicate interest and then perform a privacy-disabled scan to retrieve the
// name.
//
// The UA may allow the user to select a nearby device that does not match
// filters.
//
// Wait for the user to have made their selection.
//
// If the user cancels the prompt, reject the Promise with a NotFoundError and
// abort these steps.
//
// Record the selected device in the origin's allowed devices list and the union
// of the service UUIDs from filters and options.optionalServices in the device
// and origin's allowed services list.
//
// Connect to the device. ([BLUETOOTH41] 3.G.6.2.1) If the connection fails,
// reject the Promise with a NetworkError and abort these steps.
//
// Resolve the Promise with a BluetoothDevice instance representing the selected
// device.
// """

#include "ipc/ipc_message_macros.h"
#include "content/common/bluetooth/bluetooth_device.h"
#include "content/common/bluetooth/bluetooth_error.h"

#define IPC_MESSAGE_START BluetoothMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(
    device::BluetoothDevice::VendorIDSource,
    device::BluetoothDevice::VendorIDSource::VENDOR_ID_MAX_VALUE)

IPC_STRUCT_TRAITS_BEGIN(content::BluetoothDevice)
IPC_STRUCT_TRAITS_MEMBER(instance_id)
IPC_STRUCT_TRAITS_MEMBER(name)
IPC_STRUCT_TRAITS_MEMBER(device_class)
IPC_STRUCT_TRAITS_MEMBER(vendor_id_source)
IPC_STRUCT_TRAITS_MEMBER(vendor_id)
IPC_STRUCT_TRAITS_MEMBER(product_id)
IPC_STRUCT_TRAITS_MEMBER(product_version)
IPC_STRUCT_TRAITS_MEMBER(paired)
IPC_STRUCT_TRAITS_MEMBER(connected)
IPC_STRUCT_TRAITS_MEMBER(uuids)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(content::BluetoothError,
                          content::BluetoothError::ENUM_MAX_VALUE)

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
                     content::BluetoothError /* result */)

// Messages sent from the renderer to the browser.

// Requests a bluetooth device from the browser.
// TODO(scheib): UI to select and permit access to a device crbug.com/436280.
//   This will include refactoring messages to be associated with an origin
//   and making this initial requestDevice call with an associated frame.
//   This work is deferred to simplify initial prototype patches.
//   The Bluetooth feature, and the BluetoothDispatcherHost are behind
//   the --enable-experimental-web-platform-features flag.
IPC_MESSAGE_CONTROL2(BluetoothHostMsg_RequestDevice,
                     int /* thread_id */,
                     int /* request_id */)

// Configures the mock data set in the browser used while under test.
// TODO(scheib): Disable testing in non-test executables. crbug.com/436284.
IPC_MESSAGE_CONTROL1(BluetoothHostMsg_SetBluetoothMockDataSetForTesting,
                     std::string /* name */)
