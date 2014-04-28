// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "extensions/browser/event_router.h"

using content::BrowserThread;

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothGattCharacteristic;
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

}  // namespace

namespace extensions {

BluetoothLowEnergyEventRouter::GattObjectData::GattObjectData() {
}
BluetoothLowEnergyEventRouter::GattObjectData::~GattObjectData() {
}

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
    InstanceIdToObjectDataMap::const_iterator id_iter =
        service_ids_to_objects_.find(*iter);
    if (id_iter == service_ids_to_objects_.end())
      continue;

    GattObjectData data = id_iter->second;
    BluetoothDevice* device = adapter_->GetDevice(data.device_address);
    if (!device)
      continue;

    BluetoothGattService* service = device->GetGattService(data.service_id);
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

bool BluetoothLowEnergyEventRouter::GetService(
    const std::string& instance_id,
    apibtle::Service* service) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return false;
  }

  BluetoothGattService* gatt_service = FindServiceById(instance_id);
  if (!gatt_service) {
    VLOG(1) << "Service not found: " << instance_id;
    return false;
  }

  PopulateService(gatt_service, service);
  return true;
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
  DCHECK(service_ids_to_objects_.find(service->GetIdentifier()) ==
         service_ids_to_objects_.end());

  service->AddObserver(this);
  observed_gatt_services_.insert(service->GetIdentifier());

  GattObjectData data;
  data.device_address = device->GetAddress();
  data.service_id = service->GetIdentifier();
  service_ids_to_objects_[data.service_id] = data;

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
  DCHECK(service_ids_to_objects_.find(service->GetIdentifier()) !=
         service_ids_to_objects_.end());

  service->RemoveObserver(this);
  observed_gatt_services_.erase(service->GetIdentifier());

  DCHECK(device->GetAddress() ==
         service_ids_to_objects_[service->GetIdentifier()].device_address);
  service_ids_to_objects_.erase(service->GetIdentifier());

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
  // TODO
}

void BluetoothLowEnergyEventRouter::GattCharacteristicAdded(
    BluetoothGattService* service,
    BluetoothGattCharacteristic* characteristic) {
  // TODO(armansito): Implement.
}

void BluetoothLowEnergyEventRouter::GattCharacteristicRemoved(
    BluetoothGattService* service,
    BluetoothGattCharacteristic* characteristic) {
  // TODO(armansito): Implement.
}

void BluetoothLowEnergyEventRouter::GattCharacteristicValueChanged(
    BluetoothGattService* service,
    BluetoothGattCharacteristic* characteristic,
    const std::vector<uint8>& value) {
  // TODO(armansito): Implement.
}

void BluetoothLowEnergyEventRouter::OnGetAdapter(
    const base::Closure& callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(service_ids_to_objects_.empty());
  DCHECK(observed_devices_.empty());
  DCHECK(observed_gatt_services_.empty());

  // TODO: Observe adapter and all devices.
  adapter_ = adapter;

  // Initialize instance ID mappings for all discovered GATT objects and add
  // observers.
  InitializeIdentifierMappings();
  adapter_->AddObserver(this);

  callback.Run();
}

void BluetoothLowEnergyEventRouter::InitializeIdentifierMappings() {
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (BluetoothAdapter::DeviceList::iterator iter = devices.begin();
       iter != devices.end();
       ++iter) {
    BluetoothDevice* device = *iter;
    device->AddObserver(this);
    observed_devices_.insert(device->GetAddress());

    std::vector<BluetoothGattService*> services = device->GetGattServices();
    for (std::vector<BluetoothGattService*>::iterator siter = services.begin();
         siter != services.end();
         ++siter) {
      BluetoothGattService* service = *siter;
      service->AddObserver(this);
      observed_gatt_services_.insert(service->GetIdentifier());

      GattObjectData service_data;
      service_data.device_address = device->GetAddress();
      service_data.service_id = service->GetIdentifier();
      service_ids_to_objects_[service_data.service_id] = service_data;

      // TODO(armansito): Initialize mapping for characteristics & descriptors.
    }
  }
}

BluetoothGattService* BluetoothLowEnergyEventRouter::FindServiceById(
    const std::string& instance_id) const {
  InstanceIdToObjectDataMap::const_iterator iter =
      service_ids_to_objects_.find(instance_id);
  if (iter == service_ids_to_objects_.end()) {
    VLOG(1) << "GATT service identifier unknown: " << instance_id;
    return NULL;
  }

  GattObjectData data = iter->second;
  DCHECK(!data.device_address.empty());
  DCHECK(!data.service_id.empty());
  DCHECK(data.characteristic_id.empty());
  DCHECK(data.descriptor_id.empty());

  BluetoothDevice* device = adapter_->GetDevice(data.device_address);
  if (!device) {
    VLOG(1) << "Bluetooth device not found: " << data.device_address;
    return NULL;
  }

  BluetoothGattService* service = device->GetGattService(data.service_id);
  if (!service) {
    VLOG(1) << "GATT service with ID \"" << data.service_id
            << "\" not found on device \"" << data.device_address << "\"";
    return NULL;
  }

  return service;
}

}  // namespace extensions
