// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_devices_service.h"

#include <set>
#include <vector>

#include "apps/saved_devices_service_factory.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace apps {

using device::UsbDevice;
using device::UsbDeviceHandle;
using extensions::APIPermission;
using extensions::Extension;
using extensions::ExtensionHost;
using extensions::ExtensionPrefs;

namespace {

// Preference keys

// The device that the app has permission to access.
const char kDevices[] = "devices";

// The type of device saved.
const char kDeviceType[] = "type";

// Type identifier for USB devices.
const char kDeviceTypeUsb[] = "usb";

// The vendor ID of the device that the app had permission to access.
const char kDeviceVendorId[] = "vendor_id";

// The product ID of the device that the app had permission to access.
const char kDeviceProductId[] = "product_id";

// The serial number of the device that the app has permission to access.
const char kDeviceSerialNumber[] = "serial_number";

// Persists a SavedDeviceEntry in ExtensionPrefs.
void AddSavedDeviceEntry(Profile* profile,
                         const std::string& extension_id,
                         const SavedDeviceEntry& device) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile);
  ExtensionPrefs::ScopedListUpdate update(prefs, extension_id, kDevices);
  base::ListValue* devices = update.Get();
  if (!devices) {
    devices = update.Create();
  }

  base::Value* device_entry = device.ToValue();
  DCHECK(devices->Find(*device_entry) == devices->end());
  devices->Append(device_entry);
}

// Clears all SavedDeviceEntry for the app from ExtensionPrefs.
void ClearSavedDeviceEntries(ExtensionPrefs* prefs,
                             const std::string& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kDevices, NULL);
}

// Returns all SavedDeviceEntries for the app.
std::vector<SavedDeviceEntry> GetSavedDeviceEntries(
    ExtensionPrefs* prefs,
    const std::string& extension_id) {
  std::vector<SavedDeviceEntry> result;
  const base::ListValue* devices = NULL;
  if (!prefs->ReadPrefAsList(extension_id, kDevices, &devices)) {
    return result;
  }

  for (base::ListValue::const_iterator it = devices->begin();
       it != devices->end();
       ++it) {
    const base::DictionaryValue* device_entry = NULL;
    if (!(*it)->GetAsDictionary(&device_entry)) {
      continue;
    }
    int vendor_id;
    if (!device_entry->GetIntegerWithoutPathExpansion(kDeviceVendorId,
                                                      &vendor_id) ||
        vendor_id < 0 || vendor_id > UINT16_MAX) {
      continue;
    }
    int product_id;
    if (!device_entry->GetIntegerWithoutPathExpansion(kDeviceProductId,
                                                      &product_id) ||
        product_id < 0 || product_id > UINT16_MAX) {
      continue;
    }
    base::string16 serial_number;
    if (!device_entry->GetStringWithoutPathExpansion(kDeviceSerialNumber,
                                                     &serial_number)) {
      continue;
    }

    result.push_back(SavedDeviceEntry(vendor_id, product_id, serial_number));
  }
  return result;
}

}  // namespace

SavedDeviceEntry::SavedDeviceEntry(uint16_t vendor_id,
                                   uint16_t product_id,
                                   const base::string16& serial_number)
    : vendor_id(vendor_id),
      product_id(product_id),
      serial_number(serial_number) {
}

base::Value* SavedDeviceEntry::ToValue() const {
  base::DictionaryValue* device_entry_dict = new base::DictionaryValue();
  device_entry_dict->SetStringWithoutPathExpansion(kDeviceType, kDeviceTypeUsb);
  device_entry_dict->SetIntegerWithoutPathExpansion(kDeviceVendorId, vendor_id);
  device_entry_dict->SetIntegerWithoutPathExpansion(kDeviceProductId,
                                                    product_id);
  device_entry_dict->SetStringWithoutPathExpansion(kDeviceSerialNumber,
                                                   serial_number);
  return device_entry_dict;
}

bool SavedDevicesService::SavedDevices::IsRegistered(
    scoped_refptr<UsbDevice> device) const {
  if (ephemeral_devices_.find(device) != ephemeral_devices_.end()) {
    return true;
  }

  bool have_serial_number = false;
  base::string16 serial_number;
  for (std::vector<SavedDeviceEntry>::const_iterator it =
           persistent_devices_.begin();
       it != persistent_devices_.end();
       ++it) {
    if (it->vendor_id != device->vendor_id()) {
      continue;
    }
    if (it->product_id != device->product_id()) {
      continue;
    }
    if (!have_serial_number) {
      if (!device->GetSerialNumber(&serial_number)) {
        break;
      }
      have_serial_number = true;
    }
    if (it->serial_number != serial_number) {
      continue;
    }
    return true;
  }
  return false;
}

