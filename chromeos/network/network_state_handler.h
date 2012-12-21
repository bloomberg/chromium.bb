// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/managed_state.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/shill_property_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

class DeviceState;
class NetworkState;
class NetworkStateHandlerObserver;
class NetworkStateHandlerTest;

// Class for tracking the list of visible networks and their properties.
//
// This class maps essential properties from the connection manager (Shill) for
// each visible network. It is not used to change the properties of services or
// devices, only global (manager) properties.
//
// All getters return the currently cached properties. This class is expected to
// keep properties up to date by managing the appropriate Shill observers.
// It will invoke its own more specific observer methods when the specified
// changes occur.
class CHROMEOS_EXPORT NetworkStateHandler
    : public internal::ShillPropertyHandler::Listener {
 public:
  typedef std::vector<ManagedState*> ManagedStateList;
  typedef std::vector<const NetworkState*> NetworkStateList;

  virtual ~NetworkStateHandler();

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static NetworkStateHandler* Get();

  // Add/remove observers.
  void AddObserver(NetworkStateHandlerObserver* observer);
  void RemoveObserver(NetworkStateHandlerObserver* observer);

  // Returns true if |technology| is enabled / available.
  bool TechnologyAvailable(const std::string& technology) const;
  bool TechnologyEnabled(const std::string& technology) const;

  // Asynchronously sets the enabled property for |technology|.
  // Note: Modifies Manager state. Calls |error_callback| on failure.
  void SetTechnologyEnabled(
      const std::string& technology,
      bool enabled,
      const network_handler::ErrorCallback& error_callback);

  // Finds and returns a device state by |device_path| or NULL if not found.
  const DeviceState* GetDeviceState(const std::string& device_path) const;

  // Finds and returns a device state by |type|. Returns NULL if not found.
  const DeviceState* GetDeviceStateByType(const std::string& type) const;

  // Finds and returns a network state by |service_path| or NULL if not found.
  // Note: NetworkState is frequently updated asynchronously, i.e. properties
  // are not always updated all at once. This will contain the most recent
  // value for each property. To receive notifications when a property changes,
  // observe this class and implement NetworkPropertyChanged().
  const NetworkState* GetNetworkState(const std::string& service_path) const;

  // Returns the default connected network (which includes VPNs) or NULL.
  // This is equivalent to ConnectedNetworkByType(kMatchTypeDefault).
  const NetworkState* DefaultNetwork() const;

  // Returns the primary connected network of matching |type|, otherwise NULL.
  // |type| can be a type defined in service_constants.h, or one of the
  // following special constants:
  // * kMatchTypeDefault returns the default (active) network
  // * kMatchTypeNonVirtual returns the primary connected non virtual network
  // * kMatchTypeWireless returns the primary connected wireless network
  const NetworkState* ConnectedNetworkByType(const std::string& type) const;

  // Like ConnectedNetworkByType() but returns a connecting network or NULL.
  const NetworkState* ConnectingNetworkByType(const std::string& type) const;

  // Returns the hardware (MAC) address for the first connected network
  // matching |type|, or an empty string if none.
  std::string HardwareAddressForType(const std::string& type) const;
  // Same as above but in aa:bb format.
  std::string FormattedHardwareAddressForType(const std::string& type) const;

  // Sets |list| to contain the list of networks.  The returned list contains
  // a copy of NetworkState pointers which should not be stored or used beyond
  // the scope of the calling function (i.e. they may later become invalid, but
  // only on the UI thread).
  void GetNetworkList(NetworkStateList* list) const;

  // Requests a scan of wifi networks. This may trigger updates to the network
  // list, which will trigger the appropriate observer calls.
  // Returns true if a scan was requested.
  bool RequestWifiScan() const;

  static const char kMatchTypeDefault[];
  static const char kMatchTypeWireless[];
  static const char kMatchTypeNonVirtual[];

 protected:
  NetworkStateHandler();

  // ShillPropertyHandler::Listener overrides.

  // This adds new entries to the managed list specified by |type| and deletes
  // any entries that are no longer in the list.
  virtual void UpdateManagedList(ManagedState::ManagedType type,
                                 const base::ListValue& entries) OVERRIDE;

  // Sets |available_technologies_| to contain only entries in |technologies|.
  virtual void UpdateAvailableTechnologies(
      const base::ListValue& technologies) OVERRIDE;

  // Sets |enabled_technologies_| to contain only entries in |technologies|.
  virtual void UpdateEnabledTechnologies(
      const base::ListValue& technologies) OVERRIDE;

  // Parses the properties for the network service or device. Mostly calls
  // managed->PropertyChanged(key, value) for each dictionary entry.
  virtual void UpdateManagedStateProperties(
      ManagedState::ManagedType type,
      const std::string& path,
      const base::DictionaryValue& properties) OVERRIDE;

  // Called by ShillPropertyHandler when a watched service property changes.
  // Calls ParseNetworkServiceProperty() and signals observers.
  virtual void UpdateNetworkServiceProperty(
      const std::string& service_path,
      const std::string& key,
      const base::Value& value) OVERRIDE;

  // Sets the IP Address for the network associated with |service_path|.
  virtual void UpdateNetworkServiceIPAddress(
      const std::string& service_path,
      const std::string& ip_address) OVERRIDE;

  // Sends NetworkManagerChanged() to observers.
  virtual void ManagerPropertyChanged() OVERRIDE;

  // Called by |shill_property_handler_| when the service or device list has
  // changed and all entries have been updated. This updates the list and
  // notifies observers. If |type| == TYPE_NETWORK this also calls
  // CheckDefaultNetworkChanged().
  virtual void ManagedStateListChanged(
      ManagedState::ManagedType type) OVERRIDE;

  // Called in Initialize(). Called explicitly by tests after adding
  // test observers.
  void InitShillPropertyHandler();

 private:
  friend class NetworkStateHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkStateHandlerTest, NetworkStateHandlerStub);

  // Non-const getters for managed entries. These are const so that they can
  // be called by Get[Network|Device]State, even though they return non-const
  // pointers.
  DeviceState* GetModifiableDeviceState(const std::string& device_path) const;
  NetworkState* GetModifiableNetworkState(
      const std::string& service_path) const;
  ManagedState* GetModifiableManagedState(const ManagedStateList* managed_list,
                                          const std::string& path) const;

  // Gets the list specified by |type|.
  ManagedStateList* GetManagedList(ManagedState::ManagedType type);

  // Helper function called to parse |network| properties. Also calls
  // OnNetworkConnectionStateChanged() if the connection_state property changes.
  bool ParseNetworkServiceProperty(NetworkState* network,
                                   const std::string& key,
                                   const base::Value& value);

  // Helper function to notify observers. Calls CheckDefaultNetworkChanged().
  void OnNetworkConnectionStateChanged(NetworkState* network);

  // If the default network changed returns true and sets
  // |default_network_path_|.
  bool CheckDefaultNetworkChanged();

  // Logs an event and notifies observers.
  void OnDefaultNetworkChanged();

  // Shill property handler instance, owned by this class.
  scoped_ptr<internal::ShillPropertyHandler> shill_property_handler_;

  // Observer list
  ObserverList<NetworkStateHandlerObserver> observers_;

  // Lists of managed states
  ManagedStateList network_list_;
  ManagedStateList device_list_;

  // Lists of available / enabled technologies
  std::set<std::string> available_technologies_;
  std::set<std::string> enabled_technologies_;

  // Keeps track of the default network for notifying observers when it changes.
  std::string default_network_path_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_H_
