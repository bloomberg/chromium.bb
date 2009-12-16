// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_menu_button.h"

#include <limits>

#include "app/gfx/canvas.h"
#include "app/gfx/skbitmap_operations.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

// static
const int NetworkMenuButton::kNumWifiImages = 3;
const int NetworkMenuButton::kMinOpacity = 50;
const int NetworkMenuButton::kMaxOpacity = 256;
const int NetworkMenuButton::kThrobDuration = 1000;
SkBitmap* NetworkMenuButton::menu_wifi_icons_ = NULL;
SkBitmap* NetworkMenuButton::menu_wired_icon_ = NULL;
SkBitmap* NetworkMenuButton::menu_disconnected_icon_ = NULL;

NetworkMenuButton::NetworkMenuButton(gfx::NativeWindow browser_window)
    : StatusAreaButton(this),
      ALLOW_THIS_IN_INITIALIZER_LIST(network_menu_(this)),
      browser_window_(browser_window),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_connecting_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_downloading_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_uploading_(this)) {
  // Initialize the static menu icons.
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    menu_wired_icon_ = new SkBitmap(SkBitmapOperations::CreateInvertedBitmap(
        *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED)));
    menu_disconnected_icon_ = new SkBitmap(
        SkBitmapOperations::CreateInvertedBitmap(
            *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED)));
    menu_wifi_icons_ = new SkBitmap[kNumWifiImages + 1];
    SkBitmap icon = *rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_DOT);
    menu_wifi_icons_[0] = SkBitmapOperations::CreateInvertedBitmap(icon);
    for (int i = 0; i < kNumWifiImages; i++) {
      icon = SkBitmapOperations::CreateSuperimposedBitmap(icon,
          *rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_UP1 + i));
      icon = SkBitmapOperations::CreateSuperimposedBitmap(icon,
          *rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_DOWN1 + i));
      menu_wifi_icons_[i + 1] = SkBitmapOperations::CreateInvertedBitmap(icon);
    }
    initialized = true;
  }
  animation_connecting_.SetThrobDuration(kThrobDuration);
  animation_connecting_.SetTweenType(SlideAnimation::NONE);
  animation_downloading_.SetThrobDuration(kThrobDuration);
  animation_downloading_.SetTweenType(SlideAnimation::NONE);
  animation_uploading_.SetThrobDuration(kThrobDuration);
  animation_uploading_.SetTweenType(SlideAnimation::NONE);
  NetworkChanged(NetworkLibrary::Get());
  NetworkLibrary::Get()->AddObserver(this);
}

