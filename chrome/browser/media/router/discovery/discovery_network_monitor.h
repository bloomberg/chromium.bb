// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_MONITOR_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_MONITOR_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/media/router/discovery/discovery_network_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"

// Tracks the set of active network interfaces that can be used for local
// discovery.  If the list of interfaces changes, then
// DiscoveryNetworkMonitor::Observer is called with the instance of the monitor.
// Only one instance of this will be created per browser process.
//
// This class is not thread-safe, except for adding and removing observers.
// Most of the work done by the monitor is done on the IO thread, which includes
// updating the current network ID.  Therefore |GetNetworkId| should only be
// called from the IO thread.  All observers will be notified of network changes
// on the thread from which they registered.
class DiscoveryNetworkMonitor
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  using NetworkInfoFunction = std::vector<DiscoveryNetworkInfo> (*)();
  using NetworkRefreshCompleteCallback = base::Callback<void()>;
  class Observer {
   public:
    // Called when the list of connected interfaces has changed.
    virtual void OnNetworksChanged(const DiscoveryNetworkMonitor& monitor) = 0;

   protected:
    ~Observer() = default;
  };

  // Constants for the special states of network Id.
  // Note: The extra const stops MSVC from thinking this can't be
  // constexpr.
  static constexpr char const kNetworkIdDisconnected[] = "__disconnected__";
  static constexpr char const kNetworkIdUnknown[] = "__unknown__";

  static DiscoveryNetworkMonitor* GetInstance();

  void RebindNetworkChangeObserverForTest();
  void SetNetworkInfoFunctionForTest(NetworkInfoFunction);

  void AddObserver(Observer* const observer);
  void RemoveObserver(Observer* const observer);

  // Forces a query of the current network state.  |callback| will be called
  // after the refresh.  This can be called from any thread and |callback| will
  // be executed on the calling thread.
  void Refresh(NetworkRefreshCompleteCallback callback);

  // Computes a stable string identifier from the list of connected interfaces.
  // Returns kNetworkIdDisconnected if there are no connected interfaces or
  // kNetworkIdUnknown if the identifier could not be computed.  This should be
  // called from the IO thread.
  const std::string& GetNetworkId() const;

 private:
  friend class DiscoveryNetworkMonitorTest;
  friend struct base::LazyInstanceTraitsBase<DiscoveryNetworkMonitor>;

  DiscoveryNetworkMonitor();
  ~DiscoveryNetworkMonitor() override;

  // net::NetworkChangeNotifier::NetworkChangeObserver
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  void UpdateNetworkInfo();

  // A hashed representation of the set of networks to which we are connected.
  // This may also be |kNetworkIdDisconnected| if no interfaces are connected or
  // |kNetworkIdUnknown| if we can't determine the set of networks.
  std::string network_id_;

  // The list of observers which have registered interest in when |network_id_|
  // changes.
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;

  // Function used to get information about the networks to which we are
  // connected.
  NetworkInfoFunction network_info_function_;

  DISALLOW_COPY_AND_ASSIGN(DiscoveryNetworkMonitor);
};

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_MONITOR_H_
