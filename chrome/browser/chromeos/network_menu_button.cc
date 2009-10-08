// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_menu_button.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

// static
const int NetworkMenuButton::kNumWifiImages = 8;
const int NetworkMenuButton::kAnimationDelayMillis = 100;

NetworkMenuButton::NetworkMenuButton(Browser* browser)
    : MenuButton(NULL, std::wstring(), this, false),
      refreshing_menu_(false),
      network_menu_(this),
      browser_(browser),
      icon_animation_index_(0),
      icon_animation_increasing_(true) {
  SetShowHighlighted(false);
  UpdateIcon();
  CrosNetworkLibrary::Get()->AddObserver(this);
}

NetworkMenuButton::~NetworkMenuButton() {
  CrosNetworkLibrary::Get()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::Menu2Model implementation:

int NetworkMenuButton::GetItemCount() const {
  // The menu contains the available wifi networks. If there are none, then it
  // only has one item with a message that no networks are available.
  return wifi_networks_.empty() ? 1 : static_cast<int>(wifi_networks_.size());
}

views::Menu2Model::ItemType NetworkMenuButton::GetTypeAt(int index) const {
  return wifi_networks_.empty() ? views::Menu2Model::TYPE_COMMAND :
      views::Menu2Model::TYPE_CHECK;
}

string16 NetworkMenuButton::GetLabelAt(int index) const {
  return wifi_networks_.empty() ?
      l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_NETWORKS_MESSAGE) :
      ASCIIToUTF16(wifi_networks_[index].ssid);
}

bool NetworkMenuButton::IsItemCheckedAt(int index) const {
  // WifiNetwork that we are connected to (or connecting to) is checked.
  return wifi_networks_.empty() ? false :
      wifi_networks_[index].ssid == CrosNetworkLibrary::Get()->wifi_ssid();
}

bool NetworkMenuButton::IsEnabledAt(int index) const {
  return !wifi_networks_.empty();
}

void NetworkMenuButton::ActivatedAt(int index) {
  // When we are refreshing the menu, ignore menu item activation.
  if (refreshing_menu_)
    return;

  CrosNetworkLibrary* cros = CrosNetworkLibrary::Get();

  // If clicked on a network that we are already connected to or we are
  // currently trying to connect to, then do nothing.
  if (wifi_networks_[index].ssid == cros->wifi_ssid())
    return;

  activated_wifi_network_ = wifi_networks_[index];

  // If wifi network is not encrypted, then directly connect.
  // Otherwise, we open password dialog window.
  if (!wifi_networks_[index].encrypted) {
    cros->ConnectToWifiNetwork(wifi_networks_[index], string16());
  } else {
    gfx::NativeWindow parent = browser_->window()->GetNativeHandle();
    PasswordDialogView* dialog = new PasswordDialogView(this,
        wifi_networks_[index].ssid);
    views::Window* window = views::Window::CreateChromeWindow(
        parent, gfx::Rect(), dialog);
    // Draw the password dialog right below this button and right aligned.
    gfx::Size size = dialog->GetPreferredSize();
    gfx::Rect rect = bounds();
    gfx::Point point = gfx::Point(rect.width() - size.width(), rect.height());
    ConvertPointToScreen(this, &point);
    window->SetBounds(gfx::Rect(point, size), parent);
    window->Show();
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, PasswordDialogDelegate implementation:

bool NetworkMenuButton::OnPasswordDialogAccept(const std::string& ssid,
                                               const string16& password) {
  CrosNetworkLibrary::Get()->ConnectToWifiNetwork(activated_wifi_network_,
                                                  password);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::ViewMenuDelegate implementation:

void NetworkMenuButton::RunMenu(views::View* source, const gfx::Point& pt,
                                gfx::NativeView hwnd) {
  wifi_networks_ = CrosNetworkLibrary::Get()->GetWifiNetworks();
  refreshing_menu_ = true;
  network_menu_.Rebuild();
  network_menu_.UpdateStates();
  refreshing_menu_ = false;
  network_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, CrosNetworkLibrary::Observer implementation:

void NetworkMenuButton::NetworkChanged(CrosNetworkLibrary* obj) {
  if (CrosNetworkLibrary::Get()->wifi_connecting()) {
    StartConnectingAnimation();
  } else {
    StopConnectingAnimation();
    UpdateIcon();
  }
}

void NetworkMenuButton::StartConnectingAnimation() {
  if (!timer_.IsRunning()) {
    icon_animation_index_ = 0;
    icon_animation_increasing_ = true;
    timer_.Start(base::TimeDelta::FromMilliseconds(kAnimationDelayMillis), this,
                 &NetworkMenuButton::UpdateIcon);
  }
}

void NetworkMenuButton::StopConnectingAnimation() {
  if (timer_.IsRunning()) {
    timer_.Stop();
  }
}

void NetworkMenuButton::UpdateIcon() {
  CrosNetworkLibrary* cros = CrosNetworkLibrary::Get();
  int id = IDR_STATUSBAR_DISCONNECTED;
  if (cros->wifi_connecting()) {
    // Get the next frame. Reverse direction if necessary.
    if (icon_animation_increasing_) {
      icon_animation_index_++;
      if (icon_animation_index_ >= kNumWifiImages) {
        icon_animation_index_ = kNumWifiImages - 1;
        icon_animation_increasing_ = false;
      }
    } else {
      icon_animation_index_--;
      if (icon_animation_index_ < 0) {
        icon_animation_index_ = 0;
        icon_animation_increasing_ = true;
      }
    }
    id = IDR_STATUSBAR_WIFI_1 + icon_animation_index_;
  } else {
    if (cros->wifi_ssid().empty()) {
      if (cros->ethernet_connected())
        id = IDR_STATUSBAR_WIRED;
      else
        id = IDR_STATUSBAR_DISCONNECTED;
    } else {
      // Gets the wifi image of 1-8 bars depending on signal strength. Signal
      // strength is from 0 to 100, so we need to convert that to 0 to 7.
      int index = floor(cros->wifi_strength() / (100.0 / kNumWifiImages));
      // This can happen if the signal strength is 100.
      if (index == kNumWifiImages)
        index--;
      id = IDR_STATUSBAR_WIFI_1 + index;
    }
  }
  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(id));
  SchedulePaint();
}
