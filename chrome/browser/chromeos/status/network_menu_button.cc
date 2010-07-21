// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_button.h"

#include <limits>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "gfx/canvas_skia.h"
#include "gfx/skbitmap_operations.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

// static
const int NetworkMenuButton::kNumWifiImages = 9;
const int NetworkMenuButton::kThrobDuration = 1000;

NetworkMenuButton::NetworkMenuButton(StatusAreaHost* host)
    : StatusAreaButton(this),
      host_(host),
      ALLOW_THIS_IN_INITIALIZER_LIST(network_menu_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_connecting_(this)) {
  animation_connecting_.SetThrobDuration(kThrobDuration);
  animation_connecting_.SetTweenType(Tween::LINEAR);
  NetworkChanged(CrosLibrary::Get()->GetNetworkLibrary());
  CrosLibrary::Get()->GetNetworkLibrary()->AddObserver(this);
}

NetworkMenuButton::~NetworkMenuButton() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveObserver(this);
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

const gfx::Font* NetworkMenuButton::GetLabelFontAt(int index) const {
  return (menu_items_[index].flags & FLAG_ASSOCIATED) ?
      &ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BoldFont) :
      NULL;
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

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  int flags = menu_items_[index].flags;
  if (flags & FLAG_OPTIONS) {
    host_->OpenButtonOptions(this);
  } else if (flags & FLAG_TOGGLE_ETHERNET) {
    cros->EnableEthernetNetworkDevice(!cros->ethernet_enabled());
  } else if (flags & FLAG_TOGGLE_WIFI) {
    cros->EnableWifiNetworkDevice(!cros->wifi_enabled());
  } else if (flags & FLAG_TOGGLE_CELLULAR) {
    cros->EnableCellularNetworkDevice(!cros->cellular_enabled());
  } else if (flags & FLAG_TOGGLE_OFFLINE) {
    cros->EnableOfflineMode(!cros->offline_mode());
  } else if (flags & FLAG_OTHER_NETWORK) {
    NetworkConfigView* view = new NetworkConfigView();
    view->set_browser_mode(host_->IsBrowserMode());
    views::Window* window = views::Window::CreateChromeWindow(
        host_->GetNativeWindow(), gfx::Rect(), view);
    window->SetIsAlwaysOnTop(true);
    window->Show();
    view->SetLoginTextfieldFocus();
  } else if (flags & FLAG_ETHERNET) {
    if (cros->ethernet_connected()) {
      NetworkConfigView* view = new NetworkConfigView(cros->ethernet_network());
      view->set_browser_mode(host_->IsBrowserMode());
      views::Window* window = views::Window::CreateChromeWindow(
            host_->GetNativeWindow(), gfx::Rect(), view);
      window->SetIsAlwaysOnTop(true);
      window->Show();
    }
  } else if (flags & FLAG_WIFI) {
    WifiNetwork wifi;
    bool wifi_exists = cros->FindWifiNetworkByPath(
        menu_items_[index].wireless_path, &wifi);
    if (!wifi_exists) {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      // TODO(stevenjb): Show notification.
    } else if (wifi.name() == cros->wifi_name()) {
      if (cros->wifi_connected()) {
        // If we are already connected, open the config dialog.
        NetworkConfigView* view = new NetworkConfigView(wifi, false);
        view->set_browser_mode(host_->IsBrowserMode());
        views::Window* window = views::Window::CreateChromeWindow(
            host_->GetNativeWindow(), gfx::Rect(), view);
        window->SetIsAlwaysOnTop(true);
        window->Show();
      } else {
        // TODO(stevenjb): Connection in progress. Show dialog?
      }
    } else {
      // If wifi network is not encrypted, then directly connect.
      // Otherwise, we open password dialog window.
      if (!wifi.encrypted()) {
        cros->ConnectToWifiNetwork(wifi, std::string(),
                                   std::string(), std::string());
      } else {
        NetworkConfigView* view = new NetworkConfigView(wifi, true);
        view->set_browser_mode(host_->IsBrowserMode());
        views::Window* window = views::Window::CreateChromeWindow(
            host_->GetNativeWindow(), gfx::Rect(), view);
        window->SetIsAlwaysOnTop(true);
        window->Show();
        view->SetLoginTextfieldFocus();
      }
    }
  } else if (flags & FLAG_CELLULAR) {
    CellularNetwork cellular;
    bool cellular_exists = cros->FindCellularNetworkByPath(
        menu_items_[index].wireless_path, &cellular);

    if (!cellular_exists) {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      // TODO(stevenjb): Show notification.
    } else if (cellular.name() == cros->cellular_name()) {
      // If clicked on a network that we are already connected to or we are
      // currently trying to connect to, then open config dialog.
      if (cros->cellular_connected()) {
        NetworkConfigView* view = new NetworkConfigView(cellular);
        view->set_browser_mode(host_->IsBrowserMode());
        views::Window* window = views::Window::CreateChromeWindow(
            host_->GetNativeWindow(), gfx::Rect(), view);
        window->SetIsAlwaysOnTop(true);
        window->Show();
      } else {
        // TODO(stevenjb): Connection in progress. Show dialog?
      }
    } else {
      cros->ConnectToCellularNetwork(cellular);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, AnimationDelegate implementation:

void NetworkMenuButton::AnimationProgressed(const Animation* animation) {
  if (animation == &animation_connecting_) {
    // Figure out which image to draw. We want a value between 0-100.
    // 0 reperesents no signal and 100 represents full signal strength.
    int value = static_cast<int>(animation_connecting_.GetCurrentValue()*100.0);
    if (value < 0)
      value = 0;
    else if (value > 100)
      value = 100;
    SetIcon(IconForNetworkStrength(value, false));
    SchedulePaint();
  } else {
    MenuButton::AnimationProgressed(animation);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, StatusAreaButton implementation:

void NetworkMenuButton::DrawPressed(gfx::Canvas* canvas) {
  // If ethernet connected and not current connecting, then show ethernet
  // pressed icon. Otherwise, show the bars pressed icon.
  if (CrosLibrary::Get()->GetNetworkLibrary()->ethernet_connected() &&
      !animation_connecting_.is_animating())
    canvas->DrawBitmapInt(IconForDisplay(
        *ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_STATUSBAR_NETWORK_WIRED_PRESSED), SkBitmap()),
        0, 0);
  else
    canvas->DrawBitmapInt(IconForDisplay(
        *ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS_PRESSED), SkBitmap()),
        0, 0);
}

void NetworkMenuButton::DrawIcon(gfx::Canvas* canvas) {
  canvas->DrawBitmapInt(IconForDisplay(icon(), badge()), 0, 0);
}

// Override the DrawIcon method to draw the wifi icon.
// The wifi icon is composed of 1 or more alpha-blended icons to show the
// network strength. We also draw an animation for when there's upload/download
// traffic.
/* TODO(chocobo): Add this code back in when UI is finalized.
void NetworkMenuButton::DrawIcon(gfx::Canvas* canvas) {

  // First draw the base icon.
  canvas->DrawBitmapInt(icon(), 0, 0);

  // If wifi, we draw the wifi signal bars.
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
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
    if (!animation_connecting_.is_animating()) {
      // For network animation, we only show animation in one direction.
      // So when we are hiding, we just use 1 minus the value.
      // We have kNumWifiImages + 1 number of states. For the first state, where
      // we are not adding any images, we set the index to -1.
      if (animation_downloading_.is_animating()) {
        double value_downloading = animation_downloading_.IsShowing() ?
            animation_downloading_.GetCurrentValue() :
            1.0 - animation_downloading_.GetCurrentValue();
        downloading_index = static_cast<int>(value_downloading *
              nextafter(static_cast<float>(kNumWifiImages + 1), 0)) - 1;
      }
      if (animation_uploading_.is_animating()) {
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
    // NOTE: Use an array rather than just calculating a resource number to
    // avoid creating implicit ordering dependencies on the resource values.
    static const int kWifiUpImages[kNumWifiImages] = {
      IDR_STATUSBAR_WIFI_UP1,
      IDR_STATUSBAR_WIFI_UP2,
      IDR_STATUSBAR_WIFI_UP3,
      IDR_STATUSBAR_WIFI_UP4,
      IDR_STATUSBAR_WIFI_UP5,
      IDR_STATUSBAR_WIFI_UP6,
      IDR_STATUSBAR_WIFI_UP7,
      IDR_STATUSBAR_WIFI_UP8,
      IDR_STATUSBAR_WIFI_UP9,
    };
    static const int kWifiUpPImages[kNumWifiImages] = {
      IDR_STATUSBAR_WIFI_UP1P,
      IDR_STATUSBAR_WIFI_UP2P,
      IDR_STATUSBAR_WIFI_UP3P,
      IDR_STATUSBAR_WIFI_UP4P,
      IDR_STATUSBAR_WIFI_UP5P,
      IDR_STATUSBAR_WIFI_UP6P,
      IDR_STATUSBAR_WIFI_UP7P,
      IDR_STATUSBAR_WIFI_UP8P,
      IDR_STATUSBAR_WIFI_UP9P,
    };
    static const int kWifiDownImages[kNumWifiImages] = {
      IDR_STATUSBAR_WIFI_DOWN1,
      IDR_STATUSBAR_WIFI_DOWN2,
      IDR_STATUSBAR_WIFI_DOWN3,
      IDR_STATUSBAR_WIFI_DOWN4,
      IDR_STATUSBAR_WIFI_DOWN5,
      IDR_STATUSBAR_WIFI_DOWN6,
      IDR_STATUSBAR_WIFI_DOWN7,
      IDR_STATUSBAR_WIFI_DOWN8,
      IDR_STATUSBAR_WIFI_DOWN9,
    };
    static const int kWifiDownPImages[kNumWifiImages] = {
      IDR_STATUSBAR_WIFI_DOWN1P,
      IDR_STATUSBAR_WIFI_DOWN2P,
      IDR_STATUSBAR_WIFI_DOWN3P,
      IDR_STATUSBAR_WIFI_DOWN4P,
      IDR_STATUSBAR_WIFI_DOWN5P,
      IDR_STATUSBAR_WIFI_DOWN6P,
      IDR_STATUSBAR_WIFI_DOWN7P,
      IDR_STATUSBAR_WIFI_DOWN8P,
      IDR_STATUSBAR_WIFI_DOWN9P,
    };

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
      canvas->DrawBitmapInt(*rb.GetBitmapNamed(kWifiUpImages[i]), 0, 0, paint);
      canvas->DrawBitmapInt(*rb.GetBitmapNamed(kWifiDownImages[i]), 0, 0,
                            paint);

      // Draw network traffic downloading/uploading image if necessary.
      if (i == downloading_index) {
        canvas->DrawBitmapInt(*rb.GetBitmapNamed(kWifiDownPImages[i]), 0, 0,
                              paint);
      }
      if (i == uploading_index) {
        canvas->DrawBitmapInt(*rb.GetBitmapNamed(kWifiUpPImages[i]), 0, 0,
                              paint);
      }
    }
  }
}
*/
////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::Observer implementation:

void NetworkMenuButton::NetworkChanged(NetworkLibrary* cros) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (CrosLibrary::Get()->EnsureLoaded()) {
    if (cros->wifi_connecting() || cros->cellular_connecting()) {
      // Start the connecting animation if not running.
      if (!animation_connecting_.is_animating()) {
        animation_connecting_.Reset();
        animation_connecting_.StartThrobbing(std::numeric_limits<int>::max());
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS1));
      }
    } else {
      // Stop connecting animation since we are not connecting.
      animation_connecting_.Stop();

      // Always show the higher priority connection first. Ethernet then wifi.
      if (cros->ethernet_connected())
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_WIRED));
      else if (cros->wifi_connected())
        SetIcon(IconForNetworkStrength(cros->wifi_strength(), false));
      else if (cros->cellular_connected())
        SetIcon(IconForNetworkStrength(cros->cellular_strength(), false));
      else
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0));
    }

    if (!cros->Connected() && !cros->Connecting()) {
      SetBadge(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED));
    } else if (!cros->ethernet_connected() && !cros->wifi_connected() &&
        (cros->cellular_connecting() || cros->cellular_connected())) {
      // TODO(chocobo): Check cellular network 3g/edge.
      SetBadge(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G));
