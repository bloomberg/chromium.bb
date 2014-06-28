// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_connection.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_notify_session.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/utils.h"
#include "chrome/common/extensions/api/bluetooth/bluetooth_manifest_data.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"

using content::BrowserThread;

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattConnection;
using device::BluetoothGattDescriptor;
using device::BluetoothGattService;

namespace apibtle = extensions::api::bluetooth_low_energy;

namespace {

void PopulateService(const BluetoothGattService* service,
                     apibtle::Service* out) {
  DCHECK(out);

  out->uuid = service->GetUUID().canonical_value();
  out->is_primary = service->IsPrimary();
  out->is_local = service->IsLocal();
  out->instance_id.reset(new std::string(service->GetIdentifier()));

  if (!service->GetDevice())
    return;

  out->device_address.reset(
      new std::string(service->GetDevice()->GetAddress()));
}

void PopulateCharacteristicProperties(
    BluetoothGattCharacteristic::Properties properties,
    std::vector<apibtle::CharacteristicProperty>* api_properties) {
  DCHECK(api_properties && api_properties->empty());

  if (properties == BluetoothGattCharacteristic::kPropertyNone)
    return;

  if (properties & BluetoothGattCharacteristic::kPropertyBroadcast)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_BROADCAST);
  if (properties & BluetoothGattCharacteristic::kPropertyRead)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_READ);
  if (properties & BluetoothGattCharacteristic::kPropertyWriteWithoutResponse) {
    api_properties->push_back(
        apibtle::CHARACTERISTIC_PROPERTY_WRITEWITHOUTRESPONSE);
  }
  if (properties & BluetoothGattCharacteristic::kPropertyWrite)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_WRITE);
  if (properties & BluetoothGattCharacteristic::kPropertyNotify)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_NOTIFY);
  if (properties & BluetoothGattCharacteristic::kPropertyIndicate)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_INDICATE);
  if (properties &
      BluetoothGattCharacteristic::kPropertyAuthenticatedSignedWrites) {
    api_properties->push_back(
        apibtle::CHARACTERISTIC_PROPERTY_AUTHENTICATEDSIGNEDWRITES);
  }
  if (properties & BluetoothGattCharacteristic::kPropertyExtendedProperties) {
    api_properties->push_back(
        apibtle::CHARACTERISTIC_PROPERTY_EXTENDEDPROPERTIES);
  }
  if (properties & BluetoothGattCharacteristic::kPropertyReliableWrite)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_RELIABLEWRITE);
  if (properties & BluetoothGattCharacteristic::kPropertyWritableAuxiliaries) {
    api_properties->push_back(
        apibtle::CHARACTERISTIC_PROPERTY_WRITABLEAUXILIARIES);
  }
}

void PopulateCharacteristic(const BluetoothGattCharacteristic* characteristic,
                            apibtle::Characteristic* out) {
  DCHECK(out);

  out->uuid = characteristic->GetUUID().canonical_value();
  out->is_local = characteristic->IsLocal();
  out->instance_id.reset(new std::string(characteristic->GetIdentifier()));

  PopulateService(characteristic->GetService(), &out->service);
  PopulateCharacteristicProperties(characteristic->GetProperties(),
                                   &out->properties);

  const std::vector<uint8>& value = characteristic->GetValue();
  if (value.empty())
    return;

  out->value.reset(new std::string(value.begin(), value.end()));
}

void PopulateDescriptor(const BluetoothGattDescriptor* descriptor,
                        apibtle::Descriptor* out) {
  DCHECK(out);

  out->uuid = descriptor->GetUUID().canonical_value();
  out->is_local = descriptor->IsLocal();
  out->instance_id.reset(new std::string(descriptor->GetIdentifier()));

  PopulateCharacteristic(descriptor->GetCharacteristic(), &out->characteristic);

  const std::vector<uint8>& value = descriptor->GetValue();
  if (value.empty())
    return;

  out->value.reset(new std::string(value.begin(), value.end()));
}

typedef extensions::ApiResourceManager<extensions::BluetoothLowEnergyConnection>
    ConnectionResourceManager;
ConnectionResourceManager* GetConnectionResourceManager(
    content::BrowserContext* context) {
  ConnectionResourceManager* manager = ConnectionResourceManager::Get(context);
  DCHECK(manager)
      << "There is no Bluetooth low energy connection manager. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<BluetoothLowEnergyConnection>.";
  return manager;
}

typedef extensions::ApiResourceManager<
    extensions::BluetoothLowEnergyNotifySession> NotifySessionResourceManager;
NotifySessionResourceManager* GetNotifySessionResourceManager(
    content::BrowserContext* context) {
  NotifySessionResourceManager* manager =
      NotifySessionResourceManager::Get(context);
  DCHECK(manager)
      << "There is no Bluetooth low energy value update session manager."
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<BluetoothLowEnergyNotifySession>.";
  return manager;
}

}  // namespace

