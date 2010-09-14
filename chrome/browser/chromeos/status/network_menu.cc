// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "gfx/canvas_skia.h"
#include "gfx/skbitmap_operations.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenu

// static
const int NetworkMenu::kNumWifiImages = 9;

NetworkMenu::NetworkMenu()
    : ALLOW_THIS_IN_INITIALIZER_LIST(network_menu_(this)) {
}

NetworkMenu::~NetworkMenu() {
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenu, menus::MenuModel implementation:

int NetworkMenu::GetItemCount() const {
  return static_cast<int>(menu_items_.size());
}

menus::MenuModel::ItemType NetworkMenu::GetTypeAt(int index) const {
  return menu_items_[index].type;
}

string16 NetworkMenu::GetLabelAt(int index) const {
  return menu_items_[index].label;
}

const gfx::Font* NetworkMenu::GetLabelFontAt(int index) const {
  return (menu_items_[index].flags & FLAG_ASSOCIATED) ?
      &ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BoldFont) :
      NULL;
}

bool NetworkMenu::IsItemCheckedAt(int index) const {
  // All menus::MenuModel::TYPE_CHECK menu items are checked.
  return true;
}

bool NetworkMenu::GetIconAt(int index, SkBitmap* icon) const {
  if (!menu_items_[index].icon.empty()) {
    *icon = menu_items_[index].icon;
    return true;
  }
  return false;
}

bool NetworkMenu::IsEnabledAt(int index) const {
  return !(menu_items_[index].flags & FLAG_DISABLED);
}

void NetworkMenu::ActivatedAt(int index) {
  // When we are refreshing the menu, ignore menu item activation.
  if (refreshing_menu_)
    return;

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  int flags = menu_items_[index].flags;
  if (flags & FLAG_OPTIONS) {
    OpenButtonOptions();
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
    view->set_browser_mode(IsBrowserMode());
    views::Window* window = views::Window::CreateChromeWindow(GetNativeWindow(),
                                                              gfx::Rect(),
                                                              view);
    window->SetIsAlwaysOnTop(true);
    window->Show();
    view->SetLoginTextfieldFocus();
  } else if (flags & FLAG_ETHERNET) {
    if (cros->ethernet_connected()) {
      NetworkConfigView* view = new NetworkConfigView(cros->ethernet_network());
      view->set_browser_mode(IsBrowserMode());
      views::Window* window = views::Window::CreateChromeWindow(
          GetNativeWindow(), gfx::Rect(), view);
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
        view->set_browser_mode(IsBrowserMode());
        views::Window* window = views::Window::CreateChromeWindow(
            GetNativeWindow(), gfx::Rect(), view);
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
        view->set_browser_mode(IsBrowserMode());
        views::Window* window = views::Window::CreateChromeWindow(
            GetNativeWindow(), gfx::Rect(), view);
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
        view->set_browser_mode(IsBrowserMode());
        views::Window* window = views::Window::CreateChromeWindow(
            GetNativeWindow(), gfx::Rect(), view);
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

void NetworkMenu::SetFirstLevelMenuWidth(int width) {
  gtk_widget_set_size_request(network_menu_.GetNativeMenu(), width, -1);
}

void NetworkMenu::CancelMenu() {
  network_menu_.CancelMenu();
}

// static
SkBitmap NetworkMenu::IconForNetworkStrength(int strength, bool black) {
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
SkBitmap NetworkMenu::IconForDisplay(SkBitmap icon, SkBitmap badge) {
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
// NetworkMenu, views::ViewMenuDelegate implementation:

void NetworkMenu::RunMenu(views::View* source, const gfx::Point& pt) {
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

void NetworkMenu::InitMenuItems() {
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

  if (cros->Connected() || ShouldOpenButtonOptions()) {
    // Separator.
    menu_items_.push_back(MenuItem());

    // IP address
    if (cros->Connected()) {
      menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND,
          ASCIIToUTF16(cros->IPAddress()), SkBitmap(),
          std::string(), FLAG_DISABLED));
    }

    // Network settings.
    if (ShouldOpenButtonOptions()) {
      label =
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_OPEN_OPTIONS_DIALOG);
      menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), FLAG_OPTIONS));
    }
  }
}

}  // namespace chromeos
