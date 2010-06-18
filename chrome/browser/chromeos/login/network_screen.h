// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_H_

#include <string>

#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/network_screen_delegate.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/network_list.h"
#include "chrome/browser/chromeos/options/network_config_view.h"

class WizardScreenDelegate;

namespace chromeos {

class NetworkSelectionView;

class NetworkScreen : public ViewScreen<NetworkSelectionView>,
                      public NetworkScreenDelegate,
                      public NetworkConfigView::Delegate {
 public:
  NetworkScreen(WizardScreenDelegate* delegate, bool is_out_of_box);
  virtual ~NetworkScreen();

  // NetworkScreenDelegate implementation:
  virtual LanguageSwitchMenu* language_switch_menu() {
    return &language_switch_menu_;
  }

  // ComboboxModel implementation:
  virtual int GetItemCount();
  virtual std::wstring GetItemAt(int index);

  // views::Combobox::Listener implementation:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // NetworkLibrary::Observer implementation:
  virtual void NetworkChanged(NetworkLibrary* network_lib);
  virtual void NetworkTraffic(NetworkLibrary* cros, int traffic_type) {}

  // NetworkConfigView::Delegate implementation:
  virtual void OnDialogAccepted();
  virtual void OnDialogCancelled();

 protected:
  // Subscribes NetworkScreen to the network change notification,
  // forces refresh of current network state.
  void Refresh();

 private:
  // ViewScreen implementation:
  virtual void CreateView();
  virtual NetworkSelectionView* AllocateView();

  // Connects to network if needed and updates screen state.
  void ConnectToNetwork(NetworkList::NetworkType type, const string16& id);

  // Enables WiFi device.
  void EnableWiFi();

  // Subscribes to network change notifications.
  void SubscribeNetworkNotification();

  // Unsubscribes from network change notifications.
  void UnsubscribeNetworkNotification();

  // Returns currently selected network in the combobox.
  NetworkList::NetworkItem* GetSelectedNetwork();

  // Notifies wizard on successful connection.
  void NotifyOnConnection();

  // Called by |connection_timer_| when connection to the network timed out.
  void OnConnectionTimeout();

  // Opens password dialog for the encrypted networks.
  void OpenPasswordDialog(WifiNetwork network);

  // Selects network by type and id.
  void SelectNetwork(NetworkList::NetworkType type,
                     const string16& id);

  // Switches connecting status based on |is_waiting_for_connect_|.
  void ShowConnectingStatus();

  // Stops waiting for network to connect.
  // If |show_combobox| is false, spinner is left on screen. Used on exit.
  void StopWaitingForConnection(bool show_combobox);

  // Starts waiting for network connection. Shows spinner.
  void WaitForConnection(const NetworkList::NetworkItem* network);

  // True if subscribed to network change notification.
  bool is_network_subscribed_;

  // Networks model, contains current state of available networks.
  NetworkList networks_;

  // True if WiFi is currently disabled.
  bool wifi_disabled_;

  // True if full OOBE flow should be shown.
  bool is_out_of_box_;

  // True if we're waiting for the selected network being connected.
  bool is_waiting_for_connect_;

  // True if "Continue" button was pressed.
  // Set only when there's a network selected.
  bool continue_pressed_;

  // True if Ethernet was already preselected in combobox.
  bool ethernet_preselected_;

  // Timer for connection timeout.
  base::OneShotTimer<NetworkScreen> connection_timer_;

  // Network which we're connecting to.
  NetworkList::NetworkItem connecting_network_;

  ScopedRunnableMethodFactory<NetworkScreen> task_factory_;
  LanguageSwitchMenu language_switch_menu_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_H_
