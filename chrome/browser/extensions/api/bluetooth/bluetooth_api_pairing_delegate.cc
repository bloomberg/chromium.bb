// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_pairing_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"
#include "chrome/common/extensions/api/bluetooth_private.h"
#include "content/public/browser/browser_context.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

namespace bt_private = api::bluetooth_private;

namespace {

void PopulatePairingEvent(const device::BluetoothDevice* device,
                          bt_private::PairingEventType type,
                          bt_private::PairingEvent* out) {
  api::bluetooth::BluetoothDeviceToApiDevice(*device, &out->device);
  out->pairing = type;
}

}  // namespace

BluetoothApiPairingDelegate::BluetoothApiPairingDelegate(
    const std::string& extension_id,
    content::BrowserContext* browser_context)
    : extension_id_(extension_id), browser_context_(browser_context) {}

BluetoothApiPairingDelegate::~BluetoothApiPairingDelegate() {}

void BluetoothApiPairingDelegate::RequestPinCode(
    device::BluetoothDevice* device) {
  bt_private::PairingEvent event;
  PopulatePairingEvent(
      device, bt_private::PAIRING_EVENT_TYPE_REQUESTPINCODE, &event);
  DispatchPairingEvent(event);
}

void BluetoothApiPairingDelegate::RequestPasskey(
    device::BluetoothDevice* device) {
  bt_private::PairingEvent event;
  PopulatePairingEvent(
      device, bt_private::PAIRING_EVENT_TYPE_REQUESTPASSKEY, &event);
  DispatchPairingEvent(event);
}

void BluetoothApiPairingDelegate::DisplayPinCode(
    device::BluetoothDevice* device,
    const std::string& pincode) {
  bt_private::PairingEvent event;
  PopulatePairingEvent(
      device, bt_private::PAIRING_EVENT_TYPE_DISPLAYPINCODE, &event);
  event.pincode.reset(new std::string(pincode));
  DispatchPairingEvent(event);
}

void BluetoothApiPairingDelegate::DisplayPasskey(
    device::BluetoothDevice* device,
    uint32 passkey) {
  bt_private::PairingEvent event;
  PopulatePairingEvent(
      device, bt_private::PAIRING_EVENT_TYPE_DISPLAYPASSKEY, &event);
  event.passkey.reset(new int(passkey));
  DispatchPairingEvent(event);
}

void BluetoothApiPairingDelegate::KeysEntered(device::BluetoothDevice* device,
                                              uint32 entered) {
  bt_private::PairingEvent event;
  PopulatePairingEvent(
      device, bt_private::PAIRING_EVENT_TYPE_KEYSENTERED, &event);
  event.entered_key.reset(new int(entered));
  DispatchPairingEvent(event);
}

void BluetoothApiPairingDelegate::ConfirmPasskey(
    device::BluetoothDevice* device,
    uint32 passkey) {
  bt_private::PairingEvent event;
  PopulatePairingEvent(
      device, bt_private::PAIRING_EVENT_TYPE_CONFIRMPASSKEY, &event);
  event.passkey.reset(new int(passkey));
  DispatchPairingEvent(event);
}

void BluetoothApiPairingDelegate::AuthorizePairing(
    device::BluetoothDevice* device) {
  bt_private::PairingEvent event;
  PopulatePairingEvent(
      device, bt_private::PAIRING_EVENT_TYPE_REQUESTAUTHORIZATION, &event);
  DispatchPairingEvent(event);
}

void BluetoothApiPairingDelegate::DispatchPairingEvent(
    const bt_private::PairingEvent& pairing_event) {
  scoped_ptr<base::ListValue> args =
      bt_private::OnPairing::Create(pairing_event);
  scoped_ptr<Event> event(
      new Event(bt_private::OnPairing::kEventName, args.Pass()));
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id_, event.Pass());
}

}  // namespace extensions
