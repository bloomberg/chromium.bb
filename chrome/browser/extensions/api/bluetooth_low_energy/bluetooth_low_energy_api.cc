// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_api.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_api_advertisement.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/utils.h"
#include "chrome/common/extensions/api/bluetooth_low_energy.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/bluetooth/bluetooth_manifest_data.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

namespace apibtle = extensions::api::bluetooth_low_energy;

namespace extensions {

namespace {

const char kErrorAdapterNotInitialized[] =
    "Could not initialize Bluetooth adapter";
const char kErrorAlreadyConnected[] = "Already connected";
const char kErrorAlreadyNotifying[] = "Already notifying";
const char kErrorAuthenticationFailed[] = "Authentication failed";
const char kErrorCanceled[] = "Request canceled";
const char kErrorGattNotSupported[] = "Operation not supported by this service";
const char kErrorHigherSecurity[] = "Higher security needed";
const char kErrorInProgress[] = "In progress";
const char kErrorInsufficientAuthorization[] = "Insufficient authorization";
const char kErrorInvalidLength[] = "Invalid attribute value length";
const char kErrorNotConnected[] = "Not connected";
const char kErrorNotFound[] = "Instance not found";
const char kErrorNotNotifying[] = "Not notifying";
const char kErrorOperationFailed[] = "Operation failed";
const char kErrorPermissionDenied[] = "Permission denied";
const char kErrorPlatformNotSupported[] =
    "This operation is not supported on the current platform";
const char kErrorTimeout[] = "Operation timed out";
const char kErrorUnsupportedDevice[] =
    "This device is not supported on the current platform";
const char kErrorInvalidAdvertisementLength[] = "Invalid advertisement length";
const char kStatusAdvertisementAlreadyExists[] =
    "An advertisement is already advertising";
const char kStatusAdvertisementDoesNotExist[] =
    "This advertisement does not exist";

// Returns the correct error string based on error status |status|. This is used
// to set the value of |chrome.runtime.lastError.message| and should not be
// passed |BluetoothLowEnergyEventRouter::kStatusSuccess|.
std::string StatusToString(BluetoothLowEnergyEventRouter::Status status) {
  switch (status) {
    case BluetoothLowEnergyEventRouter::kStatusErrorPermissionDenied:
      return kErrorPermissionDenied;
    case BluetoothLowEnergyEventRouter::kStatusErrorNotFound:
      return kErrorNotFound;
    case BluetoothLowEnergyEventRouter::kStatusErrorAlreadyConnected:
      return kErrorAlreadyConnected;
    case BluetoothLowEnergyEventRouter::kStatusErrorAlreadyNotifying:
      return kErrorAlreadyNotifying;
    case BluetoothLowEnergyEventRouter::kStatusErrorNotConnected:
      return kErrorNotConnected;
    case BluetoothLowEnergyEventRouter::kStatusErrorInsufficientAuthorization:
      return kErrorInsufficientAuthorization;
    case BluetoothLowEnergyEventRouter::kStatusErrorNotNotifying:
      return kErrorNotNotifying;
    case BluetoothLowEnergyEventRouter::kStatusErrorInProgress:
      return kErrorInProgress;
    case BluetoothLowEnergyEventRouter::kStatusErrorAuthenticationFailed:
      return kErrorAuthenticationFailed;
    case BluetoothLowEnergyEventRouter::kStatusErrorHigherSecurity:
      return kErrorHigherSecurity;
    case BluetoothLowEnergyEventRouter::kStatusErrorCanceled:
      return kErrorCanceled;
    case BluetoothLowEnergyEventRouter::kStatusErrorTimeout:
      return kErrorTimeout;
    case BluetoothLowEnergyEventRouter::kStatusErrorUnsupportedDevice:
      return kErrorUnsupportedDevice;
    case BluetoothLowEnergyEventRouter::kStatusErrorInvalidLength:
      return kErrorInvalidLength;
    case BluetoothLowEnergyEventRouter::kStatusErrorGattNotSupported:
      return kErrorGattNotSupported;
    case BluetoothLowEnergyEventRouter::kStatusSuccess:
      NOTREACHED();
      break;
    default:
      return kErrorOperationFailed;
  }
  return "";
}

extensions::BluetoothLowEnergyEventRouter* GetEventRouter(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return extensions::BluetoothLowEnergyAPI::Get(context)->event_router();
}

void DoWorkCallback(const base::Callback<bool()>& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

scoped_ptr<device::BluetoothAdvertisement::ManufacturerData>
CreateManufacturerData(
    std::vector<linked_ptr<apibtle::ManufacturerData>>* manufacturer_data) {
  scoped_ptr<device::BluetoothAdvertisement::ManufacturerData> created_data(
      new device::BluetoothAdvertisement::ManufacturerData());
  for (const auto& it : *manufacturer_data) {
    std::vector<uint8_t> data(it->data.size());
    std::copy(it->data.begin(), it->data.end(), data.begin());
    (*created_data)[it->id] = data;
  }
  return created_data;
}

scoped_ptr<device::BluetoothAdvertisement::ServiceData> CreateServiceData(
    std::vector<linked_ptr<apibtle::ServiceData>>* service_data) {
  scoped_ptr<device::BluetoothAdvertisement::ServiceData> created_data(
      new device::BluetoothAdvertisement::ServiceData());
  for (const auto& it : *service_data) {
    std::vector<uint8_t> data(it->data.size());
    std::copy(it->data.begin(), it->data.end(), data.begin());
    (*created_data)[it->uuid] = data;
  }
  return created_data;
}

}  // namespace


static base::LazyInstance<BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>*
BluetoothLowEnergyAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
BluetoothLowEnergyAPI* BluetoothLowEnergyAPI::Get(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetFactoryInstance()->Get(context);
}

BluetoothLowEnergyAPI::BluetoothLowEnergyAPI(BrowserContext* context)
    : event_router_(new BluetoothLowEnergyEventRouter(context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BluetoothLowEnergyAPI::~BluetoothLowEnergyAPI() {
}

void BluetoothLowEnergyAPI::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

namespace api {

BluetoothLowEnergyExtensionFunction::BluetoothLowEnergyExtensionFunction() {
}

BluetoothLowEnergyExtensionFunction::~BluetoothLowEnergyExtensionFunction() {
}

bool BluetoothLowEnergyExtensionFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!BluetoothManifestData::CheckLowEnergyPermitted(extension())) {
    error_ = kErrorPermissionDenied;
    return false;
  }

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

bool BluetoothLowEnergyConnectFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::Connect::Params> params(
      apibtle::Connect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  bool persistent = false;  // Not persistent by default.
  apibtle::ConnectProperties* properties = params.get()->properties.get();
  if (properties)
    persistent = properties->persistent;

  event_router->Connect(
      persistent,
      extension(),
      params->device_address,
      base::Bind(&BluetoothLowEnergyConnectFunction::SuccessCallback, this),
      base::Bind(&BluetoothLowEnergyConnectFunction::ErrorCallback, this));

  return true;
}

void BluetoothLowEnergyConnectFunction::SuccessCallback() {
  SendResponse(true);
}

void BluetoothLowEnergyConnectFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyDisconnectFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::Disconnect::Params> params(
      apibtle::Disconnect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  event_router->Disconnect(
      extension(),
      params->device_address,
      base::Bind(&BluetoothLowEnergyDisconnectFunction::SuccessCallback, this),
      base::Bind(&BluetoothLowEnergyDisconnectFunction::ErrorCallback, this));

  return true;
}

void BluetoothLowEnergyDisconnectFunction::SuccessCallback() {
  SendResponse(true);
}

void BluetoothLowEnergyDisconnectFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyGetServiceFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  apibtle::Service service;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetService(params->service_id, &service);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return false;
  }

  results_ = apibtle::GetService::Results::Create(service);
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetServicesFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  BluetoothLowEnergyEventRouter::ServiceList service_list;
  if (!event_router->GetServices(params->device_address, &service_list)) {
    SetError(kErrorNotFound);
    SendResponse(false);
    return false;
  }

  results_ = apibtle::GetServices::Results::Create(service_list);
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetCharacteristicFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  apibtle::Characteristic characteristic;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetCharacteristic(
          extension(), params->characteristic_id, &characteristic);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  BluetoothLowEnergyEventRouter::CharacteristicList characteristic_list;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetCharacteristics(
          extension(), params->service_id, &characteristic_list);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  BluetoothLowEnergyEventRouter::ServiceList service_list;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetIncludedServices(params->service_id, &service_list);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return false;
  }