namespace extensions {

BluetoothLowEnergyEventRouter::BluetoothLowEnergyEventRouter(
    content::BrowserContext* context)
    : adapter_(NULL), browser_context_(context), weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(browser_context_);
  VLOG(1) << "Initializing BluetoothLowEnergyEventRouter.";

  if (!IsBluetoothSupported()) {
    VLOG(1) << "Bluetooth not supported on the current platform.";
    return;
  }
}

BluetoothLowEnergyEventRouter::~BluetoothLowEnergyEventRouter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!adapter_.get())
    return;

  adapter_->RemoveObserver(this);

  for (std::set<std::string>::const_iterator iter = observed_devices_.begin();
       iter != observed_devices_.end();
       ++iter) {
    BluetoothDevice* device = adapter_->GetDevice(*iter);
    if (!device)
      continue;
    device->RemoveObserver(this);
  }

  for (std::set<std::string>::const_iterator iter =
           observed_gatt_services_.begin();
       iter != observed_gatt_services_.end();
       ++iter) {
    BluetoothGattService* service = FindServiceById(*iter);
    if (!service)
      continue;
    service->RemoveObserver(this);
  }

  adapter_ = NULL;
}

bool BluetoothLowEnergyEventRouter::IsBluetoothSupported() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return adapter_.get() ||
         BluetoothAdapterFactory::IsBluetoothAdapterAvailable();
}

bool BluetoothLowEnergyEventRouter::InitializeAdapterAndInvokeCallback(
    const base::Closure& callback) {
  if (!IsBluetoothSupported())
    return false;

  if (adapter_.get()) {
    callback.Run();
    return true;
  }

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothLowEnergyEventRouter::OnGetAdapter,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
  return true;
}

bool BluetoothLowEnergyEventRouter::HasAdapter() const {
  return (adapter_.get() != NULL);
}

void BluetoothLowEnergyEventRouter::Connect(
    bool persistent,
    const Extension* extension,
    const std::string& device_address,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  const std::string extension_id = extension->id();
  const std::string connect_id = extension_id + device_address;

  if (connecting_devices_.count(connect_id) != 0) {
    error_callback.Run(kStatusErrorInProgress);
    return;
  }

  BluetoothLowEnergyConnection* conn =
      FindConnection(extension_id, device_address);
  if (conn) {
    if (conn->GetConnection()->IsConnected()) {
      VLOG(1) << "Application already connected to device: " << device_address;
      error_callback.Run(kStatusErrorAlreadyConnected);
      return;
    }

    // There is a connection object but it's no longer active. Simply remove it.
    RemoveConnection(extension_id, device_address);
  }

  BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (!device) {
    VLOG(1) << "Bluetooth device not found: " << device_address;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  connecting_devices_.insert(connect_id);
  device->CreateGattConnection(
      base::Bind(&BluetoothLowEnergyEventRouter::OnCreateGattConnection,
                 weak_ptr_factory_.GetWeakPtr(),
                 persistent,
                 extension_id,
                 device_address,
                 callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnConnectError,
                 weak_ptr_factory_.GetWeakPtr(),
                 extension_id,
                 device_address,
                 error_callback));
}

void BluetoothLowEnergyEventRouter::Disconnect(
    const Extension* extension,
    const std::string& device_address,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  const std::string extension_id = extension->id();
  const std::string disconnect_id = extension_id + device_address;

  if (disconnecting_devices_.count(disconnect_id) != 0) {
    error_callback.Run(kStatusErrorInProgress);
    return;
  }

  BluetoothLowEnergyConnection* conn =
      FindConnection(extension_id, device_address);
  if (!conn || !conn->GetConnection()->IsConnected()) {
    VLOG(1) << "Application not connected to device: " << device_address;
    error_callback.Run(kStatusErrorNotConnected);
    return;
  }

  disconnecting_devices_.insert(disconnect_id);
  conn->GetConnection()->Disconnect(
      base::Bind(&BluetoothLowEnergyEventRouter::OnDisconnect,
                 weak_ptr_factory_.GetWeakPtr(),
                 extension_id,
                 device_address,
                 callback));
}

bool BluetoothLowEnergyEventRouter::GetServices(
    const std::string& device_address,
    ServiceList* out_services) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(out_services);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return false;
  }

  BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (!device) {
    VLOG(1) << "Bluetooth device not found: " << device_address;
    return false;
  }

  out_services->clear();

  const std::vector<BluetoothGattService*>& services =
      device->GetGattServices();
  for (std::vector<BluetoothGattService*>::const_iterator iter =
           services.begin();
       iter != services.end();
       ++iter) {
    // Populate an API service and add it to the return value.
    const BluetoothGattService* service = *iter;
    linked_ptr<apibtle::Service> api_service(new apibtle::Service());
    PopulateService(service, api_service.get());

    out_services->push_back(api_service);
  }

  return true;
}

