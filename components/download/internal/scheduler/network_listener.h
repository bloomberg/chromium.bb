// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_NETWORK_LISTENER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_NETWORK_LISTENER_H_

#include "net/base/network_change_notifier.h"

namespace download {

// Listens to network status change and notifies observers in download service.
class NetworkListener
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // NetworkStatus should mostly one to one map to
  // SchedulingParams::NetworkRequirements. Has coarser granularity than
  // network connection type.
  enum class NetworkStatus {
    DISCONNECTED = 0,
    UNMETERED = 1,  // WIFI or Ethernet.
    METERED = 2,    // Mobile networks.
  };

  class Observer {
   public:
    // Called when network status is changed.
    virtual void OnNetworkChange(NetworkStatus network_status) = 0;
  };

  NetworkListener();
  ~NetworkListener() override;

  // Returns the current network status for download scheduling.
  NetworkStatus CurrentNetworkStatus() const;

  // Starts/stops to listen network change events.
  void Start();
  void Stop();

  // Adds/Removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // net::NetworkChangeNotifier implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Notifies |observers_| about network status change. See |NetworkStatus|.
  void NotifyNetworkChange(NetworkStatus network_status);

  // The current network status.
  NetworkStatus status_;

  // If we are actively listening to network change events.
  bool listening_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(NetworkListener);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_NETWORK_LISTENER_H_
