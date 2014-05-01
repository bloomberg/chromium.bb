// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_private_api.h"

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/common/extensions/api/bluetooth_private.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace bt_private = extensions::api::bluetooth_private;

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<BluetoothPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>*
BluetoothPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

BluetoothPrivateAPI::BluetoothPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter::Get(browser_context_)
      ->RegisterObserver(this, bt_private::OnPairing::kEventName);
}

BluetoothPrivateAPI::~BluetoothPrivateAPI() {}

void BluetoothPrivateAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void BluetoothPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  // This function can be called multiple times for the same JS listener, for
  // example, once for the addListener call and again if it is a lazy listener.
  if (!details.browser_context)
    return;

  BluetoothAPI::Get(browser_context_)->event_router()->AddPairingDelegate(
      details.extension_id);
}

void BluetoothPrivateAPI::OnListenerRemoved(const EventListenerInfo& details) {
  // This function can be called multiple times for the same JS listener, for
  // example, once for the addListener call and again if it is a lazy listener.
  if (!details.browser_context)
    return;

  BluetoothAPI::Get(browser_context_)->event_router()->RemovePairingDelegate(
      details.extension_id);
}

namespace api {

namespace {

const char kNameProperty[] = "name";
const char kPoweredProperty[] = "powered";
const char kDiscoverableProperty[] = "discoverable";

const char kSetAdapterPropertyError[] = "Error setting adapter properties: $1";

const char kDeviceNotFoundError[] =
    "Given address is not a valid Bluetooth device.";

const char kPairingNotEnabled[] =
    "Pairing must be enabled to set a pairing response.";

const char kInvalidPairingResponseOptions[] =
    "Invalid pairing response options";

const char kAdapterNotPresent[] =
    "Could not find a Bluetooth adapter.";

// Returns true if the pairing response options passed into the
// setPairingResponse function are valid.
bool ValidatePairingResponseOptions(
    const device::BluetoothDevice* device,
    const bt_private::SetPairingResponseOptions& options) {
  bool response = options.response != bt_private::PAIRING_RESPONSE_NONE;
  bool pincode = options.pincode.get() != NULL;
  bool passkey = options.passkey.get() != NULL;

  if (!response && !pincode && !passkey)
    return false;
  if (pincode && passkey)
    return false;
  if (options.response != bt_private::PAIRING_RESPONSE_CONFIRM &&
      (pincode || passkey))
    return false;

  // Check the BluetoothDevice is in expecting the correct response.
  if (!device->ExpectingConfirmation() && !device->ExpectingPinCode() &&
      !device->ExpectingPasskey())
    return false;
  if (pincode && !device->ExpectingPinCode())
    return false;
  if (passkey && !device->ExpectingPasskey())
    return false;
  if (options.response == bt_private::PAIRING_RESPONSE_CONFIRM && !pincode &&
      !passkey && !device->ExpectingConfirmation())
    return false;

  return true;
}

}  // namespace

BluetoothPrivateSetAdapterStateFunction::
    BluetoothPrivateSetAdapterStateFunction() {}

BluetoothPrivateSetAdapterStateFunction::
    ~BluetoothPrivateSetAdapterStateFunction() {}

bool BluetoothPrivateSetAdapterStateFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  scoped_ptr<bt_private::SetAdapterState::Params> params(
      bt_private::SetAdapterState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!adapter->IsPresent()) {
    SetError(kAdapterNotPresent);
    SendResponse(false);
    return true;
  }

  const bt_private::NewAdapterState& new_state = params->adapter_state;

  // These properties are not owned.
  std::string* name = new_state.name.get();
  bool* powered = new_state.powered.get();
  bool* discoverable = new_state.discoverable.get();

  if (name && adapter->GetName() != *name) {
    pending_properties_.insert(kNameProperty);
    adapter->SetName(*name,
                     CreatePropertySetCallback(kNameProperty),
                     CreatePropertyErrorCallback(kNameProperty));
  }

