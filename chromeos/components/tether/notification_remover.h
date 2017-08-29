// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_NOTIFICATION_REMOVER_H_
#define CHROMEOS_COMPONENTS_TETHER_NOTIFICATION_REMOVER_H_

#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/host_scan_cache.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

class NotificationPresenter;

// Removes "Available Hotspot" notifications when there are no potential
// hotspots nearby, or when the device connects to a network, or when the Active
// Host status changes to "connected" or "connecting", and removes "Available
// Hotspot", "Setup Required", and "Connection Failed" notifications when it is
// destroyed.
class NotificationRemover : public HostScanCache::Observer,
                            public NetworkStateHandlerObserver,
                            public ActiveHost::Observer {
 public:
  NotificationRemover(NetworkStateHandler* network_state_handler,
                      NotificationPresenter* notification_presenter,
                      HostScanCache* host_scan_cache,
                      ActiveHost* active_host);

  ~NotificationRemover() override;

  // HostScanCache::Observer:
  void OnCacheBecameEmpty() override;

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& active_host_change_info) override;

 private:
  NetworkStateHandler* network_state_handler_;
  NotificationPresenter* notification_presenter_;
  HostScanCache* host_scan_cache_;
  ActiveHost* active_host_;

  DISALLOW_COPY_AND_ASSIGN(NotificationRemover);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_NOTIFICATION_REMOVER_H_