BluetoothLowEnergyEventRouter::Status BluetoothLowEnergyEventRouter::GetService(
    const std::string& instance_id,
    apibtle::Service* out_service) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(out_service);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothGattService* gatt_service = FindServiceById(instance_id);
  if (!gatt_service) {
    VLOG(1) << "Service not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  PopulateService(gatt_service, out_service);
  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetIncludedServices(
    const std::string& instance_id,
    ServiceList* out_services) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(out_services);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothGattService* service = FindServiceById(instance_id);
  if (!service) {
    VLOG(1) << "Service not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  out_services->clear();

  const std::vector<BluetoothGattService*>& includes =
      service->GetIncludedServices();
  for (std::vector<BluetoothGattService*>::const_iterator iter =
           includes.begin();
       iter != includes.end();
       ++iter) {
    // Populate an API service and add it to the return value.
    const BluetoothGattService* included = *iter;
    linked_ptr<apibtle::Service> api_service(new apibtle::Service());
    PopulateService(included, api_service.get());

    out_services->push_back(api_service);
  }

  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetCharacteristics(
    const Extension* extension,
    const std::string& instance_id,
    CharacteristicList* out_characteristics) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  DCHECK(out_characteristics);
  if (!adapter_) {
    VLOG(1) << "BlutoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothGattService* service = FindServiceById(instance_id);
  if (!service) {
    VLOG(1) << "Service not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  BluetoothPermissionRequest request(service->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access the characteristics of this "
            << "service: " << instance_id;
    return kStatusErrorPermissionDenied;
  }

  out_characteristics->clear();

  const std::vector<BluetoothGattCharacteristic*>& characteristics =
      service->GetCharacteristics();
  for (std::vector<BluetoothGattCharacteristic*>::const_iterator iter =
           characteristics.begin();
       iter != characteristics.end();
       ++iter) {
    // Populate an API characteristic and add it to the return value.
    const BluetoothGattCharacteristic* characteristic = *iter;
    linked_ptr<apibtle::Characteristic> api_characteristic(
        new apibtle::Characteristic());
    PopulateCharacteristic(characteristic, api_characteristic.get());

    out_characteristics->push_back(api_characteristic);
  }

  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetCharacteristic(
    const Extension* extension,
    const std::string& instance_id,
    apibtle::Characteristic* out_characteristic) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  DCHECK(out_characteristic);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this characteristic: "
            << instance_id;
    return kStatusErrorPermissionDenied;
  }

  PopulateCharacteristic(characteristic, out_characteristic);
  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetDescriptors(
    const Extension* extension,
    const std::string& instance_id,
    DescriptorList* out_descriptors) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  DCHECK(out_descriptors);
  if (!adapter_) {
    VLOG(1) << "BlutoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access the descriptors of this "
            << "characteristic: " << instance_id;
    return kStatusErrorPermissionDenied;
  }

  out_descriptors->clear();

  const std::vector<BluetoothGattDescriptor*>& descriptors =
      characteristic->GetDescriptors();
  for (std::vector<BluetoothGattDescriptor*>::const_iterator iter =
           descriptors.begin();
       iter != descriptors.end();
       ++iter) {
    // Populate an API descriptor and add it to the return value.
    const BluetoothGattDescriptor* descriptor = *iter;
    linked_ptr<apibtle::Descriptor> api_descriptor(new apibtle::Descriptor());
    PopulateDescriptor(descriptor, api_descriptor.get());

    out_descriptors->push_back(api_descriptor);
  }

  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetDescriptor(
    const Extension* extension,
    const std::string& instance_id,
    api::bluetooth_low_energy::Descriptor* out_descriptor) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  DCHECK(out_descriptor);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothGattDescriptor* descriptor = FindDescriptorById(instance_id);
  if (!descriptor) {
    VLOG(1) << "Descriptor not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  BluetoothPermissionRequest request(
      descriptor->GetCharacteristic()->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this descriptor: "
            << instance_id;
    return kStatusErrorPermissionDenied;
  }

  PopulateDescriptor(descriptor, out_descriptor);
  return kStatusSuccess;
}

void BluetoothLowEnergyEventRouter::ReadCharacteristicValue(
    const Extension* extension,
    const std::string& instance_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  BluetoothGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this characteristic: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothLowEnergyEventRouter::OnValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void BluetoothLowEnergyEventRouter::WriteCharacteristicValue(
    const Extension* extension,
    const std::string& instance_id,
    const std::vector<uint8>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  BluetoothGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this characteristic: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  characteristic->WriteRemoteCharacteristic(
      value,
      callback,
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void BluetoothLowEnergyEventRouter::StartCharacteristicNotifications(
    bool persistent,
    const Extension* extension,
    const std::string& instance_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  const std::string extension_id = extension->id();
  const std::string session_id = extension_id + instance_id;

  if (pending_session_calls_.count(session_id) != 0) {
    error_callback.Run(kStatusErrorInProgress);
    return;
  }

  BluetoothLowEnergyNotifySession* session =
      FindNotifySession(extension_id, instance_id);
  if (session) {
    if (session->GetSession()->IsActive()) {
      VLOG(1) << "Application has already enabled notifications from "
              << "characteristic: " << instance_id;
      error_callback.Run(kStatusErrorAlreadyNotifying);
      return;
    }

    RemoveNotifySession(extension_id, instance_id);
  }

  BluetoothGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this characteristic: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  pending_session_calls_.insert(session_id);
  characteristic->StartNotifySession(
      base::Bind(&BluetoothLowEnergyEventRouter::OnStartNotifySession,
                 weak_ptr_factory_.GetWeakPtr(),
                 persistent,
                 extension_id,
                 instance_id,
                 callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnStartNotifySessionError,
                 weak_ptr_factory_.GetWeakPtr(),
                 extension_id,
                 instance_id,
                 error_callback));
}

void BluetoothLowEnergyEventRouter::StopCharacteristicNotifications(
    const Extension* extension,
    const std::string& instance_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  const std::string extension_id = extension->id();

  BluetoothLowEnergyNotifySession* session =
      FindNotifySession(extension_id, instance_id);
  if (!session || !session->GetSession()->IsActive()) {
    VLOG(1) << "Application has not enabled notifications from "
            << "characteristic: " << instance_id;
    error_callback.Run(kStatusErrorNotNotifying);
    return;
  }

  session->GetSession()->Stop(
      base::Bind(&BluetoothLowEnergyEventRouter::OnStopNotifySession,
                 weak_ptr_factory_.GetWeakPtr(),
                 extension_id,
                 instance_id,
                 callback));
}

void BluetoothLowEnergyEventRouter::ReadDescriptorValue(
    const Extension* extension,
    const std::string& instance_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  BluetoothGattDescriptor* descriptor = FindDescriptorById(instance_id);
  if (!descriptor) {
    VLOG(1) << "Descriptor not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      descriptor->GetCharacteristic()->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this descriptor: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  descriptor->ReadRemoteDescriptor(
      base::Bind(&BluetoothLowEnergyEventRouter::OnValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void BluetoothLowEnergyEventRouter::WriteDescriptorValue(
    const Extension* extension,
    const std::string& instance_id,
    const std::vector<uint8>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  BluetoothGattDescriptor* descriptor = FindDescriptorById(instance_id);
  if (!descriptor) {
    VLOG(1) << "Descriptor not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      descriptor->GetCharacteristic()->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this descriptor: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  descriptor->WriteRemoteDescriptor(
      value,
      callback,
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void BluetoothLowEnergyEventRouter::SetAdapterForTesting(
    device::BluetoothAdapter* adapter) {
  adapter_ = adapter;
  InitializeIdentifierMappings();
}

void BluetoothLowEnergyEventRouter::DeviceAdded(BluetoothAdapter* adapter,
                                                BluetoothDevice* device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(observed_devices_.find(device->GetAddress()) ==
         observed_devices_.end());
  device->AddObserver(this);
  observed_devices_.insert(device->GetAddress());
}

void BluetoothLowEnergyEventRouter::DeviceRemoved(BluetoothAdapter* adapter,
                                                  BluetoothDevice* device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(observed_devices_.find(device->GetAddress()) !=
         observed_devices_.end());
  device->RemoveObserver(this);
  observed_devices_.erase(device->GetAddress());
}

void BluetoothLowEnergyEventRouter::GattServiceAdded(
    BluetoothDevice* device,
    BluetoothGattService* service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT service added: " << service->GetIdentifier();

  DCHECK(observed_gatt_services_.find(service->GetIdentifier()) ==
         observed_gatt_services_.end());
  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) ==
         service_id_to_device_address_.end());

  service->AddObserver(this);

  const std::string& service_id = service->GetIdentifier();
  observed_gatt_services_.insert(service_id);
  service_id_to_device_address_[service_id] = device->GetAddress();

  // Signal API event.
  apibtle::Service api_service;
  PopulateService(service, &api_service);

  scoped_ptr<base::ListValue> args =
      apibtle::OnServiceAdded::Create(api_service);
  scoped_ptr<Event> event(
      new Event(apibtle::OnServiceAdded::kEventName, args.Pass()));
  EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

void BluetoothLowEnergyEventRouter::GattServiceRemoved(
    BluetoothDevice* device,
    BluetoothGattService* service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT service removed: " << service->GetIdentifier();

  DCHECK(observed_gatt_services_.find(service->GetIdentifier()) !=
         observed_gatt_services_.end());
  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) !=
         service_id_to_device_address_.end());

  service->RemoveObserver(this);
  observed_gatt_services_.erase(service->GetIdentifier());

  DCHECK(device->GetAddress() ==
         service_id_to_device_address_[service->GetIdentifier()]);
  service_id_to_device_address_.erase(service->GetIdentifier());

  // Signal API event.
  apibtle::Service api_service;
  PopulateService(service, &api_service);

  scoped_ptr<base::ListValue> args =
      apibtle::OnServiceRemoved::Create(api_service);
  scoped_ptr<Event> event(
      new Event(apibtle::OnServiceRemoved::kEventName, args.Pass()));
  EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

void BluetoothLowEnergyEventRouter::GattServiceChanged(
    BluetoothGattService* service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT service changed: " << service->GetIdentifier();

  DCHECK(observed_gatt_services_.find(service->GetIdentifier()) !=
         observed_gatt_services_.end());
  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) !=
         service_id_to_device_address_.end());

  // Signal API event.
  apibtle::Service api_service;
  PopulateService(service, &api_service);

  DispatchEventToExtensionsWithPermission(
      apibtle::OnServiceChanged::kEventName,
      service->GetUUID(),
      "" /* characteristic_id */,
      apibtle::OnServiceChanged::Create(api_service));
}

void BluetoothLowEnergyEventRouter::GattCharacteristicAdded(
    BluetoothGattService* service,
    BluetoothGattCharacteristic* characteristic) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT characteristic added: " << characteristic->GetIdentifier();

  DCHECK(chrc_id_to_service_id_.find(characteristic->GetIdentifier()) ==
         chrc_id_to_service_id_.end());
  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) !=
         service_id_to_device_address_.end());

  chrc_id_to_service_id_[characteristic->GetIdentifier()] =
      service->GetIdentifier();
}

void BluetoothLowEnergyEventRouter::GattCharacteristicRemoved(
    BluetoothGattService* service,
    BluetoothGattCharacteristic* characteristic) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT characteristic removed: " << characteristic->GetIdentifier();

  DCHECK(chrc_id_to_service_id_.find(characteristic->GetIdentifier()) !=
         chrc_id_to_service_id_.end());
  DCHECK(service->GetIdentifier() ==
         chrc_id_to_service_id_[characteristic->GetIdentifier()]);

  chrc_id_to_service_id_.erase(characteristic->GetIdentifier());
}

void BluetoothLowEnergyEventRouter::GattDescriptorAdded(
    BluetoothGattCharacteristic* characteristic,
    BluetoothGattDescriptor* descriptor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT descriptor added: " << descriptor->GetIdentifier();

  DCHECK(desc_id_to_chrc_id_.find(descriptor->GetIdentifier()) ==
         desc_id_to_chrc_id_.end());
  DCHECK(chrc_id_to_service_id_.find(characteristic->GetIdentifier()) !=
         chrc_id_to_service_id_.end());

  desc_id_to_chrc_id_[descriptor->GetIdentifier()] =
      characteristic->GetIdentifier();
}

void BluetoothLowEnergyEventRouter::GattDescriptorRemoved(
    BluetoothGattCharacteristic* characteristic,
    BluetoothGattDescriptor* descriptor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT descriptor removed: " << descriptor->GetIdentifier();

  DCHECK(desc_id_to_chrc_id_.find(descriptor->GetIdentifier()) !=
         desc_id_to_chrc_id_.end());
  DCHECK(characteristic->GetIdentifier() ==
         desc_id_to_chrc_id_[descriptor->GetIdentifier()]);

  desc_id_to_chrc_id_.erase(descriptor->GetIdentifier());
}

void BluetoothLowEnergyEventRouter::GattCharacteristicValueChanged(
    BluetoothGattService* service,
    BluetoothGattCharacteristic* characteristic,
    const std::vector<uint8>& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT characteristic value changed: "
          << characteristic->GetIdentifier();

  DCHECK(observed_gatt_services_.find(service->GetIdentifier()) !=
         observed_gatt_services_.end());
  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) !=
         service_id_to_device_address_.end());
  DCHECK(chrc_id_to_service_id_.find(characteristic->GetIdentifier()) !=
         chrc_id_to_service_id_.end());
  DCHECK(chrc_id_to_service_id_[characteristic->GetIdentifier()] ==
         service->GetIdentifier());

  // Send the event; manually construct the arguments, instead of using
  // apibtle::OnCharacteristicValueChanged::Create, as it doesn't convert
  // lists of enums correctly.
  apibtle::Characteristic api_characteristic;
  PopulateCharacteristic(characteristic, &api_characteristic);
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(apibtle::CharacteristicToValue(&api_characteristic).release());

  DispatchEventToExtensionsWithPermission(
      apibtle::OnCharacteristicValueChanged::kEventName,
      service->GetUUID(),
      characteristic->GetIdentifier(),
      args.Pass());
}

