// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/device_permissions_manager.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "device/core/device_client.h"
#include "device/usb/usb_device.h"
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
using content::BrowserThread;
using device::UsbDevice;
using device::UsbService;
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

// Serialized timestamp of the last time when the device was opened by the app.
const char kDeviceLastUsed[] = "last_used_time";

// Persists a DevicePermissionEntry in ExtensionPrefs.
void SaveDevicePermissionEntry(BrowserContext* context,
                               const std::string& extension_id,
                               scoped_refptr<DevicePermissionEntry> entry) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  ExtensionPrefs::ScopedListUpdate update(prefs, extension_id, kDevices);
  base::ListValue* devices = update.Get();
  if (!devices) {
    devices = update.Create();
  }

  scoped_ptr<base::Value> device_entry(entry->ToValue());
  DCHECK(devices->Find(*device_entry.get()) == devices->end());
  devices->Append(device_entry.release());
}

bool MatchesDevicePermissionEntry(const base::DictionaryValue* value,
                                  scoped_refptr<DevicePermissionEntry> entry) {
  std::string type;
  if (!value->GetStringWithoutPathExpansion(kDeviceType, &type) ||
      type != kDeviceTypeUsb) {
    return false;
  }
  int vendor_id;
  if (!value->GetIntegerWithoutPathExpansion(kDeviceVendorId, &vendor_id) ||
      vendor_id != entry->vendor_id()) {
    return false;
  }
  int product_id;
  if (!value->GetIntegerWithoutPathExpansion(kDeviceProductId, &product_id) ||
      product_id != entry->product_id()) {
    return false;
  }
  base::string16 serial_number;
  if (!value->GetStringWithoutPathExpansion(kDeviceSerialNumber,
                                            &serial_number) ||
      serial_number != entry->serial_number()) {
    return false;
  }
  return true;
}

// Updates the timestamp stored in ExtensionPrefs for the given
// DevicePermissionEntry.
void UpdateDevicePermissionEntry(BrowserContext* context,
                                 const std::string& extension_id,
                                 scoped_refptr<DevicePermissionEntry> entry) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  ExtensionPrefs::ScopedListUpdate update(prefs, extension_id, kDevices);
  base::ListValue* devices = update.Get();
  if (!devices) {
    return;
  }

  for (size_t i = 0; i < devices->GetSize(); ++i) {
    base::DictionaryValue* dict_value;
    if (!devices->GetDictionary(i, &dict_value)) {
      continue;
    }
    if (!MatchesDevicePermissionEntry(dict_value, entry)) {
      continue;
    }
    devices->Set(i, entry->ToValue().release());
    break;
  }
}

// Removes the given DevicePermissionEntry from ExtensionPrefs.
void RemoveDevicePermissionEntry(BrowserContext* context,
                                 const std::string& extension_id,
                                 scoped_refptr<DevicePermissionEntry> entry) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  ExtensionPrefs::ScopedListUpdate update(prefs, extension_id, kDevices);
  base::ListValue* devices = update.Get();
  if (!devices) {
    return;
  }

  for (size_t i = 0; i < devices->GetSize(); ++i) {
    base::DictionaryValue* dict_value;
    if (!devices->GetDictionary(i, &dict_value)) {
      continue;
    }
    if (!MatchesDevicePermissionEntry(dict_value, entry)) {
      continue;
    }
    devices->Remove(i, nullptr);
    break;
  }
}

// Clears all DevicePermissionEntries for the app from ExtensionPrefs.
void ClearDevicePermissionEntries(ExtensionPrefs* prefs,
                                  const std::string& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kDevices, NULL);
}