//      SetBadge(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_EDGE));
    } else {
      SetBadge(SkBitmap());
    }
  } else {
    SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0));
    SetBadge(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_WARNING));
  }

  SchedulePaint();
}

void NetworkMenuButton::NetworkTraffic(NetworkLibrary* cros, int traffic_type) {
/* TODO(chocobo): Add this code back in when network traffic UI is finalized.
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
  */
}

void NetworkMenuButton::SetBadge(const SkBitmap& badge) {
  badge_ = badge;
}

// static
SkBitmap NetworkMenuButton::IconForNetworkStrength(int strength, bool black) {
  // Compose wifi icon by superimposing various icons.
  // NOTE: Use an array rather than just calculating a resource number to avoid
  // creating implicit ordering dependencies on the resource values.
  static const int kBarsImages[kNumWifiImages] = {
    IDR_STATUSBAR_NETWORK_BARS1,
    IDR_STATUSBAR_NETWORK_BARS2,
    IDR_STATUSBAR_NETWORK_BARS3,
    IDR_STATUSBAR_NETWORK_BARS4,
    IDR_STATUSBAR_NETWORK_BARS5,
    IDR_STATUSBAR_NETWORK_BARS6,
    IDR_STATUSBAR_NETWORK_BARS7,
    IDR_STATUSBAR_NETWORK_BARS8,
    IDR_STATUSBAR_NETWORK_BARS9,
  };
  static const int kBarsBlackImages[kNumWifiImages] = {
    IDR_STATUSBAR_NETWORK_BARS1_BLACK,
    IDR_STATUSBAR_NETWORK_BARS2_BLACK,
    IDR_STATUSBAR_NETWORK_BARS3_BLACK,
    IDR_STATUSBAR_NETWORK_BARS4_BLACK,
    IDR_STATUSBAR_NETWORK_BARS5_BLACK,
    IDR_STATUSBAR_NETWORK_BARS6_BLACK,
    IDR_STATUSBAR_NETWORK_BARS7_BLACK,
    IDR_STATUSBAR_NETWORK_BARS8_BLACK,
    IDR_STATUSBAR_NETWORK_BARS9_BLACK,
  };

  int index = static_cast<int>(strength / 100.0 *
      nextafter(static_cast<float>(kNumWifiImages), 0));
  index = std::max(std::min(index, kNumWifiImages - 1), 0);
  return *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      black ? kBarsBlackImages[index] : kBarsImages[index]);
}

