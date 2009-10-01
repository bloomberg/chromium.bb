// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_menu_button.h"

#include <algorithm>

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
SkBitmap* NetworkMenuButton::wifi_images_[kNumWifiImages];
SkBitmap* NetworkMenuButton::wired_image_ = NULL;
SkBitmap* NetworkMenuButton::disconnected_image_ = NULL;
const int NetworkMenuButton::kAnimationDelayMillis = 100;

NetworkMenuButton::NetworkMenuButton(Browser* browser, bool cros_library_loaded)
    : MenuButton(NULL, std::wstring(), this, false),
      cros_library_loaded_(cros_library_loaded),
      refreshing_menu_(false),
      ethernet_connected_(false),
      network_menu_(this),
      browser_(browser),
      icon_animation_index_(0),
      icon_animation_increasing_(true) {
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
  RefreshNetworks();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::Menu2Model implementation:

int NetworkMenuButton::GetItemCount() const {
  // The menu contains the available wifi networks. If there are none, then it
  // only has one item with a message that no networks are available.
  return wifi_networks_in_menu_.empty() ? 1 :
      static_cast<int>(wifi_networks_in_menu_.size());
}

views::Menu2Model::ItemType NetworkMenuButton::GetTypeAt(int index) const {
  return wifi_networks_in_menu_.empty() ? views::Menu2Model::TYPE_COMMAND :
      views::Menu2Model::TYPE_CHECK;
}

string16 NetworkMenuButton::GetLabelAt(int index) const {
  return wifi_networks_in_menu_.empty() ?
      l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_NETWORKS_MESSAGE) :
      ASCIIToUTF16(wifi_networks_in_menu_[index].ssid);
}

bool NetworkMenuButton::IsItemCheckedAt(int index) const {
  // Network that we are connected to (or currently connecting to) is checked.
  return wifi_networks_in_menu_.empty() ? false :
      wifi_networks_in_menu_[index].ssid == current_ssid_ ||
      wifi_networks_in_menu_[index].ssid == connecting_ssid_;
}

bool NetworkMenuButton::IsEnabledAt(int index) const {
  return !wifi_networks_in_menu_.empty();
}

