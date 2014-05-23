// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_api.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/utils.h"
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
const char kErrorDescriptorNotFoundFormat[] =
    "Descriptor with ID \"%s\" not found.";
const char kErrorDeviceNotFoundFormat[] =
    "Device with address \"%s\" not found.";
const char kErrorReadCharacteristicValueFailedFormat[] =
    "Failed to read value of characteristic with ID \"%s\".";
const char kErrorReadDescriptorValueFailedFormat[] =
    "Failed to read value of descriptor with ID \"%s\".";
const char kErrorServiceNotFoundFormat[] = "Service with ID \"%s\" not found.";
const char kErrorPlatformNotSupported[] =
    "This operation is not supported on the current platform";
const char kErrorWriteCharacteristicValueFailedFormat[] =
    "Failed to write value of characteristic with ID \"%s\".";

extensions::BluetoothLowEnergyEventRouter* GetEventRouter(
    BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return extensions::BluetoothLowEnergyAPI::Get(context)->event_router();
}

void DoWorkCallback(const base::Callback<bool()>& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
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
  SetResult(apibtle::CharacteristicToValue(&characteristic).release());
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
    result->Append(apibtle::CharacteristicToValue(iter->get()).release());

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

  scoped_ptr<apibtle::GetDescriptor::Params> params(
      apibtle::GetDescriptor::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::string descriptor_id = params->descriptor_id;

  apibtle::Descriptor descriptor;
  if (!event_router->GetDescriptor(descriptor_id, &descriptor)) {
    SetError(base::StringPrintf(kErrorDescriptorNotFoundFormat,
                                descriptor_id.c_str()));
    SendResponse(false);
    return false;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptor::Result::Create as it doesn't convert lists of enums
  // correctly.
  SetResult(apibtle::DescriptorToValue(&descriptor).release());
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetDescriptorsFunction::DoWork() {
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

  scoped_ptr<apibtle::GetDescriptors::Params> params(
      apibtle::GetDescriptors::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::string chrc_id = params->characteristic_id;

  BluetoothLowEnergyEventRouter::DescriptorList descriptor_list;
  if (!event_router->GetDescriptors(chrc_id, &descriptor_list)) {
    SetError(base::StringPrintf(kErrorCharacteristicNotFoundFormat,
                                chrc_id.c_str()));
    SendResponse(false);
    return false;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptors::Result::Create as it doesn't convert lists of
  // enums correctly.
  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (BluetoothLowEnergyEventRouter::DescriptorList::iterator iter =
           descriptor_list.begin();
       iter != descriptor_list.end();
       ++iter)
    result->Append(apibtle::DescriptorToValue(iter->get()).release());

  SetResult(result.release());
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyReadCharacteristicValueFunction::DoWork() {
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

  scoped_ptr<apibtle::ReadCharacteristicValue::Params> params(
      apibtle::ReadCharacteristicValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  instance_id_ = params->characteristic_id;

  if (!event_router->ReadCharacteristicValue(
          instance_id_,
          base::Bind(&BluetoothLowEnergyReadCharacteristicValueFunction::
                         SuccessCallback,
                     this),
          base::Bind(
              &BluetoothLowEnergyReadCharacteristicValueFunction::ErrorCallback,
              this))) {
    SetError(base::StringPrintf(kErrorCharacteristicNotFoundFormat,
                                instance_id_.c_str()));
    SendResponse(false);
    return false;
  }

  return true;
}

void BluetoothLowEnergyReadCharacteristicValueFunction::SuccessCallback() {
  // Obtain info on the characteristic and see whether or not the characteristic
  // is still around.
  apibtle::Characteristic characteristic;
  if (!GetEventRouter(browser_context())
           ->GetCharacteristic(instance_id_, &characteristic)) {
    SetError(base::StringPrintf(kErrorCharacteristicNotFoundFormat,
                                instance_id_.c_str()));
    SendResponse(false);
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristic::Result::Create as it doesn't convert lists of
  // enums correctly.
  SetResult(apibtle::CharacteristicToValue(&characteristic).release());
  SendResponse(true);
}

void BluetoothLowEnergyReadCharacteristicValueFunction::ErrorCallback() {
  SetError(base::StringPrintf(kErrorReadCharacteristicValueFailedFormat,
                              instance_id_.c_str()));
  SendResponse(false);
}

bool BluetoothLowEnergyWriteCharacteristicValueFunction::DoWork() {
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

  scoped_ptr<apibtle::WriteCharacteristicValue::Params> params(
      apibtle::WriteCharacteristicValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  instance_id_ = params->characteristic_id;
  std::vector<uint8> value(params->value.begin(), params->value.end());

  if (!event_router->WriteCharacteristicValue(
          instance_id_,
          value,
          base::Bind(&BluetoothLowEnergyWriteCharacteristicValueFunction::
                         SuccessCallback,
                     this),
          base::Bind(&BluetoothLowEnergyWriteCharacteristicValueFunction::
                         ErrorCallback,
                     this))) {
    SetError(base::StringPrintf(kErrorCharacteristicNotFoundFormat,
                                instance_id_.c_str()));
    SendResponse(false);
    return false;
  }

  return true;
}

void BluetoothLowEnergyWriteCharacteristicValueFunction::SuccessCallback() {
  results_ = apibtle::WriteCharacteristicValue::Results::Create();
  SendResponse(true);
}

void BluetoothLowEnergyWriteCharacteristicValueFunction::ErrorCallback() {
  SetError(base::StringPrintf(kErrorWriteCharacteristicValueFailedFormat,
                              instance_id_.c_str()));
  SendResponse(false);
}

bool BluetoothLowEnergyReadDescriptorValueFunction::DoWork() {
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

  scoped_ptr<apibtle::ReadDescriptorValue::Params> params(
      apibtle::ReadDescriptorValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  instance_id_ = params->descriptor_id;

  if (!event_router->ReadDescriptorValue(
          instance_id_,
          base::Bind(
              &BluetoothLowEnergyReadDescriptorValueFunction::SuccessCallback,
              this),
          base::Bind(
              &BluetoothLowEnergyReadDescriptorValueFunction::ErrorCallback,
              this))) {
    SetError(base::StringPrintf(kErrorDescriptorNotFoundFormat,
                                instance_id_.c_str()));
    SendResponse(false);
    return false;
  }

  return true;
}

void BluetoothLowEnergyReadDescriptorValueFunction::SuccessCallback() {
  // Obtain info on the descriptor and see whether or not the descriptor is
  // still around.
  apibtle::Descriptor descriptor;
  if (!GetEventRouter(browser_context())
           ->GetDescriptor(instance_id_, &descriptor)) {
    SetError(base::StringPrintf(kErrorDescriptorNotFoundFormat,
                                instance_id_.c_str()));
    SendResponse(false);
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptor::Results::Create as it doesn't convert lists of
  // enums correctly.
  SetResult(apibtle::DescriptorToValue(&descriptor).release());
  SendResponse(true);
}

void BluetoothLowEnergyReadDescriptorValueFunction::ErrorCallback() {
  SetError(base::StringPrintf(kErrorReadDescriptorValueFailedFormat,
                              instance_id_.c_str()));
  SendResponse(false);
}

bool BluetoothLowEnergyWriteDescriptorValueFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
}

}  // namespace api
}  // namespace extensions
