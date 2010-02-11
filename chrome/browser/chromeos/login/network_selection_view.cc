// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/network_selection_view.h"

#include <signal.h>
#include <sys/types.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::Label;
using views::View;
using views::Widget;

namespace {

const int kCornerRadius = 12;
const int kWelcomeLabelY = 200;
const int kSpacing = 25;
const int kComboboxSpacing = 5;
const int kHorizontalSpacing = 25;
const int kNetworkComboboxWidth = 250;
const int kNetworkComboboxHeight = 30;
const SkColor kWelcomeColor = SK_ColorGRAY;
const SkColor kBackground = SK_ColorWHITE;

}  // namespace

NetworkSelectionView::NetworkSelectionView(chromeos::ScreenObserver* observer)
    : network_combobox_(NULL),
      welcome_label_(NULL),
      select_network_label_(NULL),
      observer_(observer) {
  chromeos::NetworkLibrary::Get()->AddObserver(this);
}

NetworkSelectionView::~NetworkSelectionView() {
  chromeos::NetworkLibrary::Get()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WizardScreen implementation:

void NetworkSelectionView::Init() {
  // TODO(nkostylev): Add UI language and logo.
  // Use rounded rect background.
  views::Painter* painter = new chromeos::RoundedRectPainter(
      0, kBackground,             // no padding
      true, SK_ColorBLACK,        // black shadow
      kCornerRadius,              // corner radius
      kBackground, kBackground);  // background without gradient
  set_background(views::Background::CreateBackgroundPainter(true, painter));
  gfx::Font welcome_label_font =
      gfx::Font::CreateFont(L"Droid Sans", 14).DeriveFont(0, gfx::Font::BOLD);
  gfx::Font network_label_font = gfx::Font::CreateFont(L"Droid Sans", 9);

  welcome_label_ = new views::Label();
  welcome_label_->SetColor(kWelcomeColor);
  welcome_label_->SetFont(welcome_label_font);

  select_network_label_ = new views::Label();
  select_network_label_->SetFont(network_label_font);

  network_combobox_ = new views::Combobox(this);
  network_combobox_->SetSelectedItem(0);
  network_combobox_->set_listener(this);

  UpdateLocalizedStrings();

  AddChildView(welcome_label_);
  AddChildView(select_network_label_);
  AddChildView(network_combobox_);
  NetworkChanged(chromeos::NetworkLibrary::Get());
}

void NetworkSelectionView::UpdateLocalizedStrings() {
  welcome_label_->SetText(l10n_util::GetStringF(IDS_NETWORK_SELECTION_TITLE,
                          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  select_network_label_->SetText(
      l10n_util::GetString(IDS_NETWORK_SELECTION_SELECT));
  network_combobox_->ModelChanged();
}

////////////////////////////////////////////////////////////////////////////////
// views::View: implementation:

gfx::Size NetworkSelectionView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void NetworkSelectionView::Layout() {
  int x = (width() -
           welcome_label_->GetPreferredSize().width()) / 2;
  int y = kWelcomeLabelY;
  welcome_label_->SetBounds(x,
                            y,
                            welcome_label_->GetPreferredSize().width(),
                            welcome_label_->GetPreferredSize().height());

  x = (width() - select_network_label_->GetPreferredSize().width() -
       kNetworkComboboxWidth) / 2;
  y += welcome_label_->GetPreferredSize().height() + kSpacing;
  select_network_label_->SetBounds(
      x,
      y,
      select_network_label_->GetPreferredSize().width(),
      select_network_label_->GetPreferredSize().height());

  x += select_network_label_->GetPreferredSize().width() + kHorizontalSpacing;
  y -= kComboboxSpacing;
  network_combobox_->SetBounds(x, y,
                               kNetworkComboboxWidth, kNetworkComboboxHeight);

  // Need to refresh combobox layout explicitly.
  network_combobox_->Layout();
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// views::WindowDelegate implementation:

views::View* NetworkSelectionView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// ComboboxModel implementation:

int NetworkSelectionView::GetItemCount() {
  // Item count is always 1 more than number of networks.
  return static_cast<int>(wifi_networks_.size()) + 1;
}

std::wstring NetworkSelectionView::GetItemAt(int index) {
  if (index == 0) {
    return wifi_networks_.empty() ?
        l10n_util::GetString(IDS_STATUSBAR_NO_NETWORKS_MESSAGE) :
        std::wstring();
  }
  // TODO(nkostylev): Sort networks (Ethernet, public WiFi, encrypted WiFi).
  return ASCIIToWide(wifi_networks_[index-1].ssid);
}

////////////////////////////////////////////////////////////////////////////////
// views::Combobox::Listener implementation:

void NetworkSelectionView::ItemChanged(views::Combobox* sender,
                                       int prev_index,
                                       int new_index) {
  if (new_index == prev_index)
    return;

  // First item is empty.
  if (new_index == 0) {
    network_combobox_->SetSelectedItem(prev_index);
    return;
  }

  if (!HasWifiNetworks())
    return;

  chromeos::WifiNetwork selected_network = GetWifiNetworkAt(new_index);
  if (selected_network.encrypted) {
    // TODO(nkostylev): Show password dialog.
  } else {
    chromeos::NetworkLibrary::Get()->ConnectToWifiNetwork(selected_network,
                                                          string16());
  }
  // TODO(avayvod): Check for connection error.
  if (observer_) {
    observer_->OnExit(chromeos::ScreenObserver::NETWORK_CONNECTED);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::Observer implementation:

void NetworkSelectionView::NetworkChanged(chromeos::NetworkLibrary* network) {
  // TODO(nkostylev): Add cellular networks and Ethernet to the list.
  if (network && network->EnsureLoaded()) {
    wifi_networks_ = network->wifi_networks();
  } else {
    wifi_networks_.clear();
  }
  network_combobox_->ModelChanged();
}

void NetworkSelectionView::NetworkTraffic(chromeos::NetworkLibrary* cros,
                                          int traffic_type) {
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSelectionView, private:

bool NetworkSelectionView::HasWifiNetworks() {
  return !wifi_networks_.empty();
}

const chromeos::WifiNetwork& NetworkSelectionView::GetWifiNetworkAt(int index) {
  return wifi_networks_[index-1];
}