void BluetoothLowEnergyEventRouter::GattDescriptorValueChanged(
    BluetoothGattCharacteristic* characteristic,
    BluetoothGattDescriptor* descriptor,
    const std::vector<uint8>& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT descriptor value changed: " << descriptor->GetIdentifier();

  DCHECK(desc_id_to_chrc_id_.find(descriptor->GetIdentifier()) !=
         desc_id_to_chrc_id_.end());
  DCHECK(characteristic->GetIdentifier() ==
         desc_id_to_chrc_id_[descriptor->GetIdentifier()]);

  // Send the event; manually construct the arguments, instead of using
  // apibtle::OnDescriptorValueChanged::Create, as it doesn't convert
  // lists of enums correctly.
  apibtle::Descriptor api_descriptor;
  PopulateDescriptor(descriptor, &api_descriptor);
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(apibtle::DescriptorToValue(&api_descriptor).release());

  DispatchEventToExtensionsWithPermission(
      apibtle::OnDescriptorValueChanged::kEventName,
      characteristic->GetService()->GetUUID(),
      "" /* characteristic_id */,
      args.Pass());
}

void BluetoothLowEnergyEventRouter::OnGetAdapter(
    const base::Closure& callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;

  // Initialize instance ID mappings for all discovered GATT objects and add
  // observers.
  InitializeIdentifierMappings();
  adapter_->AddObserver(this);

  callback.Run();
}

