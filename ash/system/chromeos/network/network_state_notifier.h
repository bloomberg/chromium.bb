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
#include "base/time/time.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {
class NetworkState;
}

namespace ash {

// This class has two purposes:
// 1. ShowNetworkConnectError() gets called after any user initiated connect
//    failure. This will handle displaying an error notification.
//    NOTE: Because Shill sets the Error property of a Service *after*
//    the Connect call fails, this class will delay the notification if
//    necessary until the Error property is set so that the correct
//    message can be displayed.
//    TODO(stevenjb): convert this class to use the new MessageCenter
//    notification system, generate a notification immediately, and update
//    it when the Error property is set to guarantee that a notification is
//    displayed for every failure.
// 2. It observes NetworkState changes to generate notifications when a
//    Cellular network is out of credits.
class ASH_EXPORT NetworkStateNotifier :
      public chromeos::NetworkStateHandlerObserver,
      public NetworkTrayDelegate {
 public:
  NetworkStateNotifier();
  virtual ~NetworkStateNotifier();

  // NetworkStateHandlerObserver
  virtual void NetworkListChanged() OVERRIDE;
  virtual void DefaultNetworkChanged(
      const chromeos::NetworkState* network) OVERRIDE;
  virtual void NetworkPropertiesUpdated(
      const chromeos::NetworkState* network) OVERRIDE;

  // NetworkTrayDelegate
  virtual void NotificationLinkClicked(
      NetworkObserver::MessageType message_type,
      size_t link_index) OVERRIDE;

  // Show a connection error notification.
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& service_path);

 private:
  typedef std::map<std::string, std::string> CachedStateMap;

  std::string last_active_network_;
  std::string cellular_network_;
  std::string connect_failed_network_;
  bool cellular_out_of_credits_;
  base::Time out_of_credits_notify_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateNotifier);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_NOTIFIER_H_