// Returns all DevicePermissionEntries for the app.
std::set<scoped_refptr<DevicePermissionEntry>> GetDevicePermissionEntries(
    ExtensionPrefs* prefs,
    const std::string& extension_id) {
  std::set<scoped_refptr<DevicePermissionEntry>> result;
  const base::ListValue* devices = NULL;
  if (!prefs->ReadPrefAsList(extension_id, kDevices, &devices)) {
    return result;
  }

  for (const base::Value* entry : *devices) {
    const base::DictionaryValue* entry_dict;
    if (!entry->GetAsDictionary(&entry_dict)) {
      continue;
    }
    std::string type;
    if (!entry_dict->GetStringWithoutPathExpansion(kDeviceType, &type) ||
        type != kDeviceTypeUsb) {
      continue;
    }
    int vendor_id;
    if (!entry_dict->GetIntegerWithoutPathExpansion(kDeviceVendorId,
                                                    &vendor_id) ||
        vendor_id < 0 || vendor_id > UINT16_MAX) {
      continue;
    }
    int product_id;
    if (!entry_dict->GetIntegerWithoutPathExpansion(kDeviceProductId,
                                                    &product_id) ||
        product_id < 0 || product_id > UINT16_MAX) {
      continue;
    }
    base::string16 serial_number;
    if (!entry_dict->GetStringWithoutPathExpansion(kDeviceSerialNumber,
                                                   &serial_number)) {
      continue;
    }

    base::string16 manufacturer_string;
    // Ignore failure as this string is optional.
    entry_dict->GetStringWithoutPathExpansion(kDeviceManufacturerString,
                                              &manufacturer_string);

    base::string16 product_string;
    // Ignore failure as this string is optional.
    entry_dict->GetStringWithoutPathExpansion(kDeviceProductString,
                                              &product_string);

    // If a last used time is not stored in ExtensionPrefs last_used.is_null()
    // will be true.
    std::string last_used_str;
    int64 last_used_i64 = 0;
    base::Time last_used;
    if (entry_dict->GetStringWithoutPathExpansion(kDeviceLastUsed,
                                                  &last_used_str) &&
        base::StringToInt64(last_used_str, &last_used_i64)) {
      last_used = base::Time::FromInternalValue(last_used_i64);
    }

    result.insert(new DevicePermissionEntry(vendor_id, product_id,
                                            serial_number, manufacturer_string,
                                            product_string, last_used));
  }
  return result;
}

}  // namespace

DevicePermissionEntry::DevicePermissionEntry(
    scoped_refptr<device::UsbDevice> device,
    const base::string16& serial_number,
    const base::string16& manufacturer_string,
    const base::string16& product_string)
    : device_(device),
      vendor_id_(device->vendor_id()),
      product_id_(device->product_id()),
      serial_number_(serial_number),
      manufacturer_string_(manufacturer_string),
      product_string_(product_string) {
}

DevicePermissionEntry::DevicePermissionEntry(
    uint16_t vendor_id,
    uint16_t product_id,
    const base::string16& serial_number,
    const base::string16& manufacturer_string,
    const base::string16& product_string,
    const base::Time& last_used)
    : vendor_id_(vendor_id),
      product_id_(product_id),
      serial_number_(serial_number),
      manufacturer_string_(manufacturer_string),
      product_string_(product_string),
      last_used_(last_used) {
}

DevicePermissionEntry::~DevicePermissionEntry() {
}

bool DevicePermissionEntry::IsPersistent() const {
  return !serial_number_.empty();
}

scoped_ptr<base::Value> DevicePermissionEntry::ToValue() const {
  if (!IsPersistent()) {
    return nullptr;
  }

  scoped_ptr<base::DictionaryValue> entry_dict(new base::DictionaryValue());
  entry_dict->SetStringWithoutPathExpansion(kDeviceType, kDeviceTypeUsb);
  entry_dict->SetIntegerWithoutPathExpansion(kDeviceVendorId, vendor_id_);
  entry_dict->SetIntegerWithoutPathExpansion(kDeviceProductId, product_id_);
  DCHECK(!serial_number_.empty());
  entry_dict->SetStringWithoutPathExpansion(kDeviceSerialNumber,
                                            serial_number_);
  if (!manufacturer_string_.empty()) {
    entry_dict->SetStringWithoutPathExpansion(kDeviceManufacturerString,
                                              manufacturer_string_);
  }
  if (!product_string_.empty()) {
    entry_dict->SetStringWithoutPathExpansion(kDeviceProductString,
                                              product_string_);
  }
  if (!last_used_.is_null()) {
    entry_dict->SetStringWithoutPathExpansion(
        kDeviceLastUsed, base::Int64ToString(last_used_.ToInternalValue()));
  }

  return entry_dict.Pass();
}