void BluetoothLowEnergyEventRouter::InitializeIdentifierMappings() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(service_id_to_device_address_.empty());
  DCHECK(chrc_id_to_service_id_.empty());
  DCHECK(observed_devices_.empty());
  DCHECK(observed_gatt_services_.empty());

  // Devices
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (BluetoothAdapter::DeviceList::iterator iter = devices.begin();
       iter != devices.end();
       ++iter) {
    BluetoothDevice* device = *iter;
    device->AddObserver(this);
    observed_devices_.insert(device->GetAddress());

    // Services
    std::vector<BluetoothGattService*> services = device->GetGattServices();
    for (std::vector<BluetoothGattService*>::iterator siter = services.begin();
         siter != services.end();
         ++siter) {
      BluetoothGattService* service = *siter;
      service->AddObserver(this);

      const std::string& service_id = service->GetIdentifier();
      observed_gatt_services_.insert(service_id);
      service_id_to_device_address_[service_id] = device->GetAddress();

      // Characteristics
      const std::vector<BluetoothGattCharacteristic*>& characteristics =
          service->GetCharacteristics();
      for (std::vector<BluetoothGattCharacteristic*>::const_iterator citer =
               characteristics.begin();
           citer != characteristics.end();
           ++citer) {
        BluetoothGattCharacteristic* characteristic = *citer;

        const std::string& chrc_id = characteristic->GetIdentifier();
        chrc_id_to_service_id_[chrc_id] = service_id;

        // Descriptors
        const std::vector<BluetoothGattDescriptor*>& descriptors =
            characteristic->GetDescriptors();
        for (std::vector<BluetoothGattDescriptor*>::const_iterator diter =
                 descriptors.begin();
             diter != descriptors.end();
             ++diter) {
          BluetoothGattDescriptor* descriptor = *diter;

          const std::string& desc_id = descriptor->GetIdentifier();
          desc_id_to_chrc_id_[desc_id] = chrc_id;
        }
      }
    }
  }
}

