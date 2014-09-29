// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_DEVICE_PERMISSION_MANAGER_H_
#define EXTENSIONS_DEVICE_PERMISSION_MANAGER_H_

#include <map>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "device/usb/usb_device.h"

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
                        const base::string16& serial_number);

  base::Value* ToValue() const;

  // The vendor ID of this device.
  uint16_t vendor_id;

  // The product ID of this device.
  uint16_t product_id;

  // The serial number (possibly alphanumeric) of this device.
  base::string16 serial_number;
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
                                 public content::NotificationObserver,
                                 public device::UsbDevice::Observer {
 public:
  static DevicePermissionsManager* Get(content::BrowserContext* context);

  // Returns a copy of the DevicePermissions object for a given extension that
  // can be used by any thread.
  scoped_ptr<DevicePermissions> GetForExtension(
      const std::string& extension_id);

  std::vector<base::string16> GetPermissionMessageStrings(
      const std::string& extension_id);

  void AllowUsbDevice(const std::string& extension_id,
                      scoped_refptr<device::UsbDevice> device,
                      const base::string16& serial_number);

  void Clear(const std::string& extension_id);

 private:
  friend class DevicePermissionsManagerFactory;

  DevicePermissionsManager(content::BrowserContext* context);
  virtual ~DevicePermissionsManager();

  DevicePermissions* Get(const std::string& extension_id) const;
  DevicePermissions* GetOrInsert(const std::string& extension_id);

  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // device::UsbDevice::Observer
  virtual void OnDisconnect(scoped_refptr<device::UsbDevice> device) OVERRIDE;

  content::BrowserContext* context_;
  std::map<std::string, DevicePermissions*> extension_id_to_device_permissions_;
  content::NotificationRegistrar registrar_;

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
  virtual ~DevicePermissionsManagerFactory();

  // BrowserContextKeyedServiceFactory
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DevicePermissionsManagerFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_DEVICE_PERMISSION_MANAGER_H_
