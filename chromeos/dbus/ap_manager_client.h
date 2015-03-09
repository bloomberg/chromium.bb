// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_DBUS_AP_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_AP_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// ApManagerClient is used to communicate with the
// WiFi AP Manager service.  All methods should be called from
// the origin thread which initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT ApManagerClient : public DBusClient {
 public:
  // Structure of properties associated with a WiFi service AP configuration.
  // These properties must be set before Start is called.
  class ConfigProperties : public dbus::PropertySet {
   public:
    ConfigProperties(dbus::ObjectProxy* object_proxy,
                     const std::string& interface_name,
                     const PropertyChangedCallback& callback);
    ~ConfigProperties() override;

    // Name of network [required].
    const std::string& ssid() const { return ssid_.value(); }
    void set_ssid(const std::string& ssid, SetCallback callback) {
      ssid_.Set(ssid, callback);
    }

    // Interface name [optional].
    const std::string& interface_name() const {
      return interface_name_.value();
    }
    void set_interface_name(const std::string& name, SetCallback callback) {
      interface_name_.Set(name, callback);
    }

    // Security Mode; "RSN", "None" [optional].
    const std::string& security_mode() const { return security_mode_.value(); }
    void set_security_mode(const std::string& mode, SetCallback callback) {
      security_mode_.Set(mode, callback);
    }

    // Passphrase; [required] when security is not "None".
    const std::string& passphrase() const { return passphrase_.value(); }
    void set_passphrase(const std::string& passphrase, SetCallback callback) {
      passphrase_.Set(passphrase, callback);
    }

    // HwMode, "802.11a", "802.11b". Default: "802.11g" [optional].
    const std::string& hw_mode() const { return hw_mode_.value(); }
    void set_hw_mode(const std::string& mode, SetCallback callback) {
      hw_mode_.Set(mode, callback);
    }

    // Operation mode, "Bridge" or "Server". Default "Server". [optional].
    const std::string& operation_mode() const {
      return operation_mode_.value();
    }
    void set_operation_mode(const std::string& mode, SetCallback callback) {
      operation_mode_.Set(mode, callback);
    }

    // Operating channel. Default '6' [optional].
    uint16_t channel() const { return channel_.value(); }
    void set_channel(uint16_t channel, SetCallback callback) {
      channel_.Set(channel, callback);
    }

    // Hidden network. Default "false". [optional].
    bool hidden_network() const { return hidden_network_.value(); }
    void set_hidden_network(bool hidden, SetCallback callback) {
      hidden_network_.Set(hidden, callback);
    }

    // Bridge interface. [required] if Bridge operation mode set.
    const std::string& bridge_interface() const {
      return bridge_interface_.value();
    }
    void set_bridge_interface(const std::string& interface,
                              SetCallback callback) {
      bridge_interface_.Set(interface, callback);
    }

    // The value of x in the following equation; "192.168.x.254".
    // This will be the server's IP address. [required] only if
    // operation mode set to "Server".
    uint16_t server_address_index() const {
      return server_address_index_.value();
    }
    void set_server_address_index(uint16_t index, SetCallback callback) {
      server_address_index_.Set(index, callback);
    }

   private:
    dbus::Property<std::string> ssid_;
    dbus::Property<std::string> interface_name_;
    dbus::Property<std::string> security_mode_;
    dbus::Property<std::string> passphrase_;
    dbus::Property<std::string> hw_mode_;
    dbus::Property<std::string> operation_mode_;
    dbus::Property<uint16_t> channel_;
    dbus::Property<bool> hidden_network_;
    dbus::Property<std::string> bridge_interface_;
    dbus::Property<uint16_t> server_address_index_;

    DISALLOW_COPY_AND_ASSIGN(ConfigProperties);
  };

  // Structure of properties associated with a WiFi service AP device.
  class DeviceProperties : public dbus::PropertySet {
   public:
    DeviceProperties(dbus::ObjectProxy* object_proxy,
                     const std::string& interface_name,
                     const PropertyChangedCallback& callback);
    ~DeviceProperties() override;

    // Name of the WiFi device.
    const std::string& device_name() const { return device_name_.value(); }

    // Flag indicating if this device is currently in-use by apmanager.
    bool in_use() const { return in_used_.value(); }

    // Name of the WiFi interface on the device that’s preferred for starting an
    // AP serivce.
    const std::string& preferred_ap_interface() const {
      return preferred_ap_interface_.value();
    }

   private:
    dbus::Property<std::string> device_name_;
    dbus::Property<bool> in_used_;
    dbus::Property<std::string> preferred_ap_interface_;

    DISALLOW_COPY_AND_ASSIGN(DeviceProperties);
  };

  // Structure of properties associated with a WiFi service AP service.
  class ServiceProperties : public dbus::PropertySet {
   public:
    ServiceProperties(dbus::ObjectProxy* object_proxy,
                      const std::string& interface_name,
                      const PropertyChangedCallback& callback);
    ~ServiceProperties() override;

    // DBus path of the config for this service.
    const dbus::ObjectPath& config_path() const { return config_.value(); }

    // Current state of service. ["Idle", "Starting", "Started", "Failed"].
    const std::string& state() const { return state_.value(); }

   private:
    dbus::Property<dbus::ObjectPath> config_;
    dbus::Property<std::string> state_;

    DISALLOW_COPY_AND_ASSIGN(ServiceProperties);
  };

  // Interface for observing changes from a apmanager daemon.
  class Observer {
   public:
    virtual ~Observer();

    // Called when the manager has been added.
    virtual void ManagerAdded();

    // Called when the manager has been removed.
    virtual void ManagerRemoved();

    // Called when the config with object path |object_path| is added to the
    // system.
    virtual void ConfigAdded(const dbus::ObjectPath& object_path);

    // Called when the config with object path |object_path| is removed from
    // the system.
    virtual void ConfigRemoved(const dbus::ObjectPath& object_path);

    // Called when the device with object path |object_path| is added to the
    // system.
    virtual void DeviceAdded(const dbus::ObjectPath& object_path);

    // Called when the device with object path |object_path| is removed from
    // the system.
    virtual void DeviceRemoved(const dbus::ObjectPath& object_path);

    // Called when the device with object path |object_path| is added to the
    // system.
    virtual void ServiceAdded(const dbus::ObjectPath& object_path);

    // Called when the device with object path |object_path| is removed from
    // the system.
    virtual void ServiceRemoved(const dbus::ObjectPath& object_path);

    // Called when the adapter with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void ConfigPropertyChanged(const dbus::ObjectPath& object_path,
                                       const std::string& property_name);

    // Called when the adapter with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                                       const std::string& property_name);

    // Called when the adapter with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void ServicePropertyChanged(const dbus::ObjectPath& object_path,
                                        const std::string& property_name);
  };

  ~ApManagerClient() override;

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ApManagerClient* Create();

  // Adds and removes observers for events on all apmanager
  // events. Check the |object_path| parameter of observer methods to
  // determine which group is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Calls CreateService method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void CreateService(const ObjectPathDBusMethodCallback& callback) = 0;

  // Calls RemoveService method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void RemoveService(const dbus::ObjectPath& object_path,
                             const VoidDBusMethodCallback& callback) = 0;

  // Calls Service::Start method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void StartService(const dbus::ObjectPath& object_path,
                            const VoidDBusMethodCallback& callback) = 0;

  // Calls Service::Stop method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void StopService(const dbus::ObjectPath& object_path,
                           const VoidDBusMethodCallback& callback) = 0;

  // Obtains the properties for the config with object path |object_path|,
  // any values should be copied if needed.
  virtual ConfigProperties* GetConfigProperties(
      const dbus::ObjectPath& object_path) = 0;

  // Obtains the properties for the device with object path |object_path|,
  // any values should be copied if needed.
  virtual const DeviceProperties* GetDeviceProperties(
      const dbus::ObjectPath& object_path) = 0;

  // Obtains the properties for the device with object path |object_path|,
  // any values should be copied if needed.
  virtual const ServiceProperties* GetServiceProperties(
      const dbus::ObjectPath& object_path) = 0;

 protected:
  // Create() should be used instead.
  ApManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ApManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_AP_MANAGER_CLIENT_H_