void BluetoothLowEnergyEventRouter::DispatchEventToExtensionsWithPermission(
    const std::string& event_name,
    const device::BluetoothUUID& uuid,
    const std::string& characteristic_id,
    scoped_ptr<base::ListValue> args) {
  // Obtain the listeners of |event_name|. The list can contain multiple
  // entries for the same extension, so we keep track of the extensions that we
  // already sent the event to, since we want the send an event to an extension
  // only once.
  BluetoothPermissionRequest request(uuid.value());
  std::set<std::string> handled_extensions;
  const EventListenerMap::ListenerList listeners =
      EventRouter::Get(browser_context_)->listeners().GetEventListenersByName(
          event_name);

  for (EventListenerMap::ListenerList::const_iterator iter = listeners.begin();
       iter != listeners.end();
       ++iter) {
    const std::string extension_id = (*iter)->extension_id();
    if (handled_extensions.find(extension_id) != handled_extensions.end())
      continue;

    handled_extensions.insert(extension_id);

    const Extension* extension =
        ExtensionRegistry::Get(browser_context_)
            ->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

    // For all API methods, the "low_energy" permission check is handled by
    // BluetoothLowEnergyExtensionFunction but for events we have to do the
    // check here.
    if (!BluetoothManifestData::CheckRequest(extension, request) ||
        !BluetoothManifestData::CheckLowEnergyPermitted(extension))
      continue;

    // If |event_name| is "onCharacteristicValueChanged", then send the
    // event only if the extension has requested notifications from the
    // related characteristic.
    if (event_name == apibtle::OnCharacteristicValueChanged::kEventName &&
        !characteristic_id.empty() &&
        !FindNotifySession(extension_id, characteristic_id))
      continue;

    // Send the event.
    scoped_ptr<base::ListValue> args_copy(args->DeepCopy());
    scoped_ptr<Event> event(new Event(event_name, args_copy.Pass()));
    EventRouter::Get(browser_context_)->DispatchEventToExtension(
        extension_id, event.Pass());
  }
}