  results_ = apibtle::GetIncludedServices::Results::Create(service_list);
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetDescriptorFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  apibtle::Descriptor descriptor;
  BluetoothLowEnergyEventRouter::Status status = event_router->GetDescriptor(
      extension(), params->descriptor_id, &descriptor);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  BluetoothLowEnergyEventRouter::DescriptorList descriptor_list;
  BluetoothLowEnergyEventRouter::Status status = event_router->GetDescriptors(
      extension(), params->characteristic_id, &descriptor_list);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
  event_router->ReadCharacteristicValue(
      extension(),
      instance_id_,
      base::Bind(
          &BluetoothLowEnergyReadCharacteristicValueFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergyReadCharacteristicValueFunction::ErrorCallback,
          this));

  return true;
}

void BluetoothLowEnergyReadCharacteristicValueFunction::SuccessCallback() {
  // Obtain info on the characteristic and see whether or not the characteristic
  // is still around.
  apibtle::Characteristic characteristic;
  BluetoothLowEnergyEventRouter::Status status =
      GetEventRouter(browser_context())
          ->GetCharacteristic(extension(), instance_id_, &characteristic);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristic::Result::Create as it doesn't convert lists of
  // enums correctly.
  SetResult(apibtle::CharacteristicToValue(&characteristic).release());
  SendResponse(true);
}

void BluetoothLowEnergyReadCharacteristicValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyWriteCharacteristicValueFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  std::vector<uint8> value(params->value.begin(), params->value.end());
  event_router->WriteCharacteristicValue(
      extension(),
      params->characteristic_id,
      value,
      base::Bind(
          &BluetoothLowEnergyWriteCharacteristicValueFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergyWriteCharacteristicValueFunction::ErrorCallback,
          this));

