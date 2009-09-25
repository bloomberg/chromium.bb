// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_menu_button.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
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
SkBitmap* NetworkMenuButton::wifi_images_[kNumWifiImages];
SkBitmap* NetworkMenuButton::wired_image_ = NULL;
SkBitmap* NetworkMenuButton::disconnected_image_ = NULL;

NetworkMenuButton::NetworkMenuButton(Browser* browser)
    : MenuButton(NULL, std::wstring(), this, false),
      refreshing_menu_(false),
      network_menu_(this),
      browser_(browser) {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    int image_index = IDR_STATUSBAR_WIFI_1;
    for (int i = 0; i < kNumWifiImages; i++)
      wifi_images_[i] = rb.GetBitmapNamed(image_index + i);
    wired_image_ = rb.GetBitmapNamed(IDR_STATUSBAR_WIRED);
    disconnected_image_ = rb.GetBitmapNamed(IDR_STATUSBAR_DISCONNECTED);
    initialized = true;
  }
  SetIcon(*disconnected_image_);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::Menu2Model implementation:

int NetworkMenuButton::GetItemCount() const {
  // The menu contains the available wifi networks. If there are none, then it
  // only has one item with a message that no networks are available.
  return wifi_networks_.empty() ? 1 : static_cast<int>(wifi_networks_.size());
}

views::Menu2Model::ItemType NetworkMenuButton::GetTypeAt(int index) const {
  return views::Menu2Model::TYPE_CHECK;
}

int NetworkMenuButton::GetCommandIdAt(int index) const {
  return index;
}

string16 NetworkMenuButton::GetLabelAt(int index) const {
  return wifi_networks_.empty() ?
         l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_NETWORKS_MESSAGE) :
         wifi_networks_[index].ssid;
}

bool NetworkMenuButton::IsItemCheckedAt(int index) const {
  return GetLabelAt(index) == current_ssid_;
}

bool NetworkMenuButton::IsEnabledAt(int index) const {
  return !wifi_networks_.empty();
}

void NetworkMenuButton::ActivatedAt(int index) {
  // When we are refreshing the menu, ignore menu item activation.
  if (refreshing_menu_)
    return;

  connecting_ssid_ = wifi_networks_[index].ssid;
  if (wifi_networks_[index].encryption.empty()) {
    ConnectToWifiNetwork(connecting_ssid_, string16());
  } else {
    // If network requires password, we open a password dialog window.
    gfx::NativeWindow parent = browser_->window()->GetNativeHandle();
    PasswordDialogView* dialog = new PasswordDialogView(this);
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

bool NetworkMenuButton::OnPasswordDialogCancel() {
  connecting_ssid_.clear();
  return true;
}

bool NetworkMenuButton::OnPasswordDialogAccept(const string16& password) {
  return ConnectToWifiNetwork(connecting_ssid_, password);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::ViewMenuDelegate implementation:

void NetworkMenuButton::RunMenu(views::View* source, const gfx::Point& pt,
                                gfx::NativeView hwnd) {
  RefreshWifiNetworks();
  refreshing_menu_ = true;
  network_menu_.Rebuild();
  network_menu_.UpdateStates();
  refreshing_menu_ = false;
  network_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

void NetworkMenuButton::AddWifiNetwork(const string16& ssid,
                                       const string16& encryption,
                                       int strength) {
  wifi_networks_.push_back(WifiNetwork(ssid, encryption, strength));
}

void NetworkMenuButton::RefreshWifiNetworks() {
  // TODO(chocobo): Refresh wifi model here.
  wifi_networks_.clear();
  AddWifiNetwork(ASCIIToUTF16("Wifi 12"), string16(), 12);
  AddWifiNetwork(ASCIIToUTF16("Wifi 28"), string16(), 28);
  AddWifiNetwork(ASCIIToUTF16("Wifi 70"), string16(), 70);
  AddWifiNetwork(ASCIIToUTF16("Wifi 99"), ASCIIToUTF16("WPA-PSK"), 99);

  // TODO(chocobo): Handle the case where current_ssid_ or connecting_ssid_ are
  // no longer found in the list of wifi networks.

  // Refresh the menu button image.
  if (current_ssid_.empty()) {
    SetIcon(*disconnected_image_);
  } else {
    int size = static_cast<int>(wifi_networks_.size());
    for (int i = 0; i < size; i++) {
      if (wifi_networks_[i].ssid == current_ssid_) {
        SetIcon(GetWifiImage(wifi_networks_[i]));
      }
    }
  }
  SchedulePaint();
}

bool NetworkMenuButton::ConnectToWifiNetwork(const string16& ssid,
                                             const string16& password) {
  // TODO(chocobo): Connect to wifi here.
  current_ssid_ = ssid;
  connecting_ssid_.clear();
  RefreshWifiNetworks();
  return true;
}

SkBitmap NetworkMenuButton::GetWifiImage(WifiNetwork wifi_network) const {
  // Returns the wifi image of 1-8 bars depending on signal strength.
  // Since signal strenght is from 0 to 100, we need to convert that to 0 to 7.
  int index = floor(wifi_network.strength / (100.0 / kNumWifiImages));
  // This can happen if the signal strength is 100.
  if (index == kNumWifiImages)
    index--;
  return *wifi_images_[index];
}