BluetoothGattService* BluetoothLowEnergyEventRouter::FindServiceById(
    const std::string& instance_id) const {
  InstanceIdMap::const_iterator iter =
      service_id_to_device_address_.find(instance_id);
  if (iter == service_id_to_device_address_.end()) {
    VLOG(1) << "GATT service identifier unknown: " << instance_id;
    return NULL;
  }

  const std::string& address = iter->second;

  BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device) {
    VLOG(1) << "Bluetooth device not found: " << address;
    return NULL;
  }

  BluetoothGattService* service = device->GetGattService(instance_id);
  if (!service) {
    VLOG(1) << "GATT service with ID \"" << instance_id
            << "\" not found on device \"" << address << "\"";
    return NULL;
  }

  return service;
}

BluetoothGattCharacteristic*
BluetoothLowEnergyEventRouter::FindCharacteristicById(
    const std::string& instance_id) const {
  InstanceIdMap::const_iterator iter = chrc_id_to_service_id_.find(instance_id);
  if (iter == chrc_id_to_service_id_.end()) {
    VLOG(1) << "GATT characteristic identifier unknown: " << instance_id;
    return NULL;
  }

  const std::string& service_id = iter->second;

  BluetoothGattService* service = FindServiceById(service_id);
  if (!service) {
    VLOG(1) << "Failed to obtain service for characteristic: " << instance_id;
    return NULL;
  }

  BluetoothGattCharacteristic* characteristic =
      service->GetCharacteristic(instance_id);
  if (!characteristic) {
    VLOG(1) << "GATT characteristic with ID \"" << instance_id
            << "\" not found on service \"" << service_id << "\"";
    return NULL;
  }

  return characteristic;
}

BluetoothGattDescriptor* BluetoothLowEnergyEventRouter::FindDescriptorById(
    const std::string& instance_id) const {
  InstanceIdMap::const_iterator iter = desc_id_to_chrc_id_.find(instance_id);
  if (iter == desc_id_to_chrc_id_.end()) {
    VLOG(1) << "GATT descriptor identifier unknown: " << instance_id;
    return NULL;
  }

  const std::string& chrc_id = iter->second;
  BluetoothGattCharacteristic* chrc = FindCharacteristicById(chrc_id);
  if (!chrc) {
    VLOG(1) << "Failed to obtain characteristic for descriptor: "
            << instance_id;
    return NULL;
  }

  BluetoothGattDescriptor* descriptor = chrc->GetDescriptor(instance_id);
  if (!descriptor) {
    VLOG(1) << "GATT descriptor with ID \"" << instance_id
            << "\" not found on characteristic \"" << chrc_id << "\"";
    return NULL;
  }

  return descriptor;
}

