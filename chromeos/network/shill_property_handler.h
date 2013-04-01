// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_SHILL_PROPERTY_HANDLER_H_
#define CHROMEOS_NETWORK_SHILL_PROPERTY_HANDLER_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/network/managed_state.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

class ShillManagerClient;

namespace internal {

class ShillPropertyObserver;

// This class handles Shill calls and observers to reflect the state of the
// Shill Manager and its services and devices. It observes Shill.Manager and
// requests properties for new devices/networks. It takes a Listener in its
// constructor (e.g. NetworkStateHandler) that it calls when properties change
// (including once to set their initial state after Init() gets called).
// It also observes Shill.Service for all services in Manager.ServiceWatchList.
// This class must not outlive the ShillManagerClient instance.
class CHROMEOS_EXPORT ShillPropertyHandler
    : public ShillPropertyChangedObserver,
      public base::SupportsWeakPtr<ShillPropertyHandler> {
 public:
  typedef std::map<std::string, ShillPropertyObserver*>
      ShillPropertyObserverMap;

  class CHROMEOS_EXPORT Listener {
   public:
    // Called when the entries in a managed list have changed.
    virtual void UpdateManagedList(ManagedState::ManagedType type,
                                   const base::ListValue& entries) = 0;

    // Called when the properties for a managed state have changed.
    virtual void UpdateManagedStateProperties(
        ManagedState::ManagedType type,
        const std::string& path,
        const base::DictionaryValue& properties) = 0;

    // Called when a property for a watched network service has changed.
    virtual void UpdateNetworkServiceProperty(
        const std::string& service_path,
        const std::string& key,
        const base::Value& value) = 0;

    // Called when a property for a watched device has changed.
    virtual void UpdateDeviceProperty(
        const std::string& device_path,
        const std::string& key,
        const base::Value& value) = 0;

    // Called when one or more manager properties (e.g. a technology list)
    // changes.
    virtual void ManagerPropertyChanged() = 0;

    // Called whent the IP address of a service has been updated. Occurs after
    // UpdateManagedStateProperties is called for the service.
    virtual void UpdateNetworkServiceIPAddress(
        const std::string& service_path,
        const std::string& ip_address) = 0;

    // Called when a managed state list has changed, after properties for any
    // new entries in the list have been received and
    // UpdateManagedStateProperties has been called for each new entry.
    virtual void ManagedStateListChanged(ManagedState::ManagedType type) = 0;

   protected:
    virtual ~Listener() {}
  };

  explicit ShillPropertyHandler(Listener* listener);
  virtual ~ShillPropertyHandler();

  // Sends an initial property request and sets up the observer.
  void Init();

  // Returns true if |technology| is available / enabled / uninitialized.
  bool TechnologyAvailable(const std::string& technology) const;
  bool TechnologyEnabled(const std::string& technology) const;
  bool TechnologyUninitialized(const std::string& technology) const;

  // Asynchronously sets the enabled state for |technology|.
  // Note: Modifes Manager state. Calls |error_callback| on failure.
  void SetTechnologyEnabled(
      const std::string& technology,
      bool enabled,
      const network_handler::ErrorCallback& error_callback);

  // Requests an immediate network scan.
  void RequestScan() const;

  // Calls Manager.ConnectToBestServices().
  void ConnectToBestServices() const;

  // Requests all properties for the service or device (called for new items).
  void RequestProperties(ManagedState::ManagedType type,
                         const std::string& path);

  // Returns true if |service_path| is being observed.
  bool IsObservingNetwork(const std::string& service_path) {
    return observed_networks_.count(service_path) != 0;
  }

  // ShillPropertyChangedObserver overrides
  virtual void OnPropertyChanged(const std::string& key,
                                 const base::Value& value) OVERRIDE;

 private:
  // Callback for dbus method fetching properties.
  void ManagerPropertiesCallback(DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& properties);
  // Called form OnPropertyChanged() and ManagerPropertiesCallback().
  // Returns true if observers should be notified.
  bool ManagerPropertyChanged(const std::string& key, const base::Value& value);

  // Updates the Shill property observers to observe any entries for |type|.
  void UpdateObserved(ManagedState::ManagedType type,
                      const base::ListValue& entries);


  // Sets |*_technologies_| to contain only entries in |technologies|.
  void UpdateAvailableTechnologies(const base::ListValue& technologies);
  void UpdateEnabledTechnologies(const base::ListValue& technologies);
  void UpdateUninitializedTechnologies(const base::ListValue& technologies);

  // Called when Shill returns the properties for a service or device.
  void GetPropertiesCallback(ManagedState::ManagedType type,
                             const std::string& path,
                             DBusMethodCallStatus call_status,
                             const base::DictionaryValue& properties);

  // Callback invoked when a watched property changes. Calls appropriate
  // handlers and signals observers.
  void PropertyChangedCallback(ManagedState::ManagedType type,
                               const std::string& path,
                               const std::string& key,
                               const base::Value& value);
  void NetworkServicePropertyChangedCallback(const std::string& path,
                                             const std::string& key,
                                             const base::Value& value);
  void NetworkDevicePropertyChangedCallback(const std::string& path,
                                            const std::string& key,
                                            const base::Value& value);

  // Callback for getting the IPConfig property of a Network. Handled here
  // instead of in NetworkState so that all asynchronous requests are done
  // in a single place (also simplifies NetworkState considerably).
  void GetIPConfigCallback(const std::string& service_path,
                           DBusMethodCallStatus call_status,
                           const base::DictionaryValue& properties);

  // Pointer to containing class (owns this)
  Listener* listener_;

  // Convenience pointer for ShillManagerClient
  ShillManagerClient* shill_manager_;

  // Pending update list for each managed state type
  std::map<ManagedState::ManagedType, std::set<std::string> > pending_updates_;

  // List of network services with Shill property changed observers
  ShillPropertyObserverMap observed_networks_;

  // List of network devices with Shill property changed observers
  ShillPropertyObserverMap observed_devices_;

  // Lists of available / enabled / uninitialized technologies
  std::set<std::string> available_technologies_;
  std::set<std::string> enabled_technologies_;
  std::set<std::string> uninitialized_technologies_;

  DISALLOW_COPY_AND_ASSIGN(ShillPropertyHandler);
};

}  // namespace internal
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_SHILL_PROPERTY_HANDLER_H_
