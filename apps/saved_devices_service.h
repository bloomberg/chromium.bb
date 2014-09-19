// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SAVED_DEVICES_SERVICE_H_
#define APPS_SAVED_DEVICES_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "device/usb/usb_device.h"

class Profile;

namespace base {
class Value;
}

namespace extensions {
class Extension;
}

namespace apps {

// Represents a device that a user has given an app permission to access. It
// will be persisted to disk (in the Preferences file) and so should remain
// serializable.
struct SavedDeviceEntry {
  SavedDeviceEntry(uint16_t vendor_id,
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

// Tracks the devices that apps have retained access to both while running and
// when suspended.
class SavedDevicesService : public KeyedService,
                            public content::NotificationObserver {
 public:
  // Tracks the devices that a particular extension has retained access to.
  // Unlike SavedDevicesService the functions of this class can be called from
  // the FILE thread.
  class SavedDevices : device::UsbDevice::Observer {
   public:
    bool IsRegistered(scoped_refptr<device::UsbDevice> device) const;
    void RegisterDevice(scoped_refptr<device::UsbDevice> device,
                        /* optional */ base::string16* serial_number);

   private:
    friend class SavedDevicesService;

    SavedDevices(Profile* profile, const std::string& extension_id);
    virtual ~SavedDevices();

    // device::UsbDevice::Observer
    virtual void OnDisconnect(scoped_refptr<device::UsbDevice> device) OVERRIDE;

    Profile* profile_;
    const std::string extension_id_;

    // Devices with serial numbers are written to the prefs file.
    std::vector<SavedDeviceEntry> persistent_devices_;
    // Other devices are ephemeral devices and cleared when the extension host
    // is destroyed.
    std::set<scoped_refptr<device::UsbDevice> > ephemeral_devices_;

    DISALLOW_COPY_AND_ASSIGN(SavedDevices);
  };

  explicit SavedDevicesService(Profile* profile);
  virtual ~SavedDevicesService();

  static SavedDevicesService* Get(Profile* profile);

  // Returns the SavedDevices for |extension_id|, creating it if necessary.
  SavedDevices* GetOrInsert(const std::string& extension_id);

  std::vector<SavedDeviceEntry> GetAllDevices(
      const std::string& extension_id) const;

  // Clears the SavedDevices for |extension_id|.
  void Clear(const std::string& extension_id);

 private:
  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns the SavedDevices for |extension_id| or NULL if one does not exist.
  SavedDevices* Get(const std::string& extension_id) const;

  std::map<std::string, SavedDevices*> extension_id_to_saved_devices_;
  Profile* profile_;
  content::NotificationRegistrar registrar_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SavedDevicesService);
};

}  // namespace apps

#endif  // APPS_SAVED_DEVICES_SERVICE_H_
