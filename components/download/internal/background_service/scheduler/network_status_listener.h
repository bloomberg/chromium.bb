// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_NETWORK_STATUS_LISTENER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_NETWORK_STATUS_LISTENER_H_

#include "services/network/public/cpp/network_connection_tracker.h"

namespace download {

// Monitor and propagate network status change events.
// Base class only manages the observer pointer, derived class should override
// to provide actual network hook to monitor the changes, and call base class
// virtual functions.
class NetworkStatusListener {
 public:
  // Observer to receive network connection type change notifications.
  class Observer {
   public:
    virtual void OnNetworkChanged(network::mojom::ConnectionType type) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Starts to listen to network changes.
  virtual void Start(Observer* observer);

  // Stops to listen to network changes.
  virtual void Stop();

  // Gets the current connection type.
  virtual network::mojom::ConnectionType GetConnectionType() = 0;

  virtual ~NetworkStatusListener() {}

 protected:
  NetworkStatusListener();

  // The only observer that listens to connection type change.
  Observer* observer_ = nullptr;

  // The current network status.
  network::mojom::ConnectionType network_status_ =
      network::mojom::ConnectionType::CONNECTION_NONE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStatusListener);
};

// Default implementation of NetworkStatusListener using
// NetworkConnectionTracker to listen to connectivity changes.
class NetworkStatusListenerImpl
    : public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public NetworkStatusListener {
 public:
  explicit NetworkStatusListenerImpl(
      network::NetworkConnectionTracker* network_connection_tracker);
  ~NetworkStatusListenerImpl() override;

  // NetworkStatusListener implementation.
  void Start(NetworkStatusListener::Observer* observer) override;
  void Stop() override;
  network::mojom::ConnectionType GetConnectionType() override;

 private:
  // network::NetworkConnectionTracker::NetworkConnectionObserver.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  network::NetworkConnectionTracker* network_connection_tracker_;
  network::mojom::ConnectionType connection_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStatusListenerImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_NETWORK_STATUS_LISTENER_H_
