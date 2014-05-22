// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/utils.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "extensions/browser/event_router.h"

using content::BrowserThread;

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothGattCharacteristic;
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
  if (properties & BluetoothGattCharacteristic::kPropertyWriteableAuxiliaries) {
    api_properties->push_back(
        apibtle::CHARACTERISTIC_PROPERTY_WRITEABLEAUXILIARIES);
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

bool BluetoothLowEnergyEventRouter::GetIncludedServices(
    const std::string& instance_id,
    ServiceList* out_services) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(out_services);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return false;
  }

  BluetoothGattService* service = FindServiceById(instance_id);
  if (!service) {
    VLOG(1) << "Service not found: " << instance_id;
    return false;
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

  return true;
}

bool BluetoothLowEnergyEventRouter::GetCharacteristics(
    const std::string& instance_id,
    CharacteristicList* out_characteristics) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(out_characteristics);
  if (!adapter_) {
    VLOG(1) << "BlutoothAdapter not ready.";
    return false;
  }

  BluetoothGattService* service = FindServiceById(instance_id);
  if (!service) {
    VLOG(1) << "Service not found: " << instance_id;
    return false;
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

  return true;
}

bool BluetoothLowEnergyEventRouter::GetCharacteristic(
    const std::string& instance_id,
    apibtle::Characteristic* out_characteristic) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(out_characteristic);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return false;
  }

  BluetoothGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    return false;
  }

  PopulateCharacteristic(characteristic, out_characteristic);
  return true;
}

bool BluetoothLowEnergyEventRouter::GetDescriptors(
    const std::string& instance_id,
    DescriptorList* out_descriptors) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(out_descriptors);
  if (!adapter_) {
    VLOG(1) << "BlutoothAdapter not ready.";
    return false;
  }

  BluetoothGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    return false;
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

  return true;
}

bool BluetoothLowEnergyEventRouter::GetDescriptor(
    const std::string& instance_id,
    api::bluetooth_low_energy::Descriptor* out_descriptor) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(out_descriptor);
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return false;
  }

  BluetoothGattDescriptor* descriptor = FindDescriptorById(instance_id);
  if (!descriptor) {
    VLOG(1) << "Descriptor not found: " << instance_id;
    return false;
  }

  PopulateDescriptor(descriptor, out_descriptor);
  return true;
}

bool BluetoothLowEnergyEventRouter::ReadCharacteristicValue(
    const std::string& instance_id,
    const base::Closure& callback,
    const base::Closure& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return false;
  }

  BluetoothGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    return false;
  }

  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothLowEnergyEventRouter::ValueCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      error_callback);

  return true;
}

bool BluetoothLowEnergyEventRouter::WriteCharacteristicValue(
    const std::string& instance_id,
    const std::vector<uint8>& value,
    const base::Closure& callback,
    const base::Closure& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!adapter_) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return false;
  }

  BluetoothGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    return false;
  }

  characteristic->WriteRemoteCharacteristic(value, callback, error_callback);
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

  scoped_ptr<base::ListValue> args =
      apibtle::OnServiceChanged::Create(api_service);
  scoped_ptr<Event> event(
      new Event(apibtle::OnServiceChanged::kEventName, args.Pass()));
  EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
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

  // Signal API event.
  apibtle::Characteristic api_characteristic;
  PopulateCharacteristic(characteristic, &api_characteristic);

  // Manually construct the arguments, instead of using
  // apibtle::OnCharacteristicValueChanged::Create, as it doesn't convert lists
  // of enums correctly.
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(apibtle::CharacteristicToValue(&api_characteristic).release());
  scoped_ptr<Event> event(new Event(
      apibtle::OnCharacteristicValueChanged::kEventName, args.Pass()));
  EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

void BluetoothLowEnergyEventRouter::GattDescriptorValueChanged(
    BluetoothGattCharacteristic* characteristic,
    BluetoothGattDescriptor* descriptor,
    const std::vector<uint8>& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "GATT descriptor value changed: " << descriptor->GetIdentifier();
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

void BluetoothLowEnergyEventRouter::ValueCallback(
    const base::Closure& callback,
    const std::vector<uint8>& value) {
  VLOG(2) << "Remote characteristic value read successful.";
  callback.Run();
}

}  // namespace extensions
