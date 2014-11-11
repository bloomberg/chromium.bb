// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/device_permissions_manager.h"

#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "device/usb/usb_ids.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

using content::BrowserContext;
using device::UsbDevice;
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

// The manufacturer string read from the device that the app has permission to
// access.
const char kDeviceManufacturerString[] = "manufacturer_string";

// The product string read from the device that the app has permission to
// access.
const char kDeviceProductString[] = "product_string";

// Persists a DevicePermissionEntry in ExtensionPrefs.
void SaveDevicePermissionEntry(BrowserContext* context,
                               const std::string& extension_id,
                               const DevicePermissionEntry& device) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  ExtensionPrefs::ScopedListUpdate update(prefs, extension_id, kDevices);
  base::ListValue* devices = update.Get();
  if (!devices) {
    devices = update.Create();
  }

  base::Value* device_entry = device.ToValue();
  DCHECK(devices->Find(*device_entry) == devices->end());
  devices->Append(device_entry);
}

// Clears all DevicePermissionEntries for the app from ExtensionPrefs.
void ClearDevicePermissionEntries(ExtensionPrefs* prefs,
                                  const std::string& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kDevices, NULL);
}

// Returns all DevicePermissionEntries for the app.
std::vector<DevicePermissionEntry> GetDevicePermissionEntries(
    ExtensionPrefs* prefs,
    const std::string& extension_id) {
  std::vector<DevicePermissionEntry> result;
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

    base::string16 manufacturer_string;
    // Ignore failure as this string is optional.
    device_entry->GetStringWithoutPathExpansion(kDeviceManufacturerString,
                                                &manufacturer_string);

    base::string16 product_string;
    // Ignore failure as this string is optional.
    device_entry->GetStringWithoutPathExpansion(kDeviceProductString,
                                                &product_string);

    result.push_back(DevicePermissionEntry(vendor_id,
                                           product_id,
                                           serial_number,
                                           manufacturer_string,
                                           product_string));
  }
  return result;
}

}  // namespace

DevicePermissionEntry::DevicePermissionEntry(
    uint16_t vendor_id,
    uint16_t product_id,
    const base::string16& serial_number,
    const base::string16& manufacturer_string,
    const base::string16& product_string)
    : vendor_id(vendor_id),
      product_id(product_id),
      serial_number(serial_number),
      manufacturer_string(manufacturer_string),
      product_string(product_string) {
}

base::Value* DevicePermissionEntry::ToValue() const {
  base::DictionaryValue* device_entry_dict = new base::DictionaryValue();
  device_entry_dict->SetStringWithoutPathExpansion(kDeviceType, kDeviceTypeUsb);
  device_entry_dict->SetIntegerWithoutPathExpansion(kDeviceVendorId, vendor_id);
  device_entry_dict->SetIntegerWithoutPathExpansion(kDeviceProductId,
                                                    product_id);
  DCHECK(!serial_number.empty());
  device_entry_dict->SetStringWithoutPathExpansion(kDeviceSerialNumber,
                                                   serial_number);
  if (!manufacturer_string.empty()) {
    device_entry_dict->SetStringWithoutPathExpansion(kDeviceManufacturerString,
                                                     manufacturer_string);
  }
  if (!product_string.empty()) {
    device_entry_dict->SetStringWithoutPathExpansion(kDeviceProductString,
                                                     product_string);
  }
  return device_entry_dict;
}

DevicePermissions::~DevicePermissions() {
}

bool DevicePermissions::CheckUsbDevice(
    scoped_refptr<device::UsbDevice> device) const {
  if (ephemeral_devices_.find(device) != ephemeral_devices_.end()) {
    return true;
  }

  bool have_serial_number = false;
  base::string16 serial_number;
  for (const auto& entry : permission_entries_) {
    if (entry.vendor_id != device->vendor_id()) {
      continue;
    }
    if (entry.product_id != device->product_id()) {
      continue;
    }
    if (!have_serial_number) {
      if (!device->GetSerialNumber(&serial_number)) {
        break;
      }
      have_serial_number = true;
    }
    if (entry.serial_number != serial_number) {
      continue;
    }
    return true;
  }
  return false;
}

