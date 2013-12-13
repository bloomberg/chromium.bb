// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/managed_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/shill_property_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace tracked_objects {
class Location;
}

namespace chromeos {

class DeviceState;
class NetworkState;
class NetworkStateHandlerObserver;
class NetworkStateHandlerTest;
class NetworkTypePattern;

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
//
// Most *ByType or *ForType methods will accept any of the following for |type|.
// See individual methods for specific notes.
// * Any type defined in service_constants.h (e.g. shill::kTypeWifi)
// * kMatchTypeDefault returns the default (active) network
// * kMatchTypeNonVirtual returns the primary non virtual network
// * kMatchTypeWired returns the primary wired network
// * kMatchTypeWireless returns the primary wireless network
// * kMatchTypeMobile returns the primary cellular or wimax network

class CHROMEOS_EXPORT NetworkStateHandler
    : public internal::ShillPropertyHandler::Listener {
 public:
  typedef std::vector<ManagedState*> ManagedStateList;
  typedef std::vector<const NetworkState*> NetworkStateList;
  typedef std::vector<const DeviceState*> DeviceStateList;
  typedef std::vector<const FavoriteState*> FavoriteStateList;

  enum TechnologyState {
    TECHNOLOGY_UNAVAILABLE,
    TECHNOLOGY_AVAILABLE,
    TECHNOLOGY_UNINITIALIZED,
    TECHNOLOGY_ENABLING,
    TECHNOLOGY_ENABLED
  };

  virtual ~NetworkStateHandler();

  // Add/remove observers.
  void AddObserver(NetworkStateHandlerObserver* observer,
                   const tracked_objects::Location& from_here);
  void RemoveObserver(NetworkStateHandlerObserver* observer,
                      const tracked_objects::Location& from_here);

  // Requests all Manager properties, specifically to update the complete
  // list of services which determines the list of Favorites. This should be
  // called any time a new service is configured or a Profile is loaded.
  void UpdateManagerProperties();

  // Returns the state for technology |type|. Only
  // NetworkTypePattern::Primitive, ::Mobile and ::Ethernet are supported.
  TechnologyState GetTechnologyState(const NetworkTypePattern& type) const;
  bool IsTechnologyAvailable(const NetworkTypePattern& type) const {
    return GetTechnologyState(type) != TECHNOLOGY_UNAVAILABLE;
  }
  bool IsTechnologyEnabled(const NetworkTypePattern& type) const {
    return GetTechnologyState(type) == TECHNOLOGY_ENABLED;
  }

  // Asynchronously sets the technology enabled property for |type|. Only
  // NetworkTypePattern::Primitive, ::Mobile and ::Ethernet are supported.
  // Note: Modifies Manager state. Calls |error_callback| on failure.
  void SetTechnologyEnabled(
      const NetworkTypePattern& type,
      bool enabled,
      const network_handler::ErrorCallback& error_callback);

  // Finds and returns a device state by |device_path| or NULL if not found.
  const DeviceState* GetDeviceState(const std::string& device_path) const;

  // Finds and returns a device state by |type|. Returns NULL if not found.
  const DeviceState* GetDeviceStateByType(const NetworkTypePattern& type) const;

  // Returns true if any device of |type| is scanning.
  bool GetScanningByType(const NetworkTypePattern& type) const;

  // Finds and returns a network state by |service_path| or NULL if not found.
  // Note: NetworkState is frequently updated asynchronously, i.e. properties
  // are not always updated all at once. This will contain the most recent
  // value for each property. To receive notifications when a property changes,
  // observe this class and implement NetworkPropertyChanged().
  const NetworkState* GetNetworkState(const std::string& service_path) const;

  // Returns the default connected network (which includes VPNs) or NULL.
  // This is equivalent to ConnectedNetworkByType(kMatchTypeDefault).
  const NetworkState* DefaultNetwork() const;

  // Returns the FavoriteState associated to DefaultNetwork. Returns NULL if,
  // and only if, DefaultNetwork returns NULL.
  const FavoriteState* DefaultFavoriteNetwork() const;

  // Returns the primary connected network of matching |type|, otherwise NULL.
  const NetworkState* ConnectedNetworkByType(
      const NetworkTypePattern& type) const;

  // Like ConnectedNetworkByType() but returns a connecting network or NULL.
  const NetworkState* ConnectingNetworkByType(
      const NetworkTypePattern& type) const;

  // Like ConnectedNetworkByType() but returns any matching network or NULL.
  // Mostly useful for mobile networks where there is generally only one
  // network. Note: O(N).
  const NetworkState* FirstNetworkByType(const NetworkTypePattern& type) const;

  // Returns the aa:bb formatted hardware (MAC) address for the first connected
  // network matching |type|, or an empty string if none is connected.
  std::string FormattedHardwareAddressForType(
      const NetworkTypePattern& type) const;

  // Sets |list| to contain the list of networks.  The returned list contains
  // a copy of NetworkState pointers which should not be stored or used beyond
  // the scope of the calling function (i.e. they may later become invalid, but
  // only on the UI thread).
  void GetNetworkList(NetworkStateList* list) const;

  // Like GetNetworkList() but only returns networks with matching |type|.
  void GetNetworkListByType(const NetworkTypePattern& type,
                            NetworkStateList* list) const;

  // Sets |list| to contain the list of devices.  The returned list contains
  // a copy of DeviceState pointers which should not be stored or used beyond
  // the scope of the calling function (i.e. they may later become invalid, but
  // only on the UI thread).
  void GetDeviceList(DeviceStateList* list) const;

  // Like GetDeviceList() but only returns networks with matching |type|.
  void GetDeviceListByType(const NetworkTypePattern& type,
                           DeviceStateList* list) const;

  // Sets |list| to contain the list of favorite (aka "preferred") networks.
  // See GetNetworkList() for usage, and notes for |favorite_list_|.
  // Favorites that are visible have the same path() as the entries in
  // GetNetworkList(), so GetNetworkState() can be used to determine if a
  // favorite is visible and retrieve the complete properties (and vice-versa).
  void GetFavoriteList(FavoriteStateList* list) const;

  // Like GetFavoriteList() but only returns favorites with matching |type|.
  void GetFavoriteListByType(const NetworkTypePattern& type,
                             FavoriteStateList* list) const;

  // Finds and returns a favorite state by |service_path| or NULL if not found.
  const FavoriteState* GetFavoriteState(const std::string& service_path) const;

  // Requests a network scan. This may trigger updates to the network
  // list, which will trigger the appropriate observer calls.
  void RequestScan() const;

  // Request a scan if not scanning and run |callback| when the Scanning state
  // for any Device of network type |type| completes.
  void WaitForScan(const std::string& type, const base::Closure& callback);

  // Request a network scan then signal Shill to connect to the best available
  // networks when completed.
  void ConnectToBestWifiNetwork();

  // Request an update for an existing NetworkState, e.g. after configuring
  // a network. This is a no-op if an update request is already pending. To
  // ensure that a change is picked up, this must be called after Shill
  // acknowledged it (e.g. in the callback of a SetProperties).
  // When the properties are received, NetworkPropertiesUpdated will be
  // signaled for each member of |observers_|, regardless of whether any
  // properties actually changed.
  void RequestUpdateForNetwork(const std::string& service_path);

  // Request an update for all existing NetworkState entries, e.g. after
  // loading an ONC configuration file that may have updated one or more
  // existing networks.
  void RequestUpdateForAllNetworks();

  // Set the list of devices on which portal check is enabled.
  void SetCheckPortalList(const std::string& check_portal_list);

  const std::string& GetCheckPortalListForTest() const {
    return check_portal_list_;
  }

  // Returns the FavoriteState of the EthernetEAP service, which contains the
  // EAP parameters used by the ethernet with |service_path|. If |service_path|
  // doesn't refer to an ethernet service or if the ethernet service is not
  // connected using EAP, returns NULL.
  const FavoriteState* GetEAPForEthernet(const std::string& service_path) const;

  // Generates a DictionaryValue of all NetworkState properties. Currently
  // provided for debugging purposes only.
  void GetNetworkStatePropertiesForTest(
      base::DictionaryValue* dictionary) const;

  // Construct and initialize an instance for testing.
  static NetworkStateHandler* InitializeForTest();

  // Default set of comma separated interfaces on which to enable
  // portal checking.
  static const char kDefaultCheckPortalList[];

 protected:
  friend class NetworkHandler;
  NetworkStateHandler();

  // ShillPropertyHandler::Listener overrides.

  // This adds new entries to the managed list specified by |type| and deletes
  // any entries that are no longer in the list.
  virtual void UpdateManagedList(ManagedState::ManagedType type,
                                 const base::ListValue& entries) OVERRIDE;

  // The list of profiles changed (i.e. a user has logged in). Re-request
  // properties for all services since they may have changed.
  virtual void ProfileListChanged() OVERRIDE;

  // Parses the properties for the network service or device. Mostly calls
  // managed->PropertyChanged(key, value) for each dictionary entry.
  virtual void UpdateManagedStateProperties(
      ManagedState::ManagedType type,
      const std::string& path,
      const base::DictionaryValue& properties) OVERRIDE;

  // Called by ShillPropertyHandler when a watched service property changes.
  virtual void UpdateNetworkServiceProperty(
      const std::string& service_path,
      const std::string& key,
      const base::Value& value) OVERRIDE;

  // Called by ShillPropertyHandler when a watched device property changes.
  virtual void UpdateDeviceProperty(
      const std::string& device_path,
      const std::string& key,
      const base::Value& value) OVERRIDE;

  // Called by ShillPropertyHandler when the portal check list manager property
  // changes.
  virtual void CheckPortalListChanged(
      const std::string& check_portal_list) OVERRIDE;

  // Called by ShillPropertyHandler when a technology list changes.
  virtual void TechnologyListChanged() OVERRIDE;

  // Called by |shill_property_handler_| when the service or device list has
  // changed and all entries have been updated. This updates the list and
  // notifies observers. If |type| == TYPE_NETWORK this also calls
  // CheckDefaultNetworkChanged().
  virtual void ManagedStateListChanged(
      ManagedState::ManagedType type) OVERRIDE;

  // Called after construction. Called explicitly by tests after adding
  // test observers.
  void InitShillPropertyHandler();

 private:
  typedef std::list<base::Closure> ScanCallbackList;
  typedef std::map<std::string, ScanCallbackList> ScanCompleteCallbackMap;
  friend class NetworkStateHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkStateHandlerTest, NetworkStateHandlerStub);

