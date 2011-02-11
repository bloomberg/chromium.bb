// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_H_
#pragma once

#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/keyboard_switch_menu.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/network_screen_delegate.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/network_list.h"
#include "chrome/browser/chromeos/options/network_config_view.h"

class WizardScreenDelegate;

namespace chromeos {

class HelpAppLauncher;
class NetworkSelectionView;

class NetworkScreen : public ViewScreen<NetworkSelectionView>,
                      public MessageBubbleDelegate,
                      public NetworkScreenDelegate {
 public:
  explicit NetworkScreen(WizardScreenDelegate* delegate);
  virtual ~NetworkScreen();

  // NetworkScreenDelegate implementation:
  virtual void ClearErrors();
  virtual bool is_error_shown() { return bubble_ != NULL; }
  virtual LanguageSwitchMenu* language_switch_menu() {
    return &language_switch_menu_;
  }
  virtual KeyboardSwitchMenu* keyboard_switch_menu() {
    return &keyboard_switch_menu_;
  }
  virtual gfx::Size size() const { return GetScreenSize(); }

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // NetworkLibrary::NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(NetworkLibrary* network_lib);

 protected:
  // Subscribes NetworkScreen to the network change notification,
  // forces refresh of current network state.
  void Refresh();

 private:
  FRIEND_TEST(NetworkScreenTest, Timeout);

  // ViewScreen implementation:
  virtual void CreateView();
  virtual NetworkSelectionView* AllocateView();

  // Overridden from views::InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) { bubble_ = NULL; }
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return false; }
  virtual void OnHelpLinkActivated();

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

  LanguageSwitchMenu language_switch_menu_;
  KeyboardSwitchMenu keyboard_switch_menu_;

  // Pointer to shown message bubble. We don't need to delete it because
  // it will be deleted on bubble closing.
  MessageBubble* bubble_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_H_