base::string16 DevicePermissionEntry::GetPermissionMessageString() const {
  if (serial_number_.empty()) {
    return l10n_util::GetStringFUTF16(IDS_DEVICE_PERMISSIONS_DEVICE_NAME,
                                      GetProduct(), GetManufacturer());
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_SERIAL, GetProduct(),
        GetManufacturer(), serial_number_);
  }
}

base::string16 DevicePermissionEntry::GetManufacturer() const {
  if (manufacturer_string_.empty()) {
    const char* vendor_name = device::UsbIds::GetVendorName(vendor_id_);
    if (vendor_name) {
      return base::UTF8ToUTF16(vendor_name);
    } else {
      return l10n_util::GetStringFUTF16(
          IDS_DEVICE_UNKNOWN_VENDOR,
          base::ASCIIToUTF16(base::StringPrintf("0x%04x", vendor_id_)));
    }
  } else {
    return manufacturer_string_;
  }
}

base::string16 DevicePermissionEntry::GetProduct() const {
  if (product_string_.empty()) {
    const char* product_name =
        device::UsbIds::GetProductName(vendor_id_, product_id_);
    if (product_name) {
      return base::UTF8ToUTF16(product_name);
    } else {
      return l10n_util::GetStringFUTF16(
          IDS_DEVICE_UNKNOWN_PRODUCT,
          base::ASCIIToUTF16(base::StringPrintf("0x%04x", product_id_)));
    }
  } else {
    return product_string_;
  }
}

DevicePermissions::~DevicePermissions() {
}

scoped_refptr<DevicePermissionEntry> DevicePermissions::FindEntry(
    scoped_refptr<device::UsbDevice> device,
    const base::string16& serial_number) const {
  const auto& ephemeral_device_entry = ephemeral_devices_.find(device);
  if (ephemeral_device_entry != ephemeral_devices_.end()) {
    return ephemeral_device_entry->second;
  }

  if (serial_number.empty()) {
    return nullptr;
  }

  for (const auto& entry : entries_) {
    if (!entry->IsPersistent()) {
      continue;
    }
    if (entry->vendor_id() != device->vendor_id()) {
      continue;
    }
    if (entry->product_id() != device->product_id()) {
      continue;
    }
    if (entry->serial_number() != serial_number) {
      continue;
    }
    return entry;
  }
  return nullptr;
}

DevicePermissions::DevicePermissions(BrowserContext* context,
                                     const std::string& extension_id) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  entries_ = GetDevicePermissionEntries(prefs, extension_id);
}

DevicePermissions::DevicePermissions(const DevicePermissions* original)
    : entries_(original->entries_),
      ephemeral_devices_(original->ephemeral_devices_) {
}

class DevicePermissionsManager::FileThreadHelper : public UsbService::Observer {
 public:
  FileThreadHelper(
      base::WeakPtr<DevicePermissionsManager> device_permissions_manager)
      : device_permissions_manager_(device_permissions_manager),
        observer_(this) {}
  virtual ~FileThreadHelper() {}

  void Start() {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    UsbService* service = device::DeviceClient::Get()->GetUsbService();
    if (service) {
      observer_.Add(service);
    }
  }

 private:
  void OnDeviceRemovedCleanup(scoped_refptr<UsbDevice> device) override {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DevicePermissionsManager::OnDeviceRemoved,
                   device_permissions_manager_, device));
  }

  base::WeakPtr<DevicePermissionsManager> device_permissions_manager_;
  ScopedObserver<UsbService, UsbService::Observer> observer_;
};

// static
DevicePermissionsManager* DevicePermissionsManager::Get(
    BrowserContext* context) {
  return DevicePermissionsManagerFactory::GetForBrowserContext(context);
}

scoped_ptr<DevicePermissions> DevicePermissionsManager::GetForExtension(
    const std::string& extension_id) {
  DCHECK(CalledOnValidThread());
  return make_scoped_ptr(new DevicePermissions(GetOrInsert(extension_id)));
}

