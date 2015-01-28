// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_DEVICE_PERMISSION_MANAGER_H_
#define EXTENSIONS_DEVICE_PERMISSION_MANAGER_H_

#include <map>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_thread.h"
#include "device/usb/usb_service.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"

template <typename T>
struct DefaultSingletonTraits;

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// Stores information about a device saved with access granted.
class DevicePermissionEntry
    : public base::RefCountedThreadSafe<DevicePermissionEntry> {
 public:
  // TODO(reillyg): This function should be able to take only the
  // device::UsbDevice and read the strings from there. This is not yet possible
  // as the device can not be accessed from the UI thread. crbug.com/427985
  DevicePermissionEntry(scoped_refptr<device::UsbDevice> device,
                        const base::string16& serial_number,
                        const base::string16& manufacturer_string,
                        const base::string16& product_string);
  DevicePermissionEntry(uint16_t vendor_id,
                        uint16_t product_id,
                        const base::string16& serial_number,
                        const base::string16& manufacturer_string,
                        const base::string16& product_string,
                        const base::Time& last_used);

  // A persistent device is one that can be recognized when it is reconnected
  // and can therefore be remembered persistently by writing information about
  // it to ExtensionPrefs. Currently this means it has a serial number string.
  bool IsPersistent() const;

  // Convert the device to a serializable value, returns a null pointer if the
  // entry is not persistent.
  scoped_ptr<base::Value> ToValue() const;

  base::string16 GetPermissionMessageString() const;

  uint16_t vendor_id() const { return vendor_id_; }
  uint16_t product_id() const { return product_id_; }
  const base::string16& serial_number() const { return serial_number_; }
  const base::Time& last_used() const { return last_used_; }

  base::string16 GetManufacturer() const;
  base::string16 GetProduct() const;

 private:
  friend class base::RefCountedThreadSafe<DevicePermissionEntry>;
  friend class DevicePermissionsManager;

  ~DevicePermissionEntry();

  void set_last_used(const base::Time& last_used) { last_used_ = last_used; }

  // The USB device tracked by this entry, may be null if this entry was
  // restored from ExtensionPrefs.
  scoped_refptr<device::UsbDevice> device_;
  // The vendor ID of this device.
  uint16_t vendor_id_;
  // The product ID of this device.
  uint16_t product_id_;
  // The serial number (possibly alphanumeric) of this device.
  base::string16 serial_number_;
  // The manufacturer string read from the device (optional).
  base::string16 manufacturer_string_;
  // The product string read from the device (optional).
  base::string16 product_string_;
  // The last time this device was used by the extension.
  base::Time last_used_;
};

// Stores a copy of device permissions associated with a particular extension.
class DevicePermissions {
 public:
  virtual ~DevicePermissions();

  // Attempts to find a permission entry matching the given device. The device
  // serial number is presented separately so that this function does not need
  // to call device->GetSerialNumber() which may not be possible on the
  // current thread.
  scoped_refptr<DevicePermissionEntry> FindEntry(
      scoped_refptr<device::UsbDevice> device,
      const base::string16& serial_number) const;

  const std::set<scoped_refptr<DevicePermissionEntry>>& entries() const {
    return entries_;
  }

 private:
  friend class DevicePermissionsManager;

  // Reads permissions out of ExtensionPrefs.
  DevicePermissions(content::BrowserContext* context,
                    const std::string& extension_id);
  // Does a shallow copy, duplicating the device lists so that the resulting
  // object can be used from a different thread.
  DevicePermissions(const DevicePermissions* original);

  std::set<scoped_refptr<DevicePermissionEntry>> entries_;
  std::map<scoped_refptr<device::UsbDevice>,
           scoped_refptr<DevicePermissionEntry>> ephemeral_devices_;

  DISALLOW_COPY_AND_ASSIGN(DevicePermissions);
};

// Manages saved device permissions for all extensions.
class DevicePermissionsManager : public KeyedService,
                                 public base::NonThreadSafe,
                                 public ProcessManagerObserver {
 public:
  static DevicePermissionsManager* Get(content::BrowserContext* context);

  // Returns a copy of the DevicePermissions object for a given extension that
  // can be used by any thread.
  scoped_ptr<DevicePermissions> GetForExtension(
      const std::string& extension_id);

  // Equivalent to calling GetForExtension and extracting the permission string
  // for each entry.
  std::vector<base::string16> GetPermissionMessageStrings(
      const std::string& extension_id) const;

  // TODO(reillyg): AllowUsbDevice should only take the extension ID and
  // device, with the strings read from the device. This isn't possible now as
  // the device can not be accessed from the UI thread yet. crbug.com/427985
  void AllowUsbDevice(const std::string& extension_id,
                      scoped_refptr<device::UsbDevice> device,
                      const base::string16& serial_number,
                      const base::string16& manufacturer_string,
                      const base::string16& product_string);

  // Updates the "last used" timestamp on the given device entry and writes it
  // out to ExtensionPrefs.
  void UpdateLastUsed(const std::string& extension_id,
                      scoped_refptr<DevicePermissionEntry> entry);

  // Revokes permission for the extension to access the given device.
  void RemoveEntry(const std::string& extension_id,
                   scoped_refptr<DevicePermissionEntry> entry);

  // Revokes permission for the extension to access all allowed devices.
  void Clear(const std::string& extension_id);

 private:
  class FileThreadHelper;

  friend class DevicePermissionsManagerFactory;
  FRIEND_TEST_ALL_PREFIXES(DevicePermissionsManagerTest, SuspendExtension);

  DevicePermissionsManager(content::BrowserContext* context);
  ~DevicePermissionsManager() override;

  DevicePermissions* Get(const std::string& extension_id) const;
  DevicePermissions* GetOrInsert(const std::string& extension_id);
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device);

  // ProcessManagerObserver implementation
  void OnBackgroundHostClose(const std::string& extension_id) override;

  content::BrowserContext* context_;
  std::map<std::string, DevicePermissions*> extension_id_to_device_permissions_;
  ScopedObserver<ProcessManager, ProcessManagerObserver>
      process_manager_observer_;
  FileThreadHelper* helper_;

  base::WeakPtrFactory<DevicePermissionsManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevicePermissionsManager);
};

class DevicePermissionsManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static DevicePermissionsManager* GetForBrowserContext(
      content::BrowserContext* context);
  static DevicePermissionsManagerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<DevicePermissionsManagerFactory>;

  DevicePermissionsManagerFactory();
  ~DevicePermissionsManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(DevicePermissionsManagerFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_DEVICE_PERMISSION_MANAGER_H_