  // NetworkState specific method for UpdateManagedStateProperties which
  // notifies observers.
  void UpdateNetworkStateProperties(NetworkState* network,
                                    const base::DictionaryValue& properties);

  // Sends DeviceListChanged() to observers and logs an event.
  void NotifyDeviceListChanged();

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

  // Helper function to notify observers. Calls CheckDefaultNetworkChanged().
  void OnNetworkConnectionStateChanged(NetworkState* network);

  // If the default network changed returns true and sets
  // |default_network_path_|.
  bool CheckDefaultNetworkChanged();

  // Logs an event and notifies observers.
  void OnDefaultNetworkChanged();

  // Notifies observers about changes to |network|.
  void NetworkPropertiesUpdated(const NetworkState* network);

  // Called whenever Device.Scanning state transitions to false.
  void ScanCompleted(const std::string& type);

  // Returns the technology type for |type|.
  std::string GetTechnologyForType(const NetworkTypePattern& type) const;

  // Shill property handler instance, owned by this class.
  scoped_ptr<internal::ShillPropertyHandler> shill_property_handler_;

  // Observer list
  ObserverList<NetworkStateHandlerObserver> observers_;

  // List of managed network states
  ManagedStateList network_list_;

  // List of managed favorite states; this list includes all entries in
  // Manager.ServiceCompleteList, but only entries with a non-empty Profile
  // property are returned in GetFavoriteList().
  ManagedStateList favorite_list_;

  // List of managed device states
  ManagedStateList device_list_;

  // Keeps track of the default network for notifying observers when it changes.
  std::string default_network_path_;

  // List of interfaces on which portal check is enabled.
  std::string check_portal_list_;

  // Callbacks to run when a scan for the technology type completes.
  ScanCompleteCallbackMap scan_complete_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_H_