// static
SkBitmap NetworkMenuButton::IconForDisplay(SkBitmap icon, SkBitmap badge) {
  // Icons are 24x24.
  static const int kIconWidth = 24;
  static const int kIconHeight = 24;
  // Draw the network icon 3 pixels down to center it.
  static const int kIconX = 0;
  static const int kIconY = 3;
  // Draw badge at (14,14).
  static const int kBadgeX = 14;
  static const int kBadgeY = 14;

  gfx::CanvasSkia canvas(kIconWidth, kIconHeight, false);
  canvas.DrawBitmapInt(icon, kIconX, kIconY);
  if (!badge.empty())
    canvas.DrawBitmapInt(badge, kBadgeX, kBadgeY);
  return canvas.ExtractBitmap();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::ViewMenuDelegate implementation:

void NetworkMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  refreshing_menu_ = true;
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->RequestWifiScan();
  cros->UpdateSystemInfo();
  InitMenuItems();
  network_menu_.Rebuild();
  network_menu_.UpdateStates();
  refreshing_menu_ = false;
  network_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

void NetworkMenuButton::InitMenuItems() {
  menu_items_.clear();
  // Populate our MenuItems with the current list of wifi networks.
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Ethernet
  string16 label = l10n_util::GetStringUTF16(
                       IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  SkBitmap icon = *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED_BLACK);
  SkBitmap badge = cros->ethernet_connecting() || cros->ethernet_connected() ?
      SkBitmap() : *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED);
  int flag = (cros->ethernet_connecting() || cros->ethernet_connected()) ?
      FLAG_ETHERNET | FLAG_ASSOCIATED : FLAG_ETHERNET;
  menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
      IconForDisplay(icon, badge), std::string(), flag));

  // Wifi
  const WifiNetworkVector& wifi_networks = cros->wifi_networks();
  // Wifi networks ssids.
  for (size_t i = 0; i < wifi_networks.size(); ++i) {
    label = ASCIIToUTF16(wifi_networks[i].name());
    SkBitmap icon = IconForNetworkStrength(wifi_networks[i].strength(), true);
    SkBitmap badge = wifi_networks[i].encrypted() ?
        *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE) : SkBitmap();
    flag = (wifi_networks[i].name() == cros->wifi_name()) ?
        FLAG_WIFI | FLAG_ASSOCIATED : FLAG_WIFI;
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        IconForDisplay(icon, badge), wifi_networks[i].service_path(), flag));
  }

  // Cellular
  const CellularNetworkVector& cell_networks = cros->cellular_networks();
  // Cellular networks ssids.
  for (size_t i = 0; i < cell_networks.size(); ++i) {
    label = ASCIIToUTF16(cell_networks[i].name());
    SkBitmap icon = IconForNetworkStrength(cell_networks[i].strength(), true);
    // TODO(chocobo): Check cellular network 3g/edge.
    SkBitmap badge = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G);
