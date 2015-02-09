// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PEER_DAEMON_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_PEER_DAEMON_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/property.h"

namespace chromeos {

// PeerDaemonManagerClient is used to communicate with the PeerDaemon Manager
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT PeerDaemonManagerClient : public DBusClient {
 public:
  class ManagerProperties : public dbus::PropertySet {
   public:
    ManagerProperties(dbus::ObjectProxy* object_proxy,
                      const PropertyChangedCallback& callback);
    ~ManagerProperties() override;

    const std::vector<std::string>& monitored_technologies() const {
      return monitored_technologies_.value();
    }

   private:
    dbus::Property<std::vector<std::string>> monitored_technologies_;

    DISALLOW_COPY_AND_ASSIGN(ManagerProperties);
  };

  class ServiceProperties : public dbus::PropertySet {
   public:
    ServiceProperties(dbus::ObjectProxy* object_proxy,
                      const PropertyChangedCallback& callback);
    ~ServiceProperties() override;

    const std::string& service_id() const { return service_id_.value(); }
    const std::map<std::string, std::string>& service_info() const {
      return service_info_.value();
    }
    const std::vector<std::pair<std::vector<uint8_t>, uint16_t>>& ip_infos()
        const {
      return ip_infos_.value();
    }

   private:
    dbus::Property<std::string> service_id_;
    dbus::Property<std::map<std::string, std::string>> service_info_;
    dbus::Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>>
        ip_infos_;

    DISALLOW_COPY_AND_ASSIGN(ServiceProperties);
  };

  class PeerProperties : public dbus::PropertySet {
   public:
    PeerProperties(dbus::ObjectProxy* object_proxy,
                   const PropertyChangedCallback& callback);
    ~PeerProperties() override;

    const std::string& uuid() const { return uuid_.value(); }
    uint64_t last_seen() const { return last_seen_.value(); }

   private:
    dbus::Property<std::string> uuid_;
    dbus::Property<uint64_t> last_seen_;

    DISALLOW_COPY_AND_ASSIGN(PeerProperties);
  };

  // Interface for observing changes from a leadership daemon.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the peer daemon manager is added.
    virtual void ManagerAdded() {}

    // Called when the peer daemon manager is removed; perhaps on a process
    // crash of the peer daemon.
    virtual void ManagerRemoved() {}

    // Called when the manager changes a property value.
    virtual void ManagerPropertyChanged(const std::string& property_name) {}

    // Called when the service with object path |object_path| is added to the
    // system.
    virtual void ServiceAdded(const dbus::ObjectPath& object_path) {}

    // Called when the service with object path |object_path| is removed from
    // the system.
    virtual void ServiceRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the service with object path |object_path| changes a
    // property value.
    virtual void ServicePropertyChanged(const dbus::ObjectPath& object_path,
                                        const std::string& property_name) {}

    // Called when the peer with object path |object_path| is added to the
    // system.
    virtual void PeerAdded(const dbus::ObjectPath& object_path) {}

    // Called when the peer with object path |object_path| is removed from
    // the system.
    virtual void PeerRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the peer with object path |object_path| changes a
    // property value.
    virtual void PeerPropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) {}
  };

  ~PeerDaemonManagerClient() override;

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static PeerDaemonManagerClient* Create();

  // Adds and removes observers for events on all peer events.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Retrieves a list of all the services.
  virtual std::vector<dbus::ObjectPath> GetServices() = 0;

  // Retrieves a list of all the peers.
  virtual std::vector<dbus::ObjectPath> GetPeers() = 0;

  // Obtains the properties for the service with object path |object_path|,
  // any values should be copied if needed.
  virtual ServiceProperties* GetServiceProperties(
      const dbus::ObjectPath& object_path) = 0;

  // Obtains the properties for the peer with object path |object_path|,
  // any values should be copied if needed.
  virtual PeerProperties* GetPeerProperties(
      const dbus::ObjectPath& object_path) = 0;

  // Calls StartMonitoring method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void StartMonitoring(
      const std::vector<std::string>& requested_technologies,
      const base::DictionaryValue& options,
      const StringDBusMethodCallback& callback) = 0;

  // Calls StopMonitoring method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void StopMonitoring(const std::string& monitoring_token,
                              const VoidDBusMethodCallback& callback) = 0;

  // Calls ExposeService method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void ExposeService(
      const std::string& service_id,
      const std::map<std::string, std::string>& service_info,
      const base::DictionaryValue& options,
      const StringDBusMethodCallback& callback) = 0;

  // Calls RemoveExposedService method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void RemoveExposedService(const std::string& service_token,
                                    const VoidDBusMethodCallback& callback) = 0;

  // Calls Ping method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void Ping(const StringDBusMethodCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  PeerDaemonManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PeerDaemonManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PEER_DAEMON_MANAGER_CLIENT_H_