std::vector<base::string16>
DevicePermissionsManager::GetPermissionMessageStrings(
    const std::string& extension_id) const {
  DCHECK(CalledOnValidThread());
  std::vector<base::string16> messages;
  const DevicePermissions* device_permissions = Get(extension_id);
  if (device_permissions) {
    for (const scoped_refptr<DevicePermissionEntry>& entry :
         device_permissions->entries()) {
      messages.push_back(entry->GetPermissionMessageString());
    }
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

  scoped_refptr<DevicePermissionEntry> device_entry(new DevicePermissionEntry(
      device, serial_number, manufacturer_string, product_string));

  if (device_entry->IsPersistent()) {
    for (const auto& entry : device_permissions->entries()) {
      if (entry->vendor_id() != device_entry->vendor_id()) {
        continue;
      }
      if (entry->product_id() != device_entry->product_id()) {
        continue;
      }
      if (entry->serial_number() == device_entry->serial_number()) {
        return;
      }
    }

    device_permissions->entries_.insert(device_entry);
    SaveDevicePermissionEntry(context_, extension_id, device_entry);
  } else if (!ContainsKey(device_permissions->ephemeral_devices_, device)) {
    // Non-persistent devices cannot be reliably identified when they are
    // reconnected so such devices are only remembered until disconnect.
    // Register an observer here so that this set doesn't grow undefinitely.
    device_permissions->entries_.insert(device_entry);
    device_permissions->ephemeral_devices_[device] = device_entry;

    // Only start observing when an ephemeral device has been added so that
    // UsbService is not automatically initialized on profile creation (which it
    // would be if this call were in the constructor).
    if (!helper_) {
      helper_ = new FileThreadHelper(weak_factory_.GetWeakPtr());
      // base::Unretained is safe because any task to delete helper_ will be
      // executed after this call.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&FileThreadHelper::Start, base::Unretained(helper_)));
    }
  }
}

void DevicePermissionsManager::UpdateLastUsed(
    const std::string& extension_id,
    scoped_refptr<DevicePermissionEntry> entry) {
  DCHECK(CalledOnValidThread());
  entry->set_last_used(base::Time::Now());
  if (entry->IsPersistent()) {
    UpdateDevicePermissionEntry(context_, extension_id, entry);
  }
}

void DevicePermissionsManager::RemoveEntry(
    const std::string& extension_id,
    scoped_refptr<DevicePermissionEntry> entry) {
  DCHECK(CalledOnValidThread());
  DevicePermissions* device_permissions = Get(extension_id);
  DCHECK(device_permissions);
  DCHECK(ContainsKey(device_permissions->entries_, entry));
  device_permissions->entries_.erase(entry);
  if (entry->IsPersistent()) {
    RemoveDevicePermissionEntry(context_, extension_id, entry);
  } else {
    device_permissions->ephemeral_devices_.erase(entry->device_);
  }
}

void DevicePermissionsManager::Clear(const std::string& extension_id) {
  DCHECK(CalledOnValidThread());

  ClearDevicePermissionEntries(ExtensionPrefs::Get(context_), extension_id);
  DevicePermissions* device_permissions = Get(extension_id);
  if (device_permissions) {
    extension_id_to_device_permissions_.erase(extension_id);
    delete device_permissions;
  }
}

DevicePermissionsManager::DevicePermissionsManager(
    content::BrowserContext* context)
    : context_(context),
      process_manager_observer_(this),
      helper_(nullptr),
      weak_factory_(this) {
  process_manager_observer_.Add(ProcessManager::Get(context));
}

DevicePermissionsManager::~DevicePermissionsManager() {
  for (const auto& map_entry : extension_id_to_device_permissions_) {
    DevicePermissions* device_permissions = map_entry.second;
    delete device_permissions;
  }
  if (helper_) {
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, helper_);
    helper_ = nullptr;
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
    for (const auto& map_entry : device_permissions->ephemeral_devices_) {
      device_permissions->entries_.erase(map_entry.second);
    }
    device_permissions->ephemeral_devices_.clear();
  }
}

void DevicePermissionsManager::OnDeviceRemoved(
    scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());
  for (const auto& map_entry : extension_id_to_device_permissions_) {
    // An ephemeral device cannot be identified if it is reconnected and so
    // permission to access it is cleared on disconnect.
    DevicePermissions* device_permissions = map_entry.second;
    const auto& device_entry =
        device_permissions->ephemeral_devices_.find(device);
    if (device_entry != device_permissions->ephemeral_devices_.end()) {
      device_permissions->entries_.erase(device_entry->second);
      device_permissions->ephemeral_devices_.erase(device);
    }
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