void BluetoothLowEnergyEventRouter::OnValueSuccess(
    const base::Closure& callback,
    const std::vector<uint8>& value) {
  VLOG(2) << "Remote characteristic/descriptor value read successful.";
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnCreateGattConnection(
    bool persistent,
    const std::string& extension_id,
    const std::string& device_address,
    const base::Closure& callback,
    scoped_ptr<BluetoothGattConnection> connection) {
  VLOG(2) << "GATT connection created.";
  DCHECK(connection.get());
  DCHECK(!FindConnection(extension_id, device_address));
  DCHECK_EQ(device_address, connection->GetDeviceAddress());

  const std::string connect_id = extension_id + device_address;
  DCHECK_NE(0U, connecting_devices_.count(connect_id));

  BluetoothLowEnergyConnection* conn = new BluetoothLowEnergyConnection(
      persistent, extension_id, connection.Pass());
  ConnectionResourceManager* manager =
      GetConnectionResourceManager(browser_context_);
  manager->Add(conn);

  connecting_devices_.erase(connect_id);
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnDisconnect(
    const std::string& extension_id,
    const std::string& device_address,
    const base::Closure& callback) {
  VLOG(2) << "GATT connection terminated.";

  const std::string disconnect_id = extension_id + device_address;
  DCHECK_NE(0U, disconnecting_devices_.count(disconnect_id));

  if (!RemoveConnection(extension_id, device_address)) {
    VLOG(1) << "The connection was removed before disconnect completed, id: "
            << extension_id << ", device: " << device_address;
  }

  disconnecting_devices_.erase(disconnect_id);
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnError(
    const ErrorCallback& error_callback) {
  VLOG(2) << "Remote characteristic/descriptor value read/write failed.";
  error_callback.Run(kStatusErrorFailed);
}

void BluetoothLowEnergyEventRouter::OnConnectError(
    const std::string& extension_id,
    const std::string& device_address,
    const ErrorCallback& error_callback,
    BluetoothDevice::ConnectErrorCode error_code) {
  VLOG(2) << "Failed to create GATT connection: " << error_code;

  const std::string connect_id = extension_id + device_address;
  DCHECK_NE(0U, connecting_devices_.count(connect_id));

  connecting_devices_.erase(connect_id);
  error_callback.Run(kStatusErrorFailed);
}

void BluetoothLowEnergyEventRouter::OnStartNotifySession(
    bool persistent,
    const std::string& extension_id,
    const std::string& characteristic_id,
    const base::Closure& callback,
    scoped_ptr<device::BluetoothGattNotifySession> session) {
  VLOG(2) << "Value update session created for characteristic: "
          << characteristic_id;
  DCHECK(session.get());
  DCHECK(!FindNotifySession(extension_id, characteristic_id));
  DCHECK_EQ(characteristic_id, session->GetCharacteristicIdentifier());

  const std::string session_id = extension_id + characteristic_id;
  DCHECK_NE(0U, pending_session_calls_.count(session_id));

  BluetoothLowEnergyNotifySession* resource =
      new BluetoothLowEnergyNotifySession(
          persistent, extension_id, session.Pass());

  NotifySessionResourceManager* manager =
      GetNotifySessionResourceManager(browser_context_);
  manager->Add(resource);

  pending_session_calls_.erase(session_id);
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnStartNotifySessionError(
    const std::string& extension_id,
    const std::string& characteristic_id,
    const ErrorCallback& error_callback) {
  VLOG(2) << "Failed to create value update session for characteristic: "
          << characteristic_id;

  const std::string session_id = extension_id + characteristic_id;
  DCHECK_NE(0U, pending_session_calls_.count(session_id));

  pending_session_calls_.erase(session_id);
  error_callback.Run(kStatusErrorFailed);
}

void BluetoothLowEnergyEventRouter::OnStopNotifySession(
    const std::string& extension_id,
    const std::string& characteristic_id,
    const base::Closure& callback) {
  VLOG(2) << "Value update session terminated.";

  if (!RemoveNotifySession(extension_id, characteristic_id)) {
    VLOG(1) << "The value update session was removed before Stop completed, "
            << "id: " << extension_id
            << ", characteristic: " << characteristic_id;
  }

  callback.Run();
}

BluetoothLowEnergyConnection* BluetoothLowEnergyEventRouter::FindConnection(
    const std::string& extension_id,
    const std::string& device_address) {
  ConnectionResourceManager* manager =
      GetConnectionResourceManager(browser_context_);

  base::hash_set<int>* connection_ids = manager->GetResourceIds(extension_id);
  if (!connection_ids)
    return NULL;

  for (base::hash_set<int>::const_iterator iter = connection_ids->begin();
       iter != connection_ids->end();
       ++iter) {
    extensions::BluetoothLowEnergyConnection* conn =
        manager->Get(extension_id, *iter);
    if (!conn)
      continue;

    if (conn->GetConnection()->GetDeviceAddress() == device_address)
      return conn;
  }

  return NULL;
}

bool BluetoothLowEnergyEventRouter::RemoveConnection(
    const std::string& extension_id,
    const std::string& device_address) {
  ConnectionResourceManager* manager =
      GetConnectionResourceManager(browser_context_);

  base::hash_set<int>* connection_ids = manager->GetResourceIds(extension_id);
  if (!connection_ids)
    return false;

  for (base::hash_set<int>::const_iterator iter = connection_ids->begin();
       iter != connection_ids->end();
       ++iter) {
    extensions::BluetoothLowEnergyConnection* conn =
        manager->Get(extension_id, *iter);
    if (!conn || conn->GetConnection()->GetDeviceAddress() != device_address)
      continue;

    manager->Remove(extension_id, *iter);
    return true;
  }

  return false;
}

BluetoothLowEnergyNotifySession*
BluetoothLowEnergyEventRouter::FindNotifySession(
    const std::string& extension_id,
    const std::string& characteristic_id) {
  NotifySessionResourceManager* manager =
      GetNotifySessionResourceManager(browser_context_);

  base::hash_set<int>* ids = manager->GetResourceIds(extension_id);
  if (!ids)
    return NULL;

  for (base::hash_set<int>::const_iterator iter = ids->begin();
       iter != ids->end();
       ++iter) {
    BluetoothLowEnergyNotifySession* session =
        manager->Get(extension_id, *iter);
    if (!session)
      continue;

    if (session->GetSession()->GetCharacteristicIdentifier() ==
        characteristic_id)
      return session;
  }

  return NULL;
}

bool BluetoothLowEnergyEventRouter::RemoveNotifySession(
    const std::string& extension_id,
    const std::string& characteristic_id) {
  NotifySessionResourceManager* manager =
      GetNotifySessionResourceManager(browser_context_);

  base::hash_set<int>* ids = manager->GetResourceIds(extension_id);
  if (!ids)
    return false;

  for (base::hash_set<int>::const_iterator iter = ids->begin();
       iter != ids->end();
       ++iter) {
    BluetoothLowEnergyNotifySession* session =
        manager->Get(extension_id, *iter);
    if (!session ||
        session->GetSession()->GetCharacteristicIdentifier() !=
            characteristic_id)
      continue;

    manager->Remove(extension_id, *iter);
    return true;
  }

  return false;
}

}  // namespace extensions