NetworkMenuButton::~NetworkMenuButton() {
  NetworkLibrary::Get()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, menus::MenuModel implementation:

int NetworkMenuButton::GetItemCount() const {
  return static_cast<int>(menu_items_.size());
}

menus::MenuModel::ItemType NetworkMenuButton::GetTypeAt(int index) const {
  return menu_items_[index].type;
}

string16 NetworkMenuButton::GetLabelAt(int index) const {
  return menu_items_[index].label;
}

bool NetworkMenuButton::IsItemCheckedAt(int index) const {
  // All menus::MenuModel::TYPE_CHECK menu items are checked.
  return true;
}

bool NetworkMenuButton::GetIconAt(int index, SkBitmap* icon) const {
  if (!menu_items_[index].icon.empty()) {
    *icon = menu_items_[index].icon;
    return true;
  }
  return false;
}

bool NetworkMenuButton::IsEnabledAt(int index) const {
  return !(menu_items_[index].flags & FLAG_DISABLED);
}

void NetworkMenuButton::ActivatedAt(int index) {
  // When we are refreshing the menu, ignore menu item activation.
  if (refreshing_menu_)
    return;

  NetworkLibrary* cros = NetworkLibrary::Get();

  if (menu_items_[index].flags & FLAG_TOGGLE_ETHERNET) {
    cros->EnableEthernetNetworkDevice(!cros->ethernet_enabled());
  } else if (menu_items_[index].flags & FLAG_TOGGLE_WIFI) {
    cros->EnableWifiNetworkDevice(!cros->wifi_enabled());
  } else if (menu_items_[index].flags & FLAG_TOGGLE_OFFLINE) {
    cros->EnableOfflineMode(!cros->offline_mode());
  } else {
    activated_wifi_network_ = menu_items_[index].wifi_network;

    // If clicked on a network that we are already connected to or we are
    // currently trying to connect to, then do nothing.
    if (activated_wifi_network_.ssid == cros->wifi_ssid())
      return;

    // If wifi network is not encrypted, then directly connect.
    // Otherwise, we open password dialog window.
    if (!activated_wifi_network_.encrypted) {
      cros->ConnectToWifiNetwork(activated_wifi_network_, string16());
    } else {
      PasswordDialogView* dialog = new PasswordDialogView(this,
         activated_wifi_network_.ssid);
      views::Window* window = views::Window::CreateChromeWindow(browser_window_,
          gfx::Rect(), dialog);
      // Draw the password dialog right below this button and right aligned.
      gfx::Size size = dialog->GetPreferredSize();
      gfx::Rect rect = bounds();
      gfx::Point point = gfx::Point(rect.width() - size.width(), rect.height());
      ConvertPointToScreen(this, &point);
      window->SetBounds(gfx::Rect(point, size), browser_window_);
      window->Show();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, PasswordDialogDelegate implementation:

bool NetworkMenuButton::OnPasswordDialogAccept(const std::string& ssid,
                                               const string16& password) {
  NetworkLibrary::Get()->ConnectToWifiNetwork(activated_wifi_network_,
                                              password);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, AnimationDelegate implementation:

void NetworkMenuButton::AnimationProgressed(const Animation* animation) {
  if (animation == &animation_connecting_ ||
      animation == &animation_downloading_ ||
      animation == &animation_uploading_)
    SchedulePaint();
  else
    MenuButton::AnimationProgressed(animation);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, StatusAreaButton implementation:

// Override the DrawIcon method to draw the wifi icon.
// The wifi icon is composed of 1 or more alpha-blended icons to show the
// network strength. We also draw an animation for when there's upload/download
// traffic.
void NetworkMenuButton::DrawIcon(gfx::Canvas* canvas) {
  // First draw the base icon.
  canvas->DrawBitmapInt(icon(), 0, 0);

  // If wifi, we draw the wifi signal bars.
  NetworkLibrary* cros = NetworkLibrary::Get();
  if (cros->wifi_connecting() ||
      (!cros->ethernet_connected() && cros->wifi_connected())) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    // We want a value between 0-1.
    // 0 reperesents no signal and 1 represents full signal strength.
    double value = cros->wifi_connecting() ?
        animation_connecting_.GetCurrentValue() :
        cros->wifi_strength() / 100.0;
    if (value < 0)
      value = 0;
    else if (value > 1)
      value = 1;

    // If we are animating network traffic and not connecting, then we need to
    // figure out if we are to also draw the extra image.
    int downloading_index = -1;
    int uploading_index = -1;
    if (!animation_connecting_.IsAnimating()) {
      // For network animation, we only show animation in one direction.
      // So when we are hiding, we just use 1 minus the value.
      // We have kNumWifiImages + 1 number of states. For the first state, where
      // we are not adding any images, we set the index to -1.
      if (animation_downloading_.IsAnimating()) {
        double value_downloading = animation_downloading_.IsShowing() ?
            animation_downloading_.GetCurrentValue() :
            1.0 - animation_downloading_.GetCurrentValue();
        downloading_index = static_cast<int>(value_downloading *
              nextafter(static_cast<float>(kNumWifiImages + 1), 0)) - 1;
      }
      if (animation_uploading_.IsAnimating()) {
        double value_uploading = animation_uploading_.IsShowing() ?
            animation_uploading_.GetCurrentValue() :
            1.0 - animation_uploading_.GetCurrentValue();
        uploading_index = static_cast<int>(value_uploading *
              nextafter(static_cast<float>(kNumWifiImages + 1), 0)) - 1;
      }
    }

    // We need to determine opacity for each of the kNumWifiImages images.
    // We split the range (0-1) into equal ranges per kNumWifiImages images.
    // For example if kNumWifiImages is 3, then [0-0.33) is the first image and
    // [0.33-0.66) is the second image and [0.66-1] is the last image.
    // For each of the image:
    //   If value < the range of this image, draw at kMinOpacity opacity.
    //   If value > the range of this image, draw at kMaxOpacity-1 opacity.
    //   If value within the range of this image, draw at an opacity value
    //     between kMinOpacity and kMaxOpacity-1 relative to where in the range
    //     value is at.
    double value_per_image = 1.0 / kNumWifiImages;
    SkPaint paint;
    for (int i = 0; i < kNumWifiImages; i++) {
      if (value > value_per_image) {
        paint.setAlpha(kMaxOpacity - 1);
        value -= value_per_image;
      } else {
        // Map value between 0 and value_per_image to [kMinOpacity,kMaxOpacity).
        paint.setAlpha(kMinOpacity + static_cast<int>(value / value_per_image *
            nextafter(static_cast<float>(kMaxOpacity - kMinOpacity), 0)));
        // For following iterations, we want to draw at kMinOpacity.
        // So we set value to 0 here.
        value = 0;
      }
      canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_UP1 + i),
                            0, 0, paint);
      canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_DOWN1 + i),
                            0, 0, paint);

      // Draw network traffic downloading/uploading image if necessary.
      if (i == downloading_index)
        canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_DOWN1P + i),
                              0, 0, paint);
      if (i == uploading_index)
        canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_UP1P + i),
                              0, 0, paint);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::Observer implementation:

