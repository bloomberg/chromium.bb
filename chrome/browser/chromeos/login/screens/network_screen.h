// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/network_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

namespace login {
class NetworkStateHelper;
}  // namespace login

class NetworkScreen : public WizardScreen,
                      public NetworkStateHandlerObserver,
                      public NetworkScreenActor::Delegate {
 public:
  NetworkScreen(ScreenObserver* screen_observer, NetworkScreenActor* actor);
  virtual ~NetworkScreen();

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // NetworkStateHandlerObserver implementation:
  virtual void NetworkConnectionStateChanged(
      const NetworkState* network) OVERRIDE;
  virtual void DefaultNetworkChanged(const NetworkState* network) OVERRIDE;

  // NetworkScreenActor::Delegate implementation:
  virtual void OnActorDestroyed(NetworkScreenActor* actor) OVERRIDE;
  virtual void OnContinuePressed() OVERRIDE;

  NetworkScreenActor* actor() const { return actor_; }

 protected:
  // Subscribes NetworkScreen to the network change notification,
  // forces refresh of current network state.
  virtual void Refresh();

 private:
  friend class NetworkScreenTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest, CanConnect);

  // Sets the NetworkStateHelper for use in tests. This
  // class will take ownership of the pointed object.
  void SetNetworkStateHelperForTest(login::NetworkStateHelper* helper);

  // Subscribes to network change notifications.
  void SubscribeNetworkNotification();

  // Unsubscribes from network change notifications.
  void UnsubscribeNetworkNotification();

  // Notifies wizard on successful connection.
  void NotifyOnConnection();

  // Called by |connection_timer_| when connection to the network timed out.
  void OnConnectionTimeout();

  // Update UI based on current network status.
  void UpdateStatus();

  // Stops waiting for network to connect.
  void StopWaitingForConnection(const base::string16& network_id);

  // Starts waiting for network connection. Shows spinner.
  void WaitForConnection(const base::string16& network_id);

  // True if subscribed to network change notification.
  bool is_network_subscribed_;

  // ID of the the network that we are waiting for.
  base::string16 network_id_;

  // True if user pressed continue button so we should proceed with OOBE
  // as soon as we are connected.
  bool continue_pressed_;

  // Timer for connection timeout.
  base::OneShotTimer<NetworkScreen> connection_timer_;

  NetworkScreenActor* actor_;
  scoped_ptr<login::NetworkStateHelper> network_state_helper_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_H_
