// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/network_screen.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "grit/generated_resources.h"
#include "views/widget/widget.h"
#include "views/window/window.h"


namespace {

// Time in seconds for connection timeout.
const int kConnectionTimeoutSec = 15;

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// NetworkScreen, public:

NetworkScreen::NetworkScreen(WizardScreenDelegate* delegate, bool is_out_of_box)
    : ViewScreen<NetworkSelectionView>(delegate),
      is_network_subscribed_(false),
      wifi_disabled_(false),
      is_out_of_box_(is_out_of_box),
      is_waiting_for_connect_(false),
      continue_pressed_(false),
      ethernet_preselected_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
}

NetworkScreen::~NetworkScreen() {
  connection_timer_.Stop();
  UnsubscribeNetworkNotification();
}

////////////////////////////////////////////////////////////////////////////////
// ComboboxModel implementation:

int NetworkScreen::GetItemCount() {
  // Item with index = 0 is either "no networks are available" or
  // "no selection".
  // If WiFi is disabled adding extra item to enable it.
  return static_cast<int>(networks_.GetNetworkCount()) + 1 +
      (wifi_disabled_ ? 1 : 0);
}

std::wstring NetworkScreen::GetItemAt(int index) {
  if (index == 0) {
    return networks_.IsEmpty() ?
        l10n_util::GetString(IDS_STATUSBAR_NO_NETWORKS_MESSAGE) :
        l10n_util::GetString(IDS_NETWORK_SELECTION_NONE);
  }
  if (wifi_disabled_ &&
      index == static_cast<int>(networks_.GetNetworkCount()) + 1) {
    return l10n_util::GetStringF(
        IDS_STATUSBAR_NETWORK_DEVICE_ENABLE,
        l10n_util::GetString(IDS_STATUSBAR_NETWORK_DEVICE_WIFI));
  }
  NetworkList::NetworkItem* network =
      networks_.GetNetworkAt(index - 1);
  return network ? UTF16ToWide(network->label) : std::wstring();
}

////////////////////////////////////////////////////////////////////////////////
// views::Combobox::Listener implementation:

void NetworkScreen::ItemChanged(views::Combobox* sender,
                                int prev_index,
                                int new_index) {
  view()->EnableContinue(new_index > 0);
  // Corner case: item with index 0 is "No selection". Just select it.
  if (new_index == prev_index || new_index <= 0 || prev_index < 0)
    return;

  if (wifi_disabled_ &&
      new_index == static_cast<int>(networks_.GetNetworkCount()) + 1) {
    view()->EnableContinue(false);
    MessageLoop::current()->PostTask(FROM_HERE,
        task_factory_.NewRunnableMethod(&NetworkScreen::EnableWiFi));
    return;
  }

  if (networks_.IsEmpty())
    return;

  // Connect to network as early as possible.
  const NetworkList::NetworkItem* network =
      networks_.GetNetworkAt(new_index - 1);
  MessageLoop::current()->PostTask(FROM_HERE, task_factory_.NewRunnableMethod(
      &NetworkScreen::ConnectToNetwork, network->network_type, network->label));
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void NetworkScreen::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  // Proceed only when selected network is connected.
  const NetworkList::NetworkItem* network = GetSelectedNetwork();
  if (!network)
    return;
  if (networks_.IsNetworkConnected(network->network_type, network->label)) {
    MessageLoop::current()->PostTask(FROM_HERE,
        task_factory_.NewRunnableMethod(&NetworkScreen::NotifyOnConnection));
  } else {
    continue_pressed_ = true;
    if (is_waiting_for_connect_) {
      ShowConnectingStatus();
    } else {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          task_factory_.NewRunnableMethod(&NetworkScreen::ConnectToNetwork,
                                          network->network_type,
                                          network->label));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary::Observer implementation:

void NetworkScreen::NetworkChanged(NetworkLibrary* network_lib) {
  if (!view())
    return;

  // TODO(nkostylev): Reuse network menu button - http://crosbug.com/4133
  wifi_disabled_ = !chromeos::CrosLibrary::Get()->
      GetNetworkLibrary()->wifi_enabled();

  // Save network selection in case it would be available after refresh.
  NetworkList::NetworkType network_type = NetworkList::NETWORK_EMPTY;
  string16 network_id;
  const NetworkList::NetworkItem* selected_network = GetSelectedNetwork();
  if (selected_network) {
    network_type = selected_network->network_type;
    network_id = selected_network->label;
  }
  networks_.NetworkChanged(network_lib);
  if (is_waiting_for_connect_ &&
      networks_.IsNetworkConnected(connecting_network_.network_type,
                                   connecting_network_.label)) {
    // Stop waiting & don't update spinner status.
    StopWaitingForConnection(false);
    if (continue_pressed_) {
      MessageLoop::current()->PostTask(FROM_HERE,
          task_factory_.NewRunnableMethod(&NetworkScreen::NotifyOnConnection));
      return;
    }
  }
  view()->NetworkModelChanged();
  // Prefer Ethernet when it's connected (only once).
  if (!ethernet_preselected_ &&
      networks_.IsNetworkConnected(NetworkList::NETWORK_ETHERNET, string16())) {
    ethernet_preselected_ = true;
    SelectNetwork(NetworkList::NETWORK_ETHERNET, string16());
  } else {
    SelectNetwork(network_type, network_id);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary::Observer implementation:

void NetworkScreen::OnDialogAccepted() {
  const NetworkList::NetworkItem* network = GetSelectedNetwork();
  if (network)
    WaitForConnection(network);
}

void NetworkScreen::OnDialogCancelled() {
  if (view()) {
    view()->EnableContinue(false);
    view()->SetSelectedNetworkItem(0);
  }
}

///////////////////////////////////////////////////////////////////////////////
// NetworkScreen, ViewScreen implementation:

void NetworkScreen::CreateView() {
  language_switch_menu_.InitLanguageMenu();
  ViewScreen<NetworkSelectionView>::CreateView();
}

NetworkSelectionView* NetworkScreen::AllocateView() {
  return new NetworkSelectionView(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, public:

void NetworkScreen::Refresh() {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    SubscribeNetworkNotification();
    NetworkChanged(chromeos::CrosLibrary::Get()->GetNetworkLibrary());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, private:

void NetworkScreen::ConnectToNetwork(NetworkList::NetworkType type,
                                     const string16& id) {
  const NetworkList::NetworkItem* network =
      networks_.GetNetworkById(type, id);
  if (network &&
      !networks_.IsNetworkConnected(type, id)) {
    if (NetworkList::NETWORK_WIFI == network->network_type) {
      if (network->wifi_network.encrypted()) {
        OpenPasswordDialog(network->wifi_network);
        return;
      } else {
        WaitForConnection(network);
        chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
            ConnectToWifiNetwork(network->wifi_network,
                                 std::string(), std::string(), std::string());
      }
    } else if (NetworkList::NETWORK_CELLULAR == network->network_type) {
      WaitForConnection(network);
      chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
          ConnectToCellularNetwork(network->cellular_network);
    }
  }
}

void NetworkScreen::EnableWiFi() {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        EnableWifiNetworkDevice(true);
  }
}

void NetworkScreen::SubscribeNetworkNotification() {
  if (!is_network_subscribed_) {
    is_network_subscribed_ = true;
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->AddObserver(this);
  }
}

void NetworkScreen::UnsubscribeNetworkNotification() {
  if (is_network_subscribed_) {
    is_network_subscribed_ = false;
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->RemoveObserver(this);
  }
}

NetworkList::NetworkItem* NetworkScreen::GetSelectedNetwork() {
  if (!view())
    return NULL;

  return networks_.GetNetworkAt(view()->GetSelectedNetworkItem() - 1);
}

void NetworkScreen::NotifyOnConnection() {
  // TODO(nkostylev): Check network connectivity.
  UnsubscribeNetworkNotification();
  connection_timer_.Stop();
  delegate()->GetObserver(this)->OnExit(ScreenObserver::NETWORK_CONNECTED);
}

void NetworkScreen::OnConnectionTimeout() {
  continue_pressed_ = false;
  // TODO(nkostylev): Notify on connection error.
  if (is_waiting_for_connect_) {
    // Stop waiting & show selection combobox.
    StopWaitingForConnection(true);
  }
}

void NetworkScreen::OpenPasswordDialog(WifiNetwork network) {
  NetworkConfigView* dialog = new NetworkConfigView(network, true);
  dialog->set_delegate(this);
  dialog->set_browser_mode(false);
  views::Window* window = views::Window::CreateChromeWindow(
      view()->GetNativeWindow(), gfx::Rect(), dialog);
  window->SetIsAlwaysOnTop(true);
  window->Show();
  dialog->SetLoginTextfieldFocus();
}

void NetworkScreen::SelectNetwork(NetworkList::NetworkType type,
                                  const string16& id) {
  int index = networks_.GetNetworkIndexById(type, id);
  if (index >= 0) {
    view()->SetSelectedNetworkItem(index + 1);
  } else {
    view()->SetSelectedNetworkItem(0);
  }
}

void NetworkScreen::ShowConnectingStatus() {
  view()->ShowConnectingStatus(is_waiting_for_connect_,
                               connecting_network_.label);
}

void NetworkScreen::StopWaitingForConnection(bool show_combobox) {
  if (connection_timer_.IsRunning())
    connection_timer_.Stop();
  is_waiting_for_connect_ = false;
  connecting_network_.network_type = NetworkList::NETWORK_EMPTY;
  connecting_network_.label.clear();
  if (show_combobox)
    ShowConnectingStatus();
}

void NetworkScreen::WaitForConnection(const NetworkList::NetworkItem* network) {
  is_waiting_for_connect_ = true;
  DCHECK(network);
  connecting_network_.network_type = network->network_type;
  connecting_network_.label = network->label;
  if (connection_timer_.IsRunning())
    connection_timer_.Stop();
  connection_timer_.Start(base::TimeDelta::FromSeconds(kConnectionTimeoutSec),
                          this,
                          &NetworkScreen::OnConnectionTimeout);
  if (continue_pressed_)
    ShowConnectingStatus();
}

}  // namespace chromeos
