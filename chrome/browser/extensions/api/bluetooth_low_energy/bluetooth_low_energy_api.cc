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

bool BluetoothLowEnergyExtensionFunction::RunImpl() {
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

  results_ = apibtle::GetServices::Results::Create(service_list).Pass();
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetCharacteristicFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
}

bool BluetoothLowEnergyGetCharacteristicsFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
}

bool BluetoothLowEnergyGetIncludedServicesFunction::DoWork() {
  // TODO(armansito): Implement.
  SetError("Call not supported.");
  SendResponse(false);
  return false;
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
