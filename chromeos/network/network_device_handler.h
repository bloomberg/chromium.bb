// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/network/network_util.h"  // WifiAccessPoint

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

class CHROMEOS_EXPORT NetworkDeviceHandler
    : public ShillPropertyChangedObserver {
 public:
  struct Device {
    Device();
    ~Device();
    std::string type;
    bool powered;
    bool scanning;
    int scan_interval;
    std::map<std::string, WifiAccessPoint> wifi_access_points;
  };
  typedef std::map<std::string, Device> DeviceMap;

  class Observer {
   public:
    typedef NetworkDeviceHandler::DeviceMap DeviceMap;

    // Called when devices are updated. Passes the updated map of devices.
    virtual void NetworkDevicesUpdated(const DeviceMap& devices) = 0;

   protected:
    virtual ~Observer() {}
  };

  NetworkDeviceHandler();
  virtual ~NetworkDeviceHandler();
  void Init();

  // Add/remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool devices_ready() const { return devices_ready_; }
  const DeviceMap& devices() const { return devices_; }

  // ShillPropertyChangedObserver overrides
  virtual void OnPropertyChanged(const std::string& key,
                                 const base::Value& value) OVERRIDE;

 private:
  void ManagerPropertiesCallback(DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& properties);
  void DevicePropertyChanged(const base::ListValue* devices);
  void DevicePropertiesCallback(const std::string& device_path,
                                DBusMethodCallStatus call_status,
                                const base::DictionaryValue& properties);
  void NetworkPropertiesCallback(const std::string& device_path,
                                 const std::string& network_path,
                                 DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& properties);
  void DeviceNetworkReady(const std::string& device_path,
                          const std::string& network_path);
  void DeviceReady(const std::string& device_path);

  // True when the device list is up to date.
  bool devices_ready_;

  // Map of Device structs, valid when |devices_ready_| is true.
  DeviceMap devices_;

  // Map of pending devices.
  std::set<std::string> pending_device_paths_;

  // Map of pending networks per device path.
  std::map<std::string, std::set<std::string> > pending_network_paths_;

  // Observer list
  ObserverList<Observer> observers_;

  // For Shill client callbacks
  base::WeakPtrFactory<NetworkDeviceHandler> weak_ptr_factory_;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_H_
