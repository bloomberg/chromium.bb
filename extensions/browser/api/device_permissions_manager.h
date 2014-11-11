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
#include "device/usb/usb_device.h"
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

namespace device {
class UsbDevice;
}

namespace extensions {

// Stores information about a device saved with access granted.
struct DevicePermissionEntry {
  DevicePermissionEntry(uint16_t vendor_id,
                        uint16_t product_id,
                        const base::string16& serial_number,
                        const base::string16& manufacturer_string,
                        const base::string16& product_string);

  base::Value* ToValue() const;

  // The vendor ID of this device.
  uint16_t vendor_id;

  // The product ID of this device.
  uint16_t product_id;

  // The serial number (possibly alphanumeric) of this device.
  base::string16 serial_number;

  // The manufacturer string read from the device (optional).
  base::string16 manufacturer_string;

  // The product string read from the device (optional).
  base::string16 product_string;
};

// Stores a copy of device permissions associated with a particular extension.
class DevicePermissions {
 public:
  virtual ~DevicePermissions();

  bool CheckUsbDevice(scoped_refptr<device::UsbDevice> device) const;

 private:
  friend class DevicePermissionsManager;

  DevicePermissions(content::BrowserContext* context,
                    const std::string& extension_id);
  DevicePermissions(
      const std::vector<DevicePermissionEntry>& permission_entries,
      const std::set<scoped_refptr<device::UsbDevice>>& ephemeral_devices);

  std::vector<DevicePermissionEntry>& permission_entries();
  std::set<scoped_refptr<device::UsbDevice>>& ephemeral_devices();

  std::vector<DevicePermissionEntry> permission_entries_;
  std::set<scoped_refptr<device::UsbDevice>> ephemeral_devices_;

  DISALLOW_COPY_AND_ASSIGN(DevicePermissions);
};

// Manages saved device permissions for all extensions.
class DevicePermissionsManager : public KeyedService,
                                 public base::NonThreadSafe,
                                 public ProcessManagerObserver,
                                 public device::UsbDevice::Observer {
 public:
  static DevicePermissionsManager* Get(content::BrowserContext* context);

  // Returns a copy of the DevicePermissions object for a given extension that
  // can be used by any thread.
  scoped_ptr<DevicePermissions> GetForExtension(
      const std::string& extension_id);

  std::vector<base::string16> GetPermissionMessageStrings(
      const std::string& extension_id);

  // TODO(reillyg): AllowUsbDevice should only take the extension ID and
  // device, with the strings read from the device. This isn't possible now as
  // the device can not be accessed from the UI thread yet. crbug.com/427985
  void AllowUsbDevice(const std::string& extension_id,
                      scoped_refptr<device::UsbDevice> device,
                      const base::string16& serial_number,
                      const base::string16& manufacturer_string,
                      const base::string16& product_string);

  void Clear(const std::string& extension_id);

 private:
  friend class DevicePermissionsManagerFactory;
  FRIEND_TEST_ALL_PREFIXES(DevicePermissionsManagerTest, SuspendExtension);

  DevicePermissionsManager(content::BrowserContext* context);
  ~DevicePermissionsManager() override;

  DevicePermissions* Get(const std::string& extension_id) const;
  DevicePermissions* GetOrInsert(const std::string& extension_id);

  // ProcessManagerObserver implementation
  void OnBackgroundHostClose(const std::string& extension_id) override;

  // device::UsbDevice::Observer implementation
  void OnDisconnect(scoped_refptr<device::UsbDevice> device) override;

  content::BrowserContext* context_;
  std::map<std::string, DevicePermissions*> extension_id_to_device_permissions_;
  ScopedObserver<ProcessManager, ProcessManagerObserver>
      process_manager_observer_;

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