DevicePermissions::DevicePermissions(BrowserContext* context,
                                     const std::string& extension_id) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  permission_entries_ = GetDevicePermissionEntries(prefs, extension_id);
}

DevicePermissions::DevicePermissions(
    const std::vector<DevicePermissionEntry>& permission_entries,
    const std::set<scoped_refptr<device::UsbDevice>>& ephemeral_devices)
    : permission_entries_(permission_entries),
      ephemeral_devices_(ephemeral_devices) {
}

std::vector<DevicePermissionEntry>& DevicePermissions::permission_entries() {
  return permission_entries_;
}

std::set<scoped_refptr<device::UsbDevice>>&
DevicePermissions::ephemeral_devices() {
  return ephemeral_devices_;
}

// static
DevicePermissionsManager* DevicePermissionsManager::Get(
    BrowserContext* context) {
  return DevicePermissionsManagerFactory::GetForBrowserContext(context);
}

scoped_ptr<DevicePermissions> DevicePermissionsManager::GetForExtension(
    const std::string& extension_id) {
  DCHECK(CalledOnValidThread());

  DevicePermissions* device_permissions = GetOrInsert(extension_id);
  return make_scoped_ptr(
      new DevicePermissions(device_permissions->permission_entries(),
                            device_permissions->ephemeral_devices()));
}

std::vector<base::string16>
DevicePermissionsManager::GetPermissionMessageStrings(
    const std::string& extension_id) {
  DCHECK(CalledOnValidThread());

  std::vector<base::string16> messages;
  DevicePermissions* device_permissions = Get(extension_id);
  if (!device_permissions) {
    return messages;
  }

  for (const auto& entry : device_permissions->permission_entries()) {
    base::string16 manufacturer_string = entry.manufacturer_string;
    if (manufacturer_string.empty()) {
      const char* vendor_name = device::UsbIds::GetVendorName(entry.vendor_id);
      if (vendor_name) {
        manufacturer_string = base::UTF8ToUTF16(vendor_name);
      } else {
        base::string16 vendor_id =
            base::ASCIIToUTF16(base::StringPrintf("0x%04x", entry.vendor_id));
        manufacturer_string =
            l10n_util::GetStringFUTF16(IDS_DEVICE_UNKNOWN_VENDOR, vendor_id);
      }
    }

    base::string16 product_string = entry.product_string;
    if (product_string.empty()) {
      const char* product_name =
          device::UsbIds::GetProductName(entry.vendor_id, entry.product_id);
      if (product_name) {
        product_string = base::UTF8ToUTF16(product_name);
      } else {
        base::string16 product_id =
            base::ASCIIToUTF16(base::StringPrintf("0x%04x", entry.product_id));
        product_string =
            l10n_util::GetStringFUTF16(IDS_DEVICE_UNKNOWN_PRODUCT, product_id);
      }
    }

    messages.push_back(l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_SERIAL,
        product_string,
        manufacturer_string,
        entry.serial_number));
  }
  return messages;
}

void DevicePermissionsManager::AllowUsbDevice(
    const std::string& extension_id,
    scoped_refptr<device::UsbDevice> device,
    const base::string16& product_string,
    const base::string16& manufacturer_string,
    const base::string16& serial_number) {
  DCHECK(CalledOnValidThread());
  DevicePermissions* device_permissions = GetOrInsert(extension_id);

  if (!serial_number.empty()) {
    for (const auto& entry : device_permissions->permission_entries()) {
      if (entry.vendor_id != device->vendor_id()) {
        continue;
      }
      if (entry.product_id != device->product_id()) {
        continue;
      }
      if (entry.serial_number == serial_number) {
        return;
      }
    }

    DevicePermissionEntry device_entry =
        DevicePermissionEntry(device->vendor_id(),
                              device->product_id(),
                              serial_number,
                              manufacturer_string,
                              product_string);
    device_permissions->permission_entries().push_back(device_entry);
    SaveDevicePermissionEntry(context_, extension_id, device_entry);
  } else {
    // Without a serial number a device cannot be reliably identified when it
    // is reconnected so such devices are only remembered until disconnect.
    // Register an observer here so that this set doesn't grow undefinitely.
    device_permissions->ephemeral_devices().insert(device);
    device->AddObserver(this);
  }
}

