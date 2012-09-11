// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_CHANGE_NOTIFIER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_CHANGE_NOTIFIER_CHROMEOS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chromeos/dbus/power_manager_client.h"
#include "net/base/network_change_notifier.h"

namespace chromeos {

class OnlineStatusReportThreadTask;

class NetworkChangeNotifierChromeos
    : public net::NetworkChangeNotifier,
      public chromeos::PowerManagerClient::Observer,
      public chromeos::NetworkLibrary::NetworkObserver,
      public chromeos::NetworkLibrary::NetworkManagerObserver {
 public:
  NetworkChangeNotifierChromeos();
  virtual ~NetworkChangeNotifierChromeos();

  // Initializes the network change notifier. Starts to observe changes
  // from the power manager and the network manager.
  void Init();

  // Shutdowns the network change notifier. Stops observing changes from
  // the power manager and the network manager.
  void Shutdown();

 private:
  friend class OnlineStatusReportThreadTask;

  class DnsConfigServiceChromeos;

  // PowerManagerClient::Observer overrides.
  virtual void PowerChanged(const PowerSupplyStatus& status) OVERRIDE;

  virtual void SystemResumed() OVERRIDE;

  // NetworkChangeNotifier overrides.
  virtual net::NetworkChangeNotifier::ConnectionType
      GetCurrentConnectionType() const OVERRIDE;

  // NetworkManagerObserver overrides:
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj) OVERRIDE;

  // NetworkObserver overrides:
  virtual void OnNetworkChanged(chromeos::NetworkLibrary* cros,
                                const chromeos::Network* network) OVERRIDE;

  // Initiate online status change reporting.
  void ReportConnectionChange();
  void ReportConnectionChangeOnUIThread();
  // Callback from online_notification_task_ when online state notification
  // is actually scheduled.
  void OnOnlineStateNotificationFired();

  // Updates data members that keep the track the network stack state.
  void UpdateNetworkState(chromeos::NetworkLibrary* cros);
  // Updates network connectivity state.
  void UpdateConnectivityState(const chromeos::Network* network);

  // Updates the initial state. Lets us trigger initial eval of the
  // connectivity status without waiting for an event from the connection
  // manager.
  static void UpdateInitialState(NetworkChangeNotifierChromeos* self);

  // Gets connection type for given |network|.
  static net::NetworkChangeNotifier::ConnectionType GetNetworkConnectionType(
      const chromeos::Network* network);

  // True if we previously had an active network around.
  bool has_active_network_;
  // Current active network's connection state.
  chromeos::ConnectionState connection_state_;
  // Current active network's connection type.
  net::NetworkChangeNotifier::ConnectionType connection_type_;
  // Current active network's service path.
  std::string service_path_;
  // Current active network's IP address.
  std::string ip_address_;

  scoped_ptr<DnsConfigServiceChromeos> dns_config_service_;

  base::WeakPtrFactory<NetworkChangeNotifierChromeos> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_CHANGE_NOTIFIER_CHROMEOS_H_
