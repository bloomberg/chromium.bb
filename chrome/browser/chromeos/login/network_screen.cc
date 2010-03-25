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
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "grit/generated_resources.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// NetworkScreen, public:

NetworkScreen::NetworkScreen(WizardScreenDelegate* delegate, bool is_out_of_box)
    : ViewScreen<NetworkSelectionView>(delegate),
      is_network_subscribed_(false),
      is_out_of_box_(is_out_of_box),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
}

NetworkScreen::~NetworkScreen() {
  UnsubscribeNetworkNotification();
}

////////////////////////////////////////////////////////////////////////////////
// ComboboxModel implementation:

int NetworkScreen::GetItemCount() {
  // Item with index = 0 is either "no networks are available" or
  // "no selection".
  return static_cast<int>(networks_.GetNetworkCount()) + 1;
}

std::wstring NetworkScreen::GetItemAt(int index) {
  if (index == 0) {
    return networks_.IsEmpty() ?
        l10n_util::GetString(IDS_STATUSBAR_NO_NETWORKS_MESSAGE) :
        l10n_util::GetString(IDS_NETWORK_SELECTION_NONE);
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
  if (new_index == prev_index || new_index < 0 || prev_index < 0)
    return;

  // First item is a text, not a network.
  if (new_index == 0) {
    view()->SetSelectedNetworkItem(prev_index);
    return;
  }

  if (networks_.IsEmpty())
    return;

  const NetworkList::NetworkItem* network =
      networks_.GetNetworkAt(new_index - 1);
  MessageLoop::current()->PostTask(FROM_HERE, task_factory_.NewRunnableMethod(
      &NetworkScreen::ConnectToNetwork, network->network_type, network->label));
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void NetworkScreen::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  MessageLoop::current()->PostTask(FROM_HERE,
      task_factory_.NewRunnableMethod(&NetworkScreen::NotifyOnOffline));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary::Observer implementation:

void NetworkScreen::NetworkChanged(NetworkLibrary* network_lib) {
  // Save network selection in case it would be available after refresh.
  NetworkList::NetworkType network_type = NetworkList::NETWORK_EMPTY;
  string16 network_id;
  const NetworkList::NetworkItem* selected_network = GetSelectedNetwork();
  if (selected_network) {
    network_type = selected_network->network_type;
    network_id = selected_network->label;
  }
  networks_.NetworkChanged(network_lib);
  // TODO(nkostylev): Check for connection error.
  // For initial out of box experience make sure that network was selected.
  if (networks_.ConnectedNetwork() &&
      (selected_network || !is_out_of_box_)) {
    MessageLoop::current()->PostTask(FROM_HERE,
        task_factory_.NewRunnableMethod(&NetworkScreen::NotifyOnConnection));
    return;
  }
  const NetworkList::NetworkItem* network = networks_.ConnectingNetwork();
  if (network &&
      (selected_network || !is_out_of_box_)) {
    view()->ShowConnectingStatus(true, network->label);
  }
  view()->NetworkModelChanged();
  SelectNetwork(network_type, network_id);
}

void NetworkScreen::NetworkTraffic(NetworkLibrary* cros,
                                   int traffic_type) {
}

///////////////////////////////////////////////////////////////////////////////
// NetworkScreen, ViewScreen implementation:

void NetworkScreen::CreateView() {
  ViewScreen<NetworkSelectionView>::CreateView();
}

NetworkSelectionView* NetworkScreen::AllocateView() {
  return new NetworkSelectionView(delegate()->GetObserver(this), this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, public:

void NetworkScreen::Refresh() {
  SubscribeNetworkNotification();
  NetworkChanged(chromeos::CrosLibrary::Get()->GetNetworkLibrary());
}

////////////////////////////////////////////////////////////////////////////////
// NetworkScreen, private:

void NetworkScreen::ConnectToNetwork(NetworkList::NetworkType type,
                                     const string16& id) {
  const NetworkList::NetworkItem* network =
      networks_.GetNetworkById(type, id);
  if (network && !IsSameNetwork(network, networks_.ConnectedNetwork())) {
    if (NetworkList::NETWORK_WIFI == network->network_type) {
      if (network->wifi_network.encrypted) {
        OpenPasswordDialog(network->wifi_network);
        return;
      } else {
        chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
            ConnectToWifiNetwork(network->wifi_network, string16());
      }
    } else if (NetworkList::NETWORK_CELLULAR == network->network_type) {
      chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
          ConnectToCellularNetwork(network->cellular_network);
    }
  }
  // Check if there's already connected network and update screen state.
  MessageLoop::current()->PostTask(FROM_HERE, task_factory_.NewRunnableMethod(
      &NetworkScreen::NetworkChanged,
      chromeos::CrosLibrary::Get()->GetNetworkLibrary()));
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
  return networks_.GetNetworkAt(view()->GetSelectedNetworkItem() - 1);
}

bool NetworkScreen::IsSameNetwork(const NetworkList::NetworkItem* network1,
                                  const NetworkList::NetworkItem* network2) {
  return (network1 && network2 &&
          network1->network_type == network2->network_type &&
          network1->label == network2->label);
}

void NetworkScreen::NotifyOnConnection() {
  UnsubscribeNetworkNotification();
  delegate()->GetObserver(this)->OnExit(ScreenObserver::NETWORK_CONNECTED);
}

void NetworkScreen::NotifyOnOffline() {
  UnsubscribeNetworkNotification();
  delegate()->GetObserver(this)->OnExit(ScreenObserver::NETWORK_OFFLINE);
}

void NetworkScreen::OpenPasswordDialog(WifiNetwork network) {
  NetworkConfigView* dialog = new NetworkConfigView(network, true);
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

}  // namespace chromeos