void DevicePermissionsManager::Clear(const std::string& extension_id) {
  DCHECK(CalledOnValidThread());

  ClearDevicePermissionEntries(ExtensionPrefs::Get(context_), extension_id);
  std::map<std::string, DevicePermissions*>::iterator it =
      extension_id_to_device_permissions_.find(extension_id);
  if (it != extension_id_to_device_permissions_.end()) {
    delete it->second;
    extension_id_to_device_permissions_.erase(it);
  }
}

DevicePermissionsManager::DevicePermissionsManager(
    content::BrowserContext* context)
    : context_(context), process_manager_observer_(this) {
  process_manager_observer_.Add(ProcessManager::Get(context));
}

DevicePermissionsManager::~DevicePermissionsManager() {
  for (const auto& map_entry : extension_id_to_device_permissions_) {
    delete map_entry.second;
  }
}

DevicePermissions* DevicePermissionsManager::Get(
    const std::string& extension_id) const {
  std::map<std::string, DevicePermissions*>::const_iterator it =
      extension_id_to_device_permissions_.find(extension_id);
  if (it != extension_id_to_device_permissions_.end()) {
    return it->second;
  }

  return NULL;
}

DevicePermissions* DevicePermissionsManager::GetOrInsert(
    const std::string& extension_id) {
  DevicePermissions* device_permissions = Get(extension_id);
  if (!device_permissions) {
    device_permissions = new DevicePermissions(context_, extension_id);
    extension_id_to_device_permissions_[extension_id] = device_permissions;
  }

  return device_permissions;
}

void DevicePermissionsManager::OnBackgroundHostClose(
    const std::string& extension_id) {
  DCHECK(CalledOnValidThread());

  DevicePermissions* device_permissions = Get(extension_id);
  if (device_permissions) {
    // When all of the app's windows are closed and the background page is
    // suspended all ephemeral device permissions are cleared.
    for (std::set<scoped_refptr<UsbDevice>>::iterator it =
             device_permissions->ephemeral_devices().begin();
         it != device_permissions->ephemeral_devices().end();
         ++it) {
      (*it)->RemoveObserver(this);
    }
    device_permissions->ephemeral_devices().clear();
  }
}

void DevicePermissionsManager::OnDisconnect(scoped_refptr<UsbDevice> device) {
  for (const auto& map_entry : extension_id_to_device_permissions_) {
    // An ephemeral device cannot be identified if it is reconnected and so
    // permission to access it is cleared on disconnect.
    map_entry.second->ephemeral_devices().erase(device);
    device->RemoveObserver(this);
  }
}

// static
DevicePermissionsManager* DevicePermissionsManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DevicePermissionsManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
DevicePermissionsManagerFactory*
DevicePermissionsManagerFactory::GetInstance() {
  return Singleton<DevicePermissionsManagerFactory>::get();
}

DevicePermissionsManagerFactory::DevicePermissionsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "DevicePermissionsManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProcessManagerFactory::GetInstance());
}

DevicePermissionsManagerFactory::~DevicePermissionsManagerFactory() {
}

KeyedService* DevicePermissionsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DevicePermissionsManager(context);
}

BrowserContext* DevicePermissionsManagerFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Return the original (possibly off-the-record) browser context so that a
  // separate instance of the DevicePermissionsManager is used in incognito
  // mode. The parent class's implemenation returns NULL.
  return context;
}

}  // namespace extensions