//    SkBitmap badge = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_EDGE);
    flag = (cell_networks[i].name() == cros->cellular_name()) ?
        FLAG_CELLULAR | FLAG_ASSOCIATED : FLAG_CELLULAR;
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        IconForDisplay(icon, badge), cell_networks[i].service_path(), flag));
  }

  // No networks available message.
  if (wifi_networks.empty() && cell_networks.empty()) {
    label = l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_MENU_ITEM_INDENT,
                l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_NETWORKS_MESSAGE));
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        SkBitmap(), std::string(), FLAG_DISABLED));
  }

  // Other networks
  menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND,
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_OTHER_NETWORKS),
      IconForDisplay(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0),
                     SkBitmap()),
      std::string(), FLAG_OTHER_NETWORK));

  if (cros->wifi_available() || cros->cellular_available()) {
    // Separator.
    menu_items_.push_back(MenuItem());

    // Turn Wifi Off. (only if wifi available)
    if (cros->wifi_available()) {
      int id = cros->wifi_enabled() ? IDS_STATUSBAR_NETWORK_DEVICE_DISABLE :
                                      IDS_STATUSBAR_NETWORK_DEVICE_ENABLE;
      label = l10n_util::GetStringFUTF16(id,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI));
      menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), FLAG_TOGGLE_WIFI));
    }

    // Turn Cellular Off. (only if cellular available)
    if (cros->cellular_available()) {
      int id = cros->cellular_enabled() ? IDS_STATUSBAR_NETWORK_DEVICE_DISABLE :
                                      IDS_STATUSBAR_NETWORK_DEVICE_ENABLE;
      label = l10n_util::GetStringFUTF16(id,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CELLULAR));
      menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), FLAG_TOGGLE_CELLULAR));
    }
  }

  // TODO(chocobo): Uncomment once we figure out how to do offline mode.
  // Offline mode.
//  menu_items_.push_back(MenuItem(cros->offline_mode() ?
//      menus::MenuModel::TYPE_CHECK : menus::MenuModel::TYPE_COMMAND,
//      l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_OFFLINE_MODE),
//      SkBitmap(), std::string(), FLAG_TOGGLE_OFFLINE));

  if (cros->Connected() || host_->ShouldOpenButtonOptions(this)) {
    // Separator.
    menu_items_.push_back(MenuItem());

    // IP address
    if (cros->Connected()) {
      menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND,
          ASCIIToUTF16(cros->IPAddress()), SkBitmap(),
          std::string(), FLAG_DISABLED));
    }

    // Network settings.
    if (host_->ShouldOpenButtonOptions(this)) {
      label =
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_OPEN_OPTIONS_DIALOG);
      menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), FLAG_OPTIONS));
    }
  }
}

}  // namespace chromeos