  return true;
}

void BluetoothLowEnergyWriteCharacteristicValueFunction::SuccessCallback() {
  results_ = apibtle::WriteCharacteristicValue::Results::Create();
  SendResponse(true);
}

void BluetoothLowEnergyWriteCharacteristicValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyStartCharacteristicNotificationsFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::StartCharacteristicNotifications::Params> params(
      apibtle::StartCharacteristicNotifications::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  bool persistent = false;  // Not persistent by default.
  apibtle::NotificationProperties* properties = params.get()->properties.get();
  if (properties)
    persistent = properties->persistent;

  event_router->StartCharacteristicNotifications(
      persistent,
      extension(),
      params->characteristic_id,
      base::Bind(&BluetoothLowEnergyStartCharacteristicNotificationsFunction::
                     SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyStartCharacteristicNotificationsFunction::
                     ErrorCallback,
                 this));

  return true;
}

void
BluetoothLowEnergyStartCharacteristicNotificationsFunction::SuccessCallback() {
  SendResponse(true);
}

void BluetoothLowEnergyStartCharacteristicNotificationsFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyStopCharacteristicNotificationsFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::StopCharacteristicNotifications::Params> params(
      apibtle::StopCharacteristicNotifications::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  event_router->StopCharacteristicNotifications(
      extension(),
      params->characteristic_id,
      base::Bind(&BluetoothLowEnergyStopCharacteristicNotificationsFunction::
                     SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyStopCharacteristicNotificationsFunction::
                     ErrorCallback,
                 this));

  return true;
}

void
BluetoothLowEnergyStopCharacteristicNotificationsFunction::SuccessCallback() {
  SendResponse(true);
}

void BluetoothLowEnergyStopCharacteristicNotificationsFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyReadDescriptorValueFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
  event_router->ReadDescriptorValue(
      extension(),
      instance_id_,
      base::Bind(
          &BluetoothLowEnergyReadDescriptorValueFunction::SuccessCallback,
          this),
      base::Bind(&BluetoothLowEnergyReadDescriptorValueFunction::ErrorCallback,
                 this));

  return true;
}