void NetworkMenuButton::NetworkChanged(NetworkLibrary* cros) {
  int id = IDR_STATUSBAR_WARNING;
  if (cros->EnsureLoaded()) {
    id = IDR_STATUSBAR_NETWORK_DISCONNECTED;
    if (cros->wifi_connecting()) {
      // Start the connecting animation if not running.
      if (!animation_connecting_.IsAnimating()) {
        animation_connecting_.Reset();
        animation_connecting_.StartThrobbing(std::numeric_limits<int>::max());
      }
      // Stop network traffic animation when we are connecting.
      animation_downloading_.Stop();
      animation_uploading_.Stop();

      id = IDR_STATUSBAR_WIFI_DOT;
    } else {
      // Stop connecting animation since we are not connecting.
      animation_connecting_.Stop();

      // Always show the higher priority connection first. Ethernet then wifi.
      if (cros->ethernet_connected()) {
        id = IDR_STATUSBAR_WIRED;
      } else if (cros->wifi_connected()) {
        id = IDR_STATUSBAR_WIFI_DOT;
      }
    }
  }

  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(id));
  SchedulePaint();
}

void NetworkMenuButton::NetworkTraffic(NetworkLibrary* cros, int traffic_type) {
  if (!cros->ethernet_connected() && cros->wifi_connected() &&
      !cros->wifi_connecting()) {
    // For downloading/uploading animation, we want to force at least one cycle
    // so that it looks smooth. And if we keep downloading/uploading, we will
    // keep calling StartThrobbing which will update the cycle count back to 2.
    if (traffic_type & TRAFFIC_DOWNLOAD)
      animation_downloading_.StartThrobbing(2);
    if (traffic_type & TRAFFIC_UPLOAD)
      animation_uploading_.StartThrobbing(2);
  }
}

