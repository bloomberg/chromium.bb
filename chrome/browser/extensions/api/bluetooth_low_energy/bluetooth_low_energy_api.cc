// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_api.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"
#include "chrome/common/extensions/api/bluetooth_low_energy.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"

using content::BrowserContext;
using content::BrowserThread;

namespace apibtle = extensions::api::bluetooth_low_energy;

namespace {

const char kErrorAdapterNotInitialized[] =
    "Could not initialize Bluetooth adapter.";
const char kErrorCharacteristicNotFoundFormat[] =
    "Characteristic with ID \"%s\" not found.";
const char kErrorDeviceNotFoundFormat[] =
    "Device with address \"%s\" not found.";
const char kErrorServiceNotFoundFormat[] = "Service with ID \"%s\" not found.";
const char kErrorPlatformNotSupported[] =
    "This operation is not supported on the current platform";

extensions::BluetoothLowEnergyEventRouter* GetEventRouter(
    BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return extensions::BluetoothLowEnergyAPI::Get(context)->event_router();
}

void DoWorkCallback(const base::Callback<bool()>& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

// TODO(armansito): Remove this function once the described bug is fixed.
// (See crbug.com/368368).
//
// Converts an apibtle::Characteristic to a base::Value. This function is
// necessary, as json_schema_compiler::util::AddItemToList has no template
// specialization for user defined enums, which get treated as integers. This is
// because Characteristic contains a list of enum CharacteristicProperty.
scoped_ptr<base::DictionaryValue> CharacteristicToValue(
    apibtle::Characteristic* from) {
  // Copy the properties. Use Characteristic::ToValue to generate the result
  // dictionary without the properties, to prevent json_schema_compiler from
  // failing.
  std::vector<apibtle::CharacteristicProperty> properties = from->properties;
  from->properties.clear();
  scoped_ptr<base::DictionaryValue> to = from->ToValue();

  // Manually set each property.
  scoped_ptr<base::ListValue> property_list(new base::ListValue());
  for (std::vector<apibtle::CharacteristicProperty>::iterator iter =
           properties.begin();
       iter != properties.end();
       ++iter)
    property_list->Append(new base::StringValue(apibtle::ToString(*iter)));

  to->Set("properties", property_list.release());

  return to.Pass();
}

}  // namespace

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>*
BluetoothLowEnergyAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
BluetoothLowEnergyAPI* BluetoothLowEnergyAPI::Get(BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return GetFactoryInstance()->Get(context);
}

BluetoothLowEnergyAPI::BluetoothLowEnergyAPI(BrowserContext* context)
    : event_router_(new BluetoothLowEnergyEventRouter(context)),
      browser_context_(context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

BluetoothLowEnergyAPI::~BluetoothLowEnergyAPI() {
}

void BluetoothLowEnergyAPI::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

namespace api {

BluetoothLowEnergyExtensionFunction::BluetoothLowEnergyExtensionFunction() {
}

BluetoothLowEnergyExtensionFunction::~BluetoothLowEnergyExtensionFunction() {
}

bool BluetoothLowEnergyExtensionFunction::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());
  if (!event_router->IsBluetoothSupported()) {
    SetError(kErrorPlatformNotSupported);
    return false;
  }

  // It is safe to pass |this| here as ExtensionFunction is refcounted.
  if (!event_router->InitializeAdapterAndInvokeCallback(base::Bind(
          &DoWorkCallback,
          base::Bind(&BluetoothLowEnergyExtensionFunction::DoWork, this)))) {
    SetError(kErrorAdapterNotInitialized);
    return false;
  }

  return true;
}

bool BluetoothLowEnergyGetServiceFunction::DoWork() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::GetService::Params> params(
      apibtle::GetService::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::string service_id = params->service_id;

  apibtle::Service service;
  if (!event_router->GetService(service_id, &service)) {
    SetError(
        base::StringPrintf(kErrorServiceNotFoundFormat, service_id.c_str()));
    SendResponse(false);
    return false;
  }

  results_ = apibtle::GetService::Results::Create(service);
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetServicesFunction::DoWork() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::GetServices::Params> params(
      apibtle::GetServices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::string device_address = params->device_address;

  BluetoothLowEnergyEventRouter::ServiceList service_list;
  if (!event_router->GetServices(device_address, &service_list)) {
    SetError(
        base::StringPrintf(kErrorDeviceNotFoundFormat, device_address.c_str()));
    SendResponse(false);
    return false;
  }

  results_ = apibtle::GetServices::Results::Create(service_list);
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetCharacteristicFunction::DoWork() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::GetCharacteristic::Params> params(
      apibtle::GetCharacteristic::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::string characteristic_id = params->characteristic_id;

  apibtle::Characteristic characteristic;
  if (!event_router->GetCharacteristic(characteristic_id, &characteristic)) {
    SetError(base::StringPrintf(kErrorCharacteristicNotFoundFormat,
                                characteristic_id.c_str()));
    SendResponse(false);
    return false;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristic::Result::Create as it doesn't convert lists of
  // enums correctly.
  SetResult(CharacteristicToValue(&characteristic).release());
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetCharacteristicsFunction::DoWork() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::GetCharacteristics::Params> params(
      apibtle::GetCharacteristics::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::string service_id = params->service_id;

  BluetoothLowEnergyEventRouter::CharacteristicList characteristic_list;
  if (!event_router->GetCharacteristics(service_id, &characteristic_list)) {
    SetError(
        base::StringPrintf(kErrorServiceNotFoundFormat, service_id.c_str()));

    SendResponse(false);
    return false;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristics::Result::Create as it doesn't convert lists of
  // enums correctly.
  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (BluetoothLowEnergyEventRouter::CharacteristicList::iterator iter =
           characteristic_list.begin();
       iter != characteristic_list.end();
       ++iter)
    result->Append(CharacteristicToValue(iter->get()).release());

  SetResult(result.release());
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetIncludedServicesFunction::DoWork() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::GetIncludedServices::Params> params(
      apibtle::GetIncludedServices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::string service_id = params->service_id;

  BluetoothLowEnergyEventRouter::ServiceList service_list;
  if (!event_router->GetIncludedServices(service_id, &service_list)) {
    SetError(
        base::StringPrintf(kErrorServiceNotFoundFormat, service_id.c_str()));
    SendResponse(false);
    return false;
  }

  results_ = apibtle::GetIncludedServices::Results::Create(service_list);
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetDescriptorFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
}

bool BluetoothLowEnergyGetDescriptorsFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
}

bool BluetoothLowEnergyReadCharacteristicValueFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
}

bool BluetoothLowEnergyWriteCharacteristicValueFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
}

bool BluetoothLowEnergyReadDescriptorValueFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
}

bool BluetoothLowEnergyWriteDescriptorValueFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
}

}  // namespace api
}  // namespace extensions