  if (powered && adapter->IsPowered() != *powered) {
    pending_properties_.insert(kPoweredProperty);
    adapter->SetPowered(*powered,
                        CreatePropertySetCallback(kPoweredProperty),
                        CreatePropertyErrorCallback(kPoweredProperty));
  }

  if (discoverable && adapter->IsDiscoverable() != *discoverable) {
    pending_properties_.insert(kDiscoverableProperty);
    adapter->SetDiscoverable(
        *discoverable,
        CreatePropertySetCallback(kDiscoverableProperty),
        CreatePropertyErrorCallback(kDiscoverableProperty));
  }

  if (pending_properties_.empty())
    SendResponse(true);
  return true;
}

base::Closure
BluetoothPrivateSetAdapterStateFunction::CreatePropertySetCallback(
    const std::string& property_name) {
  return base::Bind(
      &BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertySet,
      this,
      property_name);
}

base::Closure
BluetoothPrivateSetAdapterStateFunction::CreatePropertyErrorCallback(
    const std::string& property_name) {
  return base::Bind(
      &BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertyError,
      this,
      property_name);
}

void BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertySet(
    const std::string& property) {
  DCHECK(pending_properties_.find(property) != pending_properties_.end());
  DCHECK(failed_properties_.find(property) == failed_properties_.end());

  pending_properties_.erase(property);
  if (pending_properties_.empty()) {
    if (failed_properties_.empty())
      SendResponse(true);
    else
      SendError();
  }
}

void BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertyError(
    const std::string& property) {
  DCHECK(pending_properties_.find(property) != pending_properties_.end());
  DCHECK(failed_properties_.find(property) == failed_properties_.end());

  pending_properties_.erase(property);
  failed_properties_.insert(property);
  if (pending_properties_.empty())
    SendError();
}

void BluetoothPrivateSetAdapterStateFunction::SendError() {
  DCHECK(pending_properties_.empty());
  DCHECK(!failed_properties_.empty());

  std::vector<std::string> failed_vector;
  std::copy(failed_properties_.begin(),
            failed_properties_.end(),
            std::back_inserter(failed_vector));

  std::vector<std::string> replacements(1);
  replacements[0] = JoinString(failed_vector, ", ");
  std::string error =
      ReplaceStringPlaceholders(kSetAdapterPropertyError, replacements, NULL);
  SetError(error);
  SendResponse(false);
}

BluetoothPrivateSetPairingResponseFunction::
    BluetoothPrivateSetPairingResponseFunction() {}

BluetoothPrivateSetPairingResponseFunction::
    ~BluetoothPrivateSetPairingResponseFunction() {}

bool BluetoothPrivateSetPairingResponseFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  scoped_ptr<bt_private::SetPairingResponse::Params> params(
      bt_private::SetPairingResponse::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const bt_private::SetPairingResponseOptions& options = params->options;

  BluetoothEventRouter* router =
      BluetoothAPI::Get(browser_context())->event_router();
  if (!router->GetPairingDelegate(extension_id())) {
    SetError(kPairingNotEnabled);
    SendResponse(false);
    return true;
  }

  const std::string& device_address = options.device.address;
  device::BluetoothDevice* device = adapter->GetDevice(device_address);
  if (!device) {
    SetError(kDeviceNotFoundError);
    SendResponse(false);
    return true;
  }

  if (!ValidatePairingResponseOptions(device, options)) {
    SetError(kInvalidPairingResponseOptions);
    SendResponse(false);
    return true;
  }

  if (options.pincode.get()) {
    device->SetPinCode(*options.pincode.get());
  } else if (options.passkey.get()) {
    device->SetPasskey(*options.passkey.get());
  } else {
    switch (options.response) {
      case bt_private::PAIRING_RESPONSE_CONFIRM:
        device->ConfirmPairing();
        break;
      case bt_private::PAIRING_RESPONSE_REJECT:
        device->RejectPairing();
        break;
      case bt_private::PAIRING_RESPONSE_CANCEL:
        device->CancelPairing();
        break;
      default:
        NOTREACHED();
    }
  }

  SendResponse(true);
  return true;
}

}  // namespace api

}  // namespace extensions