void BluetoothLowEnergyReadDescriptorValueFunction::SuccessCallback() {
  // Obtain info on the descriptor and see whether or not the descriptor is
  // still around.
  apibtle::Descriptor descriptor;
  BluetoothLowEnergyEventRouter::Status status =
      GetEventRouter(browser_context())
          ->GetDescriptor(extension(), instance_id_, &descriptor);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptor::Results::Create as it doesn't convert lists of
  // enums correctly.
  SetResult(apibtle::DescriptorToValue(&descriptor).release());
  SendResponse(true);
}

void BluetoothLowEnergyReadDescriptorValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyWriteDescriptorValueFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::WriteDescriptorValue::Params> params(
      apibtle::WriteDescriptorValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::vector<uint8> value(params->value.begin(), params->value.end());
  event_router->WriteDescriptorValue(
      extension(),
      params->descriptor_id,
      value,
      base::Bind(
          &BluetoothLowEnergyWriteDescriptorValueFunction::SuccessCallback,
          this),
      base::Bind(&BluetoothLowEnergyWriteDescriptorValueFunction::ErrorCallback,
                 this));

  return true;
}

void BluetoothLowEnergyWriteDescriptorValueFunction::SuccessCallback() {
  results_ = apibtle::WriteDescriptorValue::Results::Create();
  SendResponse(true);
}

void BluetoothLowEnergyWriteDescriptorValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

BluetoothLowEnergyAdvertisementFunction::
    BluetoothLowEnergyAdvertisementFunction()
    : advertisements_manager_(nullptr) {
}

BluetoothLowEnergyAdvertisementFunction::
    ~BluetoothLowEnergyAdvertisementFunction() {
}

int BluetoothLowEnergyAdvertisementFunction::AddAdvertisement(
    BluetoothApiAdvertisement* advertisement) {
  DCHECK(advertisements_manager_);
  return advertisements_manager_->Add(advertisement);
}

BluetoothApiAdvertisement*
BluetoothLowEnergyAdvertisementFunction::GetAdvertisement(
    int advertisement_id) {
  DCHECK(advertisements_manager_);
  return advertisements_manager_->Get(extension_id(), advertisement_id);
}

void BluetoothLowEnergyAdvertisementFunction::RemoveAdvertisement(
    int advertisement_id) {
  DCHECK(advertisements_manager_);
  advertisements_manager_->Remove(extension_id(), advertisement_id);
}

bool BluetoothLowEnergyAdvertisementFunction::RunAsync() {
  Initialize();
  return BluetoothLowEnergyExtensionFunction::RunAsync();
}

void BluetoothLowEnergyAdvertisementFunction::Initialize() {
  advertisements_manager_ =
      ApiResourceManager<BluetoothApiAdvertisement>::Get(browser_context());
}

static bool IsAutoLaunchedKioskApp(const ExtensionId& id) {
#if defined(OS_CHROMEOS)
  chromeos::KioskAppManager::App app_info;
  return chromeos::KioskAppManager::Get()->GetApp(id, &app_info) &&
         app_info.was_auto_launched_with_zero_delay;
#else
  return false;
#endif
}

static bool IsPeripheralFlagEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBLEAdvertising);
}

// RegisterAdvertisement:

bool BluetoothLowEnergyRegisterAdvertisementFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check permissions in manifest.
  if (!BluetoothManifestData::CheckPeripheralPermitted(extension())) {
    error_ = kErrorPermissionDenied;
    SendResponse(false);
    return false;
  }

  // For this API to be available the app has to be either auto
  // launched in Kiosk Mode or the enable-ble-advertisement-in-apps
  // should be set.
  if (!(IsAutoLaunchedKioskApp(extension()->id()) ||
        IsPeripheralFlagEnabled())) {
    error_ = kErrorPermissionDenied;
    SendResponse(false);
    return false;
  }

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  scoped_ptr<apibtle::RegisterAdvertisement::Params> params(
      apibtle::RegisterAdvertisement::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  scoped_ptr<device::BluetoothAdvertisement::Data> advertisement_data(
      new device::BluetoothAdvertisement::Data(
          params->advertisement.type ==
                  apibtle::AdvertisementType::ADVERTISEMENT_TYPE_BROADCAST
              ? device::BluetoothAdvertisement::AdvertisementType::
                    ADVERTISEMENT_TYPE_BROADCAST
              : device::BluetoothAdvertisement::AdvertisementType::
                    ADVERTISEMENT_TYPE_PERIPHERAL));

  advertisement_data->set_service_uuids(
      params->advertisement.service_uuids.Pass());
  advertisement_data->set_solicit_uuids(
      params->advertisement.solicit_uuids.Pass());
  if (params->advertisement.manufacturer_data) {
    advertisement_data->set_manufacturer_data(
        CreateManufacturerData(params->advertisement.manufacturer_data.get())
            .Pass());
  }
  if (params->advertisement.service_data) {
    advertisement_data->set_service_data(
        CreateServiceData(params->advertisement.service_data.get()).Pass());
  }

  event_router->adapter()->RegisterAdvertisement(
      advertisement_data.Pass(),
      base::Bind(
          &BluetoothLowEnergyRegisterAdvertisementFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergyRegisterAdvertisementFunction::ErrorCallback,
          this));

  return true;
}

void BluetoothLowEnergyRegisterAdvertisementFunction::SuccessCallback(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  results_ = apibtle::RegisterAdvertisement::Results::Create(AddAdvertisement(
      new BluetoothApiAdvertisement(extension_id(), advertisement)));
  SendResponse(true);
}

void BluetoothLowEnergyRegisterAdvertisementFunction::ErrorCallback(
    device::BluetoothAdvertisement::ErrorCode status) {
  switch (status) {
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_ALREADY_EXISTS:
      SetError(kStatusAdvertisementAlreadyExists);
      break;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_INVALID_LENGTH:
      SetError(kErrorInvalidAdvertisementLength);
      break;
    default:
      SetError(kErrorOperationFailed);
  }
  SendResponse(false);
}

// UnregisterAdvertisement:

bool BluetoothLowEnergyUnregisterAdvertisementFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check permission in the manifest.
  if (!BluetoothManifestData::CheckPeripheralPermitted(extension())) {
    error_ = kErrorPermissionDenied;
    SendResponse(false);
    return false;
  }

  // For this API to be available the app has to be either auto
  // launched in Kiosk Mode or the enable-ble-advertisement-in-apps
  // should be set.
  if (!(IsAutoLaunchedKioskApp(extension()->id()) ||
        IsPeripheralFlagEnabled())) {
    error_ = kErrorPermissionDenied;
    SendResponse(false);
    return false;
  }

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // If we don't have an initialized adapter, unregistering is a no-op.
  if (!event_router->HasAdapter())
    return true;

  scoped_ptr<apibtle::UnregisterAdvertisement::Params> params(
      apibtle::UnregisterAdvertisement::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  BluetoothApiAdvertisement* advertisement =
      GetAdvertisement(params->advertisement_id);
  if (!advertisement) {
    error_ = kStatusAdvertisementDoesNotExist;
    SendResponse(false);
    return false;
  }

  advertisement->advertisement()->Unregister(
      base::Bind(
          &BluetoothLowEnergyUnregisterAdvertisementFunction::SuccessCallback,
          this, params->advertisement_id),
      base::Bind(
          &BluetoothLowEnergyUnregisterAdvertisementFunction::ErrorCallback,
          this, params->advertisement_id));

  return true;
}

void BluetoothLowEnergyUnregisterAdvertisementFunction::SuccessCallback(
    int advertisement_id) {
  RemoveAdvertisement(advertisement_id);
  SendResponse(true);
}

void BluetoothLowEnergyUnregisterAdvertisementFunction::ErrorCallback(
    int advertisement_id,
    device::BluetoothAdvertisement::ErrorCode status) {
  RemoveAdvertisement(advertisement_id);
  switch (status) {
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_DOES_NOT_EXIST:
      SetError(kStatusAdvertisementDoesNotExist);
      break;
    default:
      SetError(kErrorOperationFailed);
  }
  SendResponse(false);
}

}  // namespace api
}  // namespace extensions
