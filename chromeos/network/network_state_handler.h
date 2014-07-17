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
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/managed_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_type_pattern.h"
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
// Some notes about NetworkState and GUIDs:
// * A NetworkState exists for all network services stored in a profile, and
//   all "visible" networks (physically connected networks like ethernet and
//   cellular or in-range wifi networks). If the network is stored in a profile,
//   NetworkState.IsInProfile() will return true.
// * "Visible" networks return true for NetworkState.visible().
// * All networks saved to a profile will have a saved GUID that is persistent
//   across sessions.
// * Networks that are not saved to a profile will have a GUID assigned when
//   the initial properties are received. The GUID will be consistent for
//   the duration of a session, even if the network drops out and returns.

class CHROMEOS_EXPORT NetworkStateHandler
    : public internal::ShillPropertyHandler::Listener {
 public:
  typedef std::vector<ManagedState*> ManagedStateList;
  typedef std::vector<const NetworkState*> NetworkStateList;
  typedef std::vector<const DeviceState*> DeviceStateList;

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

  // Returns the default network (which includes VPNs) based on the Shill
  // Manager.DefaultNetwork property. Normally this is the same as
  // ConnectedNetworkByType(NetworkTypePattern::Default()), but the timing might
  // differ.
  const NetworkState* DefaultNetwork() const;

  // Returns the primary connected network of matching |type|, otherwise NULL.
  const NetworkState* ConnectedNetworkByType(
      const NetworkTypePattern& type) const;

  // Like ConnectedNetworkByType() but returns a connecting network or NULL.
  const NetworkState* ConnectingNetworkByType(
      const NetworkTypePattern& type) const;

  // Like ConnectedNetworkByType() but returns any matching visible network or
  // NULL. Mostly useful for mobile networks where there is generally only one
  // network. Note: O(N).
  const NetworkState* FirstNetworkByType(const NetworkTypePattern& type);

  // Returns the aa:bb formatted hardware (MAC) address for the first connected
  // network matching |type|, or an empty string if none is connected.
  std::string FormattedHardwareAddressForType(
      const NetworkTypePattern& type) const;

  // Convenience method to call GetNetworkListByType(visible=true).
  void GetVisibleNetworkListByType(const NetworkTypePattern& type,
                                   NetworkStateList* list);

  // Convenience method for GetVisibleNetworkListByType(Default).
  void GetVisibleNetworkList(NetworkStateList* list);

  // Sets |list| to contain the list of networks with matching |type| and the
  // following properties:
  // |configured_only| - if true only include networks where IsInProfile is true
  // |visible_only| - if true only include networks in the visible Services list
  // |limit| - if > 0 limits the number of results.
  // The returned list contains a copy of NetworkState pointers which should not
  // be stored or used beyond the scope of the calling function (i.e. they may
  // later become invalid, but only on the UI thread). SortNetworkList() will be
  // called if necessary to provide the states in a convenient order (see
  // SortNetworkList for details).
  void GetNetworkListByType(const NetworkTypePattern& type,
                            bool configured_only,
                            bool visible_only,
                            int limit,
                            NetworkStateList* list);

  // Finds and returns the NetworkState associated with |service_path| or NULL
  // if not found. If |configured_only| is true, only returns saved entries
  // (IsInProfile is true).
  const NetworkState* GetNetworkStateFromServicePath(
      const std::string& service_path,
      bool configured_only) const;

  // Finds and returns the NetworkState associated with |guid| or NULL if not
  // found. This returns all entries (IsInProfile() may be true or false).
  const NetworkState* GetNetworkStateFromGuid(const std::string& guid) const;

  // Sets |list| to contain the list of devices.  The returned list contains
  // a copy of DeviceState pointers which should not be stored or used beyond
  // the scope of the calling function (i.e. they may later become invalid, but
  // only on the UI thread).
  void GetDeviceList(DeviceStateList* list) const;

  // Like GetDeviceList() but only returns networks with matching |type|.
  void GetDeviceListByType(const NetworkTypePattern& type,
                           DeviceStateList* list) const;

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

  // Clear the last_error value for the NetworkState for |service_path|.
  void ClearLastErrorForNetwork(const std::string& service_path);

  // Set the list of devices on which portal check is enabled.
  void SetCheckPortalList(const std::string& check_portal_list);

  const std::string& GetCheckPortalListForTest() const {
    return check_portal_list_;
  }

  // Returns the NetworkState of the EthernetEAP service, which contains the
  // EAP parameters used by the ethernet with |service_path|. If |service_path|
  // doesn't refer to an ethernet service or if the ethernet service is not
  // connected using EAP, returns NULL.
  const NetworkState* GetEAPForEthernet(const std::string& service_path);

  const std::string& default_network_path() const {
    return default_network_path_;
  }

  // Construct and initialize an instance for testing.
  static NetworkStateHandler* InitializeForTest();

  // Default set of comma separated interfaces on which to enable
  // portal checking.
  static const char kDefaultCheckPortalList[];

 protected:
  friend class NetworkHandler;
  NetworkStateHandler();

  // ShillPropertyHandler::Listener overrides.

  // This adds new entries to |network_list_| or |device_list_| and deletes any
  // entries that are no longer in the list.
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

  // Called by ShillPropertyHandler when a watched network or device
  // IPConfig property changes.
  virtual void UpdateIPConfigProperties(
      ManagedState::ManagedType type,
      const std::string& path,
      const std::string& ip_config_path,
      const base::DictionaryValue& properties) OVERRIDE;

  // Called by ShillPropertyHandler when the portal check list manager property
  // changes.
  virtual void CheckPortalListChanged(
      const std::string& check_portal_list) OVERRIDE;

  // Called by ShillPropertyHandler when a technology list changes.
  virtual void TechnologyListChanged() OVERRIDE;

  // Called by |shill_property_handler_| when the service or device list has
  // changed and all entries have been updated. This updates the list and
  // notifies observers.
  virtual void ManagedStateListChanged(
      ManagedState::ManagedType type) OVERRIDE;

  // Called when the default network service changes. Sets default_network_path_
  // and notifies listeners.
  virtual void DefaultNetworkServiceChanged(
      const std::string& service_path) OVERRIDE;

  // Called after construction. Called explicitly by tests after adding
  // test observers.
  void InitShillPropertyHandler();

 private:
  typedef std::list<base::Closure> ScanCallbackList;
  typedef std::map<std::string, ScanCallbackList> ScanCompleteCallbackMap;
  typedef std::map<std::string, std::string> SpecifierGuidMap;
  friend class NetworkStateHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkStateHandlerTest, NetworkStateHandlerStub);

  // Sorts the network list. Called when all network updates have been received,
  // or when the network list is requested but the list is in an unsorted state.
  // Networks are sorted as follows, maintaining the existing relative ordering:
  // * Connected or connecting networks (should be listed first by Shill)
  // * Visible non-wifi networks
  // * Visible wifi networks
  // * Hidden (wifi) networks
  void SortNetworkList();

  // Updates UMA stats. Called once after all requested networks are updated.
  void UpdateNetworkStats();

  // NetworkState specific method for UpdateManagedStateProperties which
  // notifies observers.
  void UpdateNetworkStateProperties(NetworkState* network,
                                    const base::DictionaryValue& properties);

  // Ensure a valid GUID for NetworkState.
  void UpdateGuid(NetworkState* network);

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

  // Notifies observers when the default network or its properties change.
  void NotifyDefaultNetworkChanged(const NetworkState* default_network);

  // Notifies observers about changes to |network|, including IPConfg.
  void NotifyNetworkPropertiesUpdated(const NetworkState* network);

  // Notifies observers about changes to |device|, including IPConfigs.
  void NotifyDevicePropertiesUpdated(const DeviceState* device);

  // Called whenever Device.Scanning state transitions to false.
  void ScanCompleted(const std::string& type);

  // Returns one technology type for |type|. This technology will be the
  // highest priority technology in the type pattern.
  std::string GetTechnologyForType(const NetworkTypePattern& type) const;

  // Returns all the technology types for |type|.
  ScopedVector<std::string> GetTechnologiesForType(
      const NetworkTypePattern& type) const;

  // Shill property handler instance, owned by this class.
  scoped_ptr<internal::ShillPropertyHandler> shill_property_handler_;

  // Observer list
  ObserverList<NetworkStateHandlerObserver> observers_;

  // List of managed network states
  ManagedStateList network_list_;

  // Set to true when the network list is sorted, cleared when network updates
  // arrive. Used to trigger sorting when needed.
  bool network_list_sorted_;

  // List of managed device states
  ManagedStateList device_list_;

  // Keeps track of the default network for notifying observers when it changes.
  std::string default_network_path_;

  // List of interfaces on which portal check is enabled.
  std::string check_portal_list_;

  // Callbacks to run when a scan for the technology type completes.
  ScanCompleteCallbackMap scan_complete_callbacks_;

  // Map of network specifiers to guids. Contains an entry for each
  // NetworkState that is not saved in a profile.
  SpecifierGuidMap specifier_guid_map_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_H_