void SavedDevicesService::SavedDevices::RegisterDevice(
    scoped_refptr<device::UsbDevice> device,
    base::string16* serial_number) {
  if (serial_number) {
    for (std::vector<SavedDeviceEntry>::const_iterator it =
             persistent_devices_.begin();
         it != persistent_devices_.end();
         ++it) {
      if (it->vendor_id != device->vendor_id()) {
        continue;
      }
      if (it->product_id != device->product_id()) {
        continue;
      }
      if (it->serial_number == *serial_number) {
        return;
      }
    }
    SavedDeviceEntry device_entry = SavedDeviceEntry(
        device->vendor_id(), device->product_id(), *serial_number);
    persistent_devices_.push_back(device_entry);

    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(AddSavedDeviceEntry, profile_, extension_id_, device_entry));
  } else {
    // Without a serial number a device cannot be reliably identified when it
    // is reconnected so such devices are only remembered until disconnect.
    // Register an observer here so that this set doesn't grow undefinitely.
    ephemeral_devices_.insert(device);
    device->AddObserver(this);
  }
}

SavedDevicesService::SavedDevices::SavedDevices(Profile* profile,
                                                const std::string& extension_id)
    : profile_(profile), extension_id_(extension_id) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  persistent_devices_ = GetSavedDeviceEntries(prefs, extension_id_);
}

SavedDevicesService::SavedDevices::~SavedDevices() {
  // Only ephemeral devices have an observer registered.
  for (std::set<scoped_refptr<UsbDevice> >::iterator it =
           ephemeral_devices_.begin();
       it != ephemeral_devices_.end();
       ++it) {
    (*it)->RemoveObserver(this);
  }
}

void SavedDevicesService::SavedDevices::OnDisconnect(
    scoped_refptr<UsbDevice> device) {
  // Permission for an ephemeral device lasts only as long as the device is
  // plugged in.
  ephemeral_devices_.erase(device);
  device->RemoveObserver(this);
}

SavedDevicesService::SavedDevicesService(Profile* profile) : profile_(profile) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

SavedDevicesService::~SavedDevicesService() {
  for (std::map<std::string, SavedDevices*>::iterator it =
           extension_id_to_saved_devices_.begin();
       it != extension_id_to_saved_devices_.end();
       ++it) {
    delete it->second;
  }
}

// static
SavedDevicesService* SavedDevicesService::Get(Profile* profile) {
  return SavedDevicesServiceFactory::GetForProfile(profile);
}

SavedDevicesService::SavedDevices* SavedDevicesService::GetOrInsert(
    const std::string& extension_id) {
  SavedDevices* saved_devices = Get(extension_id);
  if (saved_devices) {
    return saved_devices;
  }

  saved_devices = new SavedDevices(profile_, extension_id);
  extension_id_to_saved_devices_[extension_id] = saved_devices;
  return saved_devices;
}

std::vector<SavedDeviceEntry> SavedDevicesService::GetAllDevices(
    const std::string& extension_id) const {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  return GetSavedDeviceEntries(prefs, extension_id);
}

void SavedDevicesService::Clear(const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ClearSavedDeviceEntries(ExtensionPrefs::Get(profile_), extension_id);
  std::map<std::string, SavedDevices*>::iterator it =
      extension_id_to_saved_devices_.find(extension_id);
  if (it != extension_id_to_saved_devices_.end()) {
    delete it->second;
    extension_id_to_saved_devices_.erase(it);
  }
}

void SavedDevicesService::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      Clear(content::Details<ExtensionHost>(details)->extension_id());
      break;
    }

    case chrome::NOTIFICATION_APP_TERMINATING: {
      // Stop listening to NOTIFICATION_EXTENSION_HOST_DESTROYED in particular
      // as all extension hosts will be destroyed as a result of shutdown.
      registrar_.RemoveAll();
      break;
    }
  }
}

SavedDevicesService::SavedDevices* SavedDevicesService::Get(
    const std::string& extension_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::map<std::string, SavedDevices*>::const_iterator it =
      extension_id_to_saved_devices_.find(extension_id);
  if (it != extension_id_to_saved_devices_.end()) {
    return it->second;
  }

  return NULL;
}

}  // namespace apps