// static
SkBitmap NetworkMenuButton::IconForWifiStrength(int strength) {
  // Compose wifi icon by superimposing various icons.
  int index = static_cast<int>(strength / 100.0 *
      nextafter(static_cast<float>(kNumWifiImages + 1), 0));
  if (index < 0)
    index = 0;
  if (index > kNumWifiImages)
    index = kNumWifiImages;
  return menu_wifi_icons_[index];
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::ViewMenuDelegate implementation:

void NetworkMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  refreshing_menu_ = true;
  InitMenuItems();
  network_menu_.Rebuild();
  network_menu_.UpdateStates();
  refreshing_menu_ = false;
  network_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

void NetworkMenuButton::InitMenuItems() {
  menu_items_.clear();
  // Populate our MenuItems with the current list of wifi networks.
  NetworkLibrary* cros = NetworkLibrary::Get();
  bool offline_mode = cros->offline_mode();

  // Wifi: Status.
  int status = IDS_STATUSBAR_NETWORK_DEVICE_DISABLED;
  if (cros->wifi_connecting())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
  else if (cros->wifi_connected())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
  else if (cros->wifi_enabled())
    status = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
  string16 label =
      l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI),
          l10n_util::GetStringUTF16(status));
  SkBitmap icon = cros->wifi_connected() ?
      IconForWifiStrength(cros->wifi_strength()) :
      *menu_disconnected_icon_;
  menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
      icon, WifiNetwork(), FLAG_DISABLED));

  // Turn Wifi Off.
  if (!offline_mode) {
    label = cros->wifi_enabled() ?
        l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_DISABLE,
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI)) :
        l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ENABLE,
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI));
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        SkBitmap(), WifiNetwork(), FLAG_TOGGLE_WIFI));

    const WifiNetworkVector& networks = cros->wifi_networks();
    // Wifi networks ssids.
    if (networks.empty()) {
      // No networks available message.
      label =
          l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_MENU_ITEM_INDENT,
              l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_NETWORKS_MESSAGE));
      menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), WifiNetwork(), FLAG_DISABLED));
    } else {
      for (size_t i = 0; i < networks.size(); ++i) {
        label =
            l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_MENU_ITEM_INDENT,
                ASCIIToUTF16(networks[i].ssid));
        if (networks[i].ssid == cros->wifi_ssid()) {
          menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_CHECK, label,
              SkBitmap(), networks[i], 0));
        } else {
          menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
              SkBitmap(), networks[i], 0));
          // TODO(chocobo): Once we have better icons and more reliable strength
          //   data, show icons for wifi ssids.
//        menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
//            IconForWifiStrength(networks[i].strength), networks[i], 0));
        }
      }
    }

    // Separator.
    menu_items_.push_back(MenuItem());
  }

  // Ethernet: Status.
  status = IDS_STATUSBAR_NETWORK_DEVICE_DISABLED;
  if (cros->ethernet_connecting())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
  else if (cros->ethernet_connected())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
  else if (cros->ethernet_enabled())
    status = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
  label = l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
      l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
      l10n_util::GetStringUTF16(status));
  icon = cros->ethernet_connected() ? *menu_wired_icon_ :
                                      *menu_disconnected_icon_;
  menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
      icon, WifiNetwork(), FLAG_DISABLED));

  // Turn Ethernet Off.
  if (!offline_mode) {
    label = cros->ethernet_enabled() ?
        l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_DISABLE,
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET)) :
        l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ENABLE,
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        SkBitmap(), WifiNetwork(), FLAG_TOGGLE_ETHERNET));

    // Separator.
    menu_items_.push_back(MenuItem());
  }

  // Offline mode.
  menu_items_.push_back(MenuItem(offline_mode ?
      menus::MenuModel::TYPE_CHECK : menus::MenuModel::TYPE_COMMAND,
      l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_OFFLINE_MODE),
      SkBitmap(), WifiNetwork(), FLAG_TOGGLE_OFFLINE));
}

}  // namespace chromeos
