// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/network_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"

namespace chromeos {

class NetworkScreen : public WizardScreen,
                      public NetworkLibrary::NetworkManagerObserver,
                      public NetworkScreenActor::Delegate {
 public:
  NetworkScreen(ScreenObserver* screen_observer, NetworkScreenActor* actor);
  virtual ~NetworkScreen();

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // NetworkLibrary::NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(NetworkLibrary* network_lib) OVERRIDE;

  // NetworkScreenActor::Delegate implementation:
  virtual void OnActorDestroyed(NetworkScreenActor* actor) OVERRIDE;
  virtual void OnContinuePressed() OVERRIDE;

  NetworkScreenActor* actor() const { return actor_; }

 protected:
  // Subscribes NetworkScreen to the network change notification,
  // forces refresh of current network state.
  virtual void Refresh();

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest, Timeout);

  // Subscribes to network change notifications.
  void SubscribeNetworkNotification();

  // Unsubscribes from network change notifications.
  void UnsubscribeNetworkNotification();

  // Notifies wizard on successful connection.
  void NotifyOnConnection();

  // Called by |connection_timer_| when connection to the network timed out.
  void OnConnectionTimeout();

  // Update UI based on current network status.
  void UpdateStatus(NetworkLibrary* network);

  // Stops waiting for network to connect.
  void StopWaitingForConnection(const string16& network_id);

  // Starts waiting for network connection. Shows spinner.
  void WaitForConnection(const string16& network_id);

  // True if subscribed to network change notification.
  bool is_network_subscribed_;

  // ID of the the network that we are waiting for.
  string16 network_id_;

  // True if user pressed continue button so we should proceed with OOBE
  // as soon as we are connected.
  bool continue_pressed_;

  // Timer for connection timeout.
  base::OneShotTimer<NetworkScreen> connection_timer_;

  NetworkScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_H_
