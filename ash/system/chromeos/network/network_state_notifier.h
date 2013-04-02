// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_NOTIFIER_H_
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_NOTIFIER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/system/chromeos/network/network_tray_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace ash {
namespace internal {

// This class observes NetworkStateHandler and generates notifications
// on connection failures.
class ASH_EXPORT NetworkStateNotifier :
      public chromeos::NetworkStateHandlerObserver,
      public NetworkTrayDelegate {
 public:
  NetworkStateNotifier();
  virtual ~NetworkStateNotifier();

  // NetworkStateHandlerObserver
  virtual void DefaultNetworkChanged(
      const chromeos::NetworkState* network) OVERRIDE;
  virtual void NetworkConnectionStateChanged(
      const chromeos::NetworkState* network) OVERRIDE;
  virtual void NetworkPropertiesUpdated(
      const chromeos::NetworkState* network) OVERRIDE;

  // NetworkTrayDelegate
  virtual void NotificationLinkClicked(
      NetworkObserver::MessageType message_type,
      size_t link_index) OVERRIDE;

 private:
  typedef std::map<std::string, std::string> CachedStateMap;

  void InitializeNetworks();

  CachedStateMap cached_state_;
  std::string last_active_network_;
  std::string cellular_network_;
  bool cellular_out_of_credits_;
  base::Time out_of_credits_notify_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateNotifier);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_NOTIFIER_H_