void NetworkMenuButton::ActivatedAt(int index) {
  // When we are refreshing the menu, ignore menu item activation.
  if (refreshing_menu_)
    return;

  // We need to look up the ssid in ssids_in_menu_.
  std::string ssid = wifi_networks_in_menu_[index].ssid;

  // If clicked on a network that we are already connected to or we are
  // currently trying to connect to, then do nothing.
  if (ssid == current_ssid_ || ssid == connecting_ssid_)
    return;

  // If wifi network is not encrypted, then directly connect.
  // Otherwise, we open password dialog window.
  if (!wifi_networks_in_menu_[index].encrypted) {
    ConnectToWifiNetwork(ssid, string16());
  } else {
    gfx::NativeWindow parent = browser_->window()->GetNativeHandle();
    PasswordDialogView* dialog = new PasswordDialogView(this, ssid);
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
  return ConnectToWifiNetwork(ssid, password);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::ViewMenuDelegate implementation:

void NetworkMenuButton::RunMenu(views::View* source, const gfx::Point& pt,
                                gfx::NativeView hwnd) {
  RefreshNetworks();
  // Make a copy of the wifi networks that we are showing because it may change.
  wifi_networks_in_menu_ = wifi_networks_;
  refreshing_menu_ = true;
  network_menu_.Rebuild();
  network_menu_.UpdateStates();
  refreshing_menu_ = false;
  network_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

bool NetworkMenuButton::GetWifiNetwork(const WifiNetworkVector& networks,
                                       const std::string& ssid,
                                       WifiNetwork* network) {
  WifiNetworkVector::const_iterator it;
  for (it = networks.begin(); it != networks.end(); ++it) {
    if (it->ssid == ssid) {
      *network = *it;
      return true;
    }
  }
  return false;
}

void NetworkMenuButton::AddWifiNetwork(const std::string& ssid,
                                       bool encrypted,
                                       chromeos::EncryptionType encryption,
                                       int strength) {
  wifi_networks_.push_back(
      WifiNetwork(ssid, encrypted, encryption, strength));
}

void NetworkMenuButton::RefreshNetworks() {
  std::string current;
  std::string connecting;
  wifi_networks_.clear();
  ethernet_connected_ = false;

  if (cros_library_loaded_) {
    chromeos::ServiceStatus* service_status = chromeos::GetAvailableNetworks();
    for (int i = 0; i < service_status->size; i++) {
      chromeos::ServiceInfo service = service_status->services[i];
      std::string ssid = service.ssid;
      DLOG(WARNING) << "Found network type=" << service.type <<
                       " ssid=" << service.ssid <<
                       " state=" << service.state <<
                       " needs_passphrase=" << service.needs_passphrase <<
                       " encryption=" << service.encryption <<
                       " signal_strength=" << service.signal_strength;
      if (service.type == chromeos::TYPE_WIFI) {
        AddWifiNetwork(ssid, service.needs_passphrase, service.encryption,
                       service.signal_strength);
        // Check connection state.
        switch (service.state) {
          case chromeos::STATE_ASSOCIATION:  // connecting to access point
          case chromeos::STATE_CONFIGURATION:  // optaining ip address
            connecting = ssid;
            break;
          case chromeos::STATE_READY:  // connected and has ip
            current = ssid;
            break;
          case chromeos::STATE_FAILURE:  // failed to connect
            // TODO(chocobo): Handle failure. Show it to user.
            DLOG(WARNING) << "Wifi network failed to connect: " << ssid;
            break;
          case chromeos::STATE_IDLE:  //  no connection
          case chromeos::STATE_DISCONNECT:  // disconnected
          case chromeos::STATE_CARRIER:  //  not used
          case chromeos::STATE_UNKNOWN:  //  unknown
          default:
            break;
        }
      } else if (service.type == chromeos::TYPE_ETHERNET) {
        if (service.state == chromeos::STATE_READY)
          ethernet_connected_ = true;
      }
    }
    chromeos::FreeServiceStatus(service_status);
  } else {
    // Use test data if ChromeOS shared library is not loaded.
    AddWifiNetwork("Wifi (12)", false, chromeos::NONE, 12);
    AddWifiNetwork("Wifi RSN (70)", true, chromeos::RSN, 70);
    AddWifiNetwork("Wifi (28)", false, chromeos::NONE, 28);
    AddWifiNetwork("Wifi WEP (99)", true, chromeos::WEP, 99);
    current = connecting_ssid_.empty() ? current_ssid_ : connecting_ssid_;
    ethernet_connected_ = true;
  }

  // Sort the list of wifi networks by ssid.
  std::sort(wifi_networks_.begin(), wifi_networks_.end());

  connecting_ssid_ = connecting;
  current_ssid_ = current;

  if (connecting_ssid_.empty()) {
    StopConnectingAnimation();
    UpdateIcon();
  } else {
    StartConnectingAnimation();
  }
}

static const char* GetEncryptionString(chromeos::EncryptionType encryption) {
  switch (encryption) {
    case chromeos::NONE:
      return "none";
    case chromeos::RSN:
      return "rsn";
    case chromeos::WEP:
      return "wep";
    case chromeos::WPA:
      return "wpa";
  }
  return "none";
}

bool NetworkMenuButton::ConnectToWifiNetwork(const std::string& ssid,
                                             const string16& password) {
  bool ok = true;
  if (cros_library_loaded_) {
    chromeos::EncryptionType encryption = chromeos::NONE;
    WifiNetwork network;
    if (GetWifiNetwork(wifi_networks_in_menu_, ssid, &network))
      encryption = network.encryption;
    ok = chromeos::ConnectToWifiNetwork(ssid.c_str(),
        password.empty() ? NULL : UTF16ToUTF8(password).c_str(),
        GetEncryptionString(encryption));
  }
  if (ok) {
    connecting_ssid_ = ssid;
    StartConnectingAnimation();
  }
  return true;
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
  if (!connecting_ssid_.empty()) {
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
    SetIcon(*wifi_images_[icon_animation_index_]);

    // Refresh wifi networks every full animation.
    // And see if we need to stop the animation.
    if (icon_animation_index_ == 0)
      RefreshNetworks();
  } else {
    if (current_ssid_.empty()) {
      if (ethernet_connected_)
        SetIcon(*wired_image_);
      else
        SetIcon(*disconnected_image_);
    } else {
      WifiNetwork network;
      if (GetWifiNetwork(wifi_networks_, current_ssid_, &network)) {
        // Gets the wifi image of 1-8 bars depending on signal strength. Signal
        // strength is from 0 to 100, so we need to convert that to 0 to 7.
        int index = floor(network.strength / (100.0 / kNumWifiImages));
        // This can happen if the signal strength is 100.
        if (index == kNumWifiImages)
          index--;
        SetIcon(*wifi_images_[index]);
      } else {
        // We no longer find the current network in the list of networks.
        // So just set the icon to the disconnected image.
        SetIcon(*disconnected_image_);
      }
    }
  }
  SchedulePaint();
}
