// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_menu_button.h"

#include <limits>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

// static
const int NetworkMenuButton::kNumWifiImages = 8;
const int NetworkMenuButton::kThrobDuration = 1000;

NetworkMenuButton::NetworkMenuButton(gfx::NativeWindow browser_window)
    : MenuButton(NULL, std::wstring(), this, false),
      refreshing_menu_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(network_menu_(this)),
      browser_window_(browser_window),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)) {
  SetShowHighlighted(false);
  animation_.SetThrobDuration(kThrobDuration);
  animation_.SetTweenType(SlideAnimation::NONE);
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
    PasswordDialogView* dialog = new PasswordDialogView(this,
        wifi_networks_[index].ssid);
    views::Window* window = views::Window::CreateChromeWindow(
        browser_window_, gfx::Rect(), dialog);
    // Draw the password dialog right below this button and right aligned.
    gfx::Size size = dialog->GetPreferredSize();
    gfx::Rect rect = bounds();
    gfx::Point point = gfx::Point(rect.width() - size.width(), rect.height());
    ConvertPointToScreen(this, &point);
    window->SetBounds(gfx::Rect(point, size), browser_window_);
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
// NetworkMenuButton, AnimationDelegate implementation:

void NetworkMenuButton::AnimationProgressed(const Animation* animation) {
  if (animation == &animation_)
    UpdateIcon();
  else
    MenuButton::AnimationProgressed(animation);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::ViewMenuDelegate implementation:

void NetworkMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  wifi_networks_ = CrosNetworkLibrary::Get()->wifi_networks();
  refreshing_menu_ = true;
  network_menu_.Rebuild();
  network_menu_.UpdateStates();
  refreshing_menu_ = false;
  network_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, CrosNetworkLibrary::Observer implementation:

void NetworkMenuButton::NetworkChanged(CrosNetworkLibrary* obj) {
  UpdateIcon();
}

void NetworkMenuButton::UpdateIcon() {
  CrosNetworkLibrary* cros = CrosNetworkLibrary::Get();
  int id = IDR_STATUSBAR_DISCONNECTED;
  if (cros->wifi_connecting()) {
    // Start the connecting animation if not running.
    if (!animation_.IsAnimating())
      animation_.StartThrobbing(std::numeric_limits<int>::max());

    // We need to map the value of 0-1 in the animation to 0 - kNumWifiImages-1.
    int index = static_cast<int>(animation_.GetCurrentValue() *
                nextafter(static_cast<float>(kNumWifiImages), 0));
    id = IDR_STATUSBAR_WIFI_1 + index;
  } else {
    // Stop connecting animation since we are not connecting.
    if (animation_.IsAnimating())
      animation_.Stop();

    // Always show the higher priority connection first. So ethernet then wifi.
    if (cros->ethernet_connected()) {
      id = IDR_STATUSBAR_WIRED;
    } else if (!cros->wifi_ssid().empty()) {
      // Gets the wifi image of 1-8 bars depending on signal strength. Signal
      // strength is from 0 to 100, so we need to convert that to 0 to 7.
      int index = static_cast<int>(cros->wifi_strength() / 100.0 *
                  nextafter(static_cast<float>(kNumWifiImages), 0));
      // Make sure that index is between 0 and kNumWifiImages - 1
      if (index < 0)
        index = 0;
      if (index >= kNumWifiImages)
        index = kNumWifiImages - 1;
      id = IDR_STATUSBAR_WIFI_1 + index;
    }
  }
  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(id));
  SchedulePaint();
}
