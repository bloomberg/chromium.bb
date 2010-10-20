// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dom_ui/network_menu_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/browser/views/window.h"
#include "gfx/canvas_skia.h"
#include "gfx/skbitmap_operations.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "views/controls/menu/menu_2.h"
#include "views/window/window.h"

namespace {
// Constants passed to Javascript:
static const char* kNetworkTypeEthernet = "ethernet";
static const char* kNetworkTypeWifi = "wifi";
static const char* kNetworkTypeCellular = "cellular";
static const char* kNetworkTypeOther = "other";

static const char* kNetworkStatusConnected = "connected";
static const char* kNetworkStatusConnecting = "connecting";
static const char* kNetworkStatusDisconnected = "disconnected";
static const char* kNetworkStatusError = "error";
}

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenu

// static
const int NetworkMenu::kNumWifiImages = 9;

// NOTE: Use an array rather than just calculating a resource number to avoid
// creating implicit ordering dependencies on the resource values.
// static
const int NetworkMenu::kBarsImages[kNumWifiImages] = {
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
// static
const int NetworkMenu::kBarsImagesBlack[kNumWifiImages] = {
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
// static
const int NetworkMenu::kBarsImagesLowData[kNumWifiImages] = {
  IDR_STATUSBAR_NETWORK_BARS1_LOWDATA,
  IDR_STATUSBAR_NETWORK_BARS2_LOWDATA,
  IDR_STATUSBAR_NETWORK_BARS3_LOWDATA,
  IDR_STATUSBAR_NETWORK_BARS4_LOWDATA,
  IDR_STATUSBAR_NETWORK_BARS5_LOWDATA,
  IDR_STATUSBAR_NETWORK_BARS6_LOWDATA,
  IDR_STATUSBAR_NETWORK_BARS7_LOWDATA,
  IDR_STATUSBAR_NETWORK_BARS8_LOWDATA,
  IDR_STATUSBAR_NETWORK_BARS9_LOWDATA,
};
// static
const int NetworkMenu::kBarsImagesVLowData[kNumWifiImages] = {
  IDR_STATUSBAR_NETWORK_BARS1_VLOWDATA,
  IDR_STATUSBAR_NETWORK_BARS2_VLOWDATA,
  IDR_STATUSBAR_NETWORK_BARS3_VLOWDATA,
  IDR_STATUSBAR_NETWORK_BARS4_VLOWDATA,
  IDR_STATUSBAR_NETWORK_BARS5_VLOWDATA,
  IDR_STATUSBAR_NETWORK_BARS6_VLOWDATA,
  IDR_STATUSBAR_NETWORK_BARS7_VLOWDATA,
  IDR_STATUSBAR_NETWORK_BARS8_VLOWDATA,
  IDR_STATUSBAR_NETWORK_BARS9_VLOWDATA,
};

NetworkMenu::NetworkMenu()
    : min_width_(-1) {
  network_menu_.reset(NetworkMenuUI::CreateMenu2(this));
}

NetworkMenu::~NetworkMenu() {
}

bool NetworkMenu::GetNetworkAt(int index, NetworkInfo* info) const {
  DCHECK(info);
  bool res = true;  // True unless a network doesn't exist.
  int flags = menu_items_[index].flags;
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (flags & FLAG_ETHERNET) {
    info->network_type = kNetworkTypeEthernet;
    if (cros->ethernet_connected()) {
      info->status = kNetworkStatusConnected;
      info->ip_address = cros->ethernet_network().ip_address();
    }
    info->need_passphrase = false;
    info->remembered = true;
  } else if (flags & FLAG_WIFI) {
    WifiNetwork wifi;
    bool found = cros->FindWifiNetworkByPath(
        menu_items_[index].wireless_path, &wifi);
    if (found) {
      info->network_type = kNetworkTypeWifi;
      if (wifi.service_path() == cros->wifi_network().service_path()) {
        if (cros->wifi_connected()) {
          info->status = kNetworkStatusConnected;
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED);
        } else if (cros->wifi_connecting()) {
          info->status = kNetworkStatusConnecting;
          // TODO(stevenjb): Eliminate status message, or localize properly.
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING)
              + ": " + wifi.GetStateString();
        } else if (wifi.state() == STATE_FAILURE) {
          info->status = kNetworkStatusError;
          info->message = wifi.GetErrorString();
        } else {
          info->status = kNetworkStatusDisconnected;
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED);
        }
      } else {
        info->status = kNetworkStatusDisconnected;
        info->message = l10n_util::GetStringUTF8(
            IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED);
      }
      if (wifi.encrypted()) {
        if (wifi.IsCertificateLoaded() ||
            wifi.encryption() == SECURITY_8021X) {
          info->need_passphrase = false;
        } else {
          info->need_passphrase = true;
        }
      } else {
        info->need_passphrase = false;
      }
      info->ip_address = wifi.ip_address();
      info->remembered = wifi.favorite();
    } else {
      res = false;  // Network not found, hide entry.
    }
  } else if (flags & FLAG_CELLULAR) {
    CellularNetwork cellular;
    bool found = cros->FindCellularNetworkByPath(
        menu_items_[index].wireless_path, &cellular);
    if (found) {
      info->network_type = kNetworkTypeCellular;
      if (cellular.service_path() ==
          cros->cellular_network().service_path()) {
        if (cros->cellular_connected()) {
          info->status = kNetworkStatusConnected;
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED);
        } else if (cros->cellular_connecting()) {
          // TODO(stevenjb): Eliminate status message, or localize properly.
          info->status = kNetworkStatusConnecting;
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING)
              + ": " + cellular.GetStateString();
        } else if (cellular.state() == STATE_FAILURE) {
          info->status = kNetworkStatusError;
          info->message = cellular.GetErrorString();
        } else {
          info->status = kNetworkStatusDisconnected;
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED);
        }
      } else {
        info->status = kNetworkStatusDisconnected;
        info->message = l10n_util::GetStringUTF8(
            IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED);
      }
      info->ip_address = cellular.ip_address();
      info->need_passphrase = false;
      info->remembered = true;
    } else {
      res = false;  // Network not found, hide entry.
    }
  } else if (flags & FLAG_OTHER_NETWORK) {
    info->status = kNetworkStatusDisconnected;
    info->message = l10n_util::GetStringUTF8(
        IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED);
    info->network_type = kNetworkTypeOther;
    info->need_passphrase = true;
    info->remembered = true;
  } else {
    // Not a network, e.g options, separator.
  }
  return res;
}

bool NetworkMenu::ConnectToNetworkAt(int index,
                                     const std::string& passphrase,
                                     const std::string& ssid,
                                     int remember) const {
  int flags = menu_items_[index].flags;
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (flags & FLAG_WIFI) {
    WifiNetwork wifi;
    bool found = cros->FindWifiNetworkByPath(
        menu_items_[index].wireless_path, &wifi);
    if (found) {
      // Connect or reconnect.
      if (remember >= 0)
        wifi.set_favorite(remember ? true : false);
      if (wifi.encrypted()) {
        if (wifi.IsCertificateLoaded()) {
          cros->ConnectToWifiNetwork(wifi, std::string(), std::string(),
                                     wifi.cert_path());
        } else if (wifi.encryption() == SECURITY_8021X) {
          // Show the wifi settings/dialog to load/select a certificate.
          ShowWifi(wifi, true);
        } else {
          cros->ConnectToWifiNetwork(wifi, passphrase, std::string(),
                                     std::string());
        }
      } else {
        cros->ConnectToWifiNetwork(wifi, std::string(), std::string(),
                                   std::string());
      }
    }
  } else if (flags & FLAG_CELLULAR) {
    CellularNetwork cellular;
    bool found = cros->FindCellularNetworkByPath(
        menu_items_[index].wireless_path, &cellular);
    if (found) {
      // Connect or reconnect.
      cros->ConnectToCellularNetwork(cellular);
    }
  } else if (flags & FLAG_OTHER_NETWORK) {
    bool favorite = remember == 0 ? false : true;  // default is true
    cros->ConnectToWifiNetwork(ssid, passphrase, std::string(), std::string(),
                               favorite);
  }
  return true;
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
    ShowOther();
  } else if (flags & FLAG_ETHERNET) {
    if (cros->ethernet_connected()) {
      ShowEthernet(cros->ethernet_network());
    }
  } else if (flags & FLAG_WIFI) {
    WifiNetwork wifi;
    bool wifi_exists = cros->FindWifiNetworkByPath(
        menu_items_[index].wireless_path, &wifi);
    if (!wifi_exists) {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      // TODO(stevenjb): Show notification.
    } else if (wifi.service_path() == cros->wifi_network().service_path()) {
      // Show the config settings for the active network.
      ShowWifi(wifi, false);
    } else {
      ConnectToNetworkAt(index, std::string(), std::string(), -1);
    }
  } else if (flags & FLAG_CELLULAR) {
    CellularNetwork cellular;
    bool cellular_exists = cros->FindCellularNetworkByPath(
        menu_items_[index].wireless_path, &cellular);
    if (!cellular_exists) {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      // TODO(stevenjb): Show notification.
    } else if (cellular.service_path() ==
               cros->cellular_network().service_path()) {
      // Show the config settings for the cellular network.
      ShowCellular(cellular, false);
    } else {
      ConnectToNetworkAt(index, std::string(), std::string(), -1);
    }
  }
}

void NetworkMenu::SetFirstLevelMenuWidth(int width) {
  min_width_ = width;
  // This actually has no effect since menu is rebuilt before showing.
  network_menu_->SetMinimumWidth(width);
}

void NetworkMenu::CancelMenu() {
  network_menu_->CancelMenu();
}

void NetworkMenu::UpdateMenu() {
  refreshing_menu_ = true;
  InitMenuItems();
  network_menu_->Rebuild();
  refreshing_menu_ = false;
}

// static
SkBitmap NetworkMenu::IconForNetworkStrength(int strength, bool black) {
  // Compose wifi icon by superimposing various icons.
  int index = static_cast<int>(strength / 100.0 *
      nextafter(static_cast<float>(kNumWifiImages), 0));
  index = std::max(std::min(index, kNumWifiImages - 1), 0);
  const int* images = black ? kBarsImagesBlack : kBarsImages;
  return *ResourceBundle::GetSharedInstance().GetBitmapNamed(images[index]);
}

SkBitmap NetworkMenu::IconForNetworkStrength(CellularNetwork cellular) {
  // Compose wifi icon by superimposing various icons.
  int index = static_cast<int>(cellular.strength() / 100.0 *
      nextafter(static_cast<float>(kNumWifiImages), 0));
  index = std::max(std::min(index, kNumWifiImages - 1), 0);
  const int* images = kBarsImages;
  switch (cellular.data_left()) {
    case CellularNetwork::DATA_NONE:
    case CellularNetwork::DATA_VERY_LOW:
      images = kBarsImagesVLowData;
      break;
    case CellularNetwork::DATA_LOW:
      images = kBarsImagesLowData;
      break;
    case CellularNetwork::DATA_NORMAL:
      images = kBarsImages;
      break;
  }
  return *ResourceBundle::GetSharedInstance().GetBitmapNamed(images[index]);
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
  network_menu_->Rebuild();
  // Restore menu width, if it was set up.
  // NOTE: width isn't checked for correctness here since all width-related
  // logic implemented inside |network_menu_|.
  if (min_width_ != -1)
    network_menu_->SetMinimumWidth(min_width_);
  refreshing_menu_ = false;
  network_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

void NetworkMenu::InitMenuItems() {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().

  menu_items_.clear();
  // Populate our MenuItems with the current list of wifi networks.
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Ethernet
  bool ethernet_connected = cros->ethernet_connected();
  bool ethernet_connecting = cros->ethernet_connecting();
  string16 label = l10n_util::GetStringUTF16(
                       IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  SkBitmap icon = *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED_BLACK);
  SkBitmap badge = ethernet_connecting || ethernet_connected ?
      SkBitmap() : *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED);
  int flag = FLAG_ETHERNET;
  if (ethernet_connecting || ethernet_connected)
    flag |= FLAG_ASSOCIATED;
  menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
      IconForDisplay(icon, badge), std::string(), flag));

  // Wifi
  const WifiNetworkVector& wifi_networks = cros->wifi_networks();
  const WifiNetwork& active_wifi = cros->wifi_network();
  // Wifi networks ssids.
  for (size_t i = 0; i < wifi_networks.size(); ++i) {
    label = ASCIIToUTF16(wifi_networks[i].name());
    SkBitmap icon = IconForNetworkStrength(wifi_networks[i].strength(), true);
    SkBitmap badge = wifi_networks[i].encrypted() ?
        *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE) : SkBitmap();
    flag = FLAG_WIFI;
    if (wifi_networks[i].service_path() == active_wifi.service_path())
      flag |= FLAG_ASSOCIATED;
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        IconForDisplay(icon, badge), wifi_networks[i].service_path(), flag));
  }

  // Cellular
  const CellularNetworkVector& cell_networks = cros->cellular_networks();
  const CellularNetwork& active_cellular = cros->cellular_network();
  // Cellular networks ssids.
  for (size_t i = 0; i < cell_networks.size(); ++i) {
    label = ASCIIToUTF16(cell_networks[i].name());
    SkBitmap icon = IconForNetworkStrength(cell_networks[i].strength(), true);
    // TODO(chocobo): Check cellular network 3g/edge.
    SkBitmap badge = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G);
//    SkBitmap badge = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_EDGE);
    flag = FLAG_CELLULAR;
    if (cell_networks[i].service_path() == active_cellular.service_path())
      flag |= FLAG_ASSOCIATED;
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

  bool wifi_available = cros->wifi_available();
  bool cellular_available = cros->cellular_available();
  if (wifi_available || cellular_available) {
    // Separator.
    menu_items_.push_back(MenuItem());

    // Turn Wifi Off. (only if wifi available)
    if (wifi_available) {
      int id = cros->wifi_enabled() ? IDS_STATUSBAR_NETWORK_DEVICE_DISABLE :
                                      IDS_STATUSBAR_NETWORK_DEVICE_ENABLE;
      label = l10n_util::GetStringFUTF16(id,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI));
      menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), FLAG_TOGGLE_WIFI));
    }

    // Turn Cellular Off. (only if cellular available)
    if (cellular_available) {
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

    // Network settings.
    if (ShouldOpenButtonOptions()) {
      label =
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_OPEN_OPTIONS_DIALOG);
      menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), FLAG_OPTIONS));
    }
  }
}

void NetworkMenu::ShowTabbedNetworkSettings(const Network& network) const {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;
  std::string page = StringPrintf("%s?servicePath=%s&networkType=%d",
      chrome::kInternetOptionsSubPage,
      EscapeUrlEncodedData(network.service_path()).c_str(),
      network.type());
  browser->ShowOptionsTab(page);
}

// TODO(stevenjb): deprecate this once we've committed to the embedded
// menu UI and fully deprecated NetworkConfigView.
void NetworkMenu::ShowNetworkConfigView(NetworkConfigView* view,
                                        bool focus_login) const {
  view->set_browser_mode(IsBrowserMode());
  views::Window* window = browser::CreateViewsWindow(
      GetNativeWindow(), gfx::Rect(), view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
  if (focus_login)
    view->SetLoginTextfieldFocus();
}

void NetworkMenu::ShowWifi(const WifiNetwork& wifi, bool focus_login) const{
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTabbedOptions)) {
    ShowTabbedNetworkSettings(wifi);
  } else {
    ShowNetworkConfigView(new NetworkConfigView(wifi, true), focus_login);
  }
}

void NetworkMenu::ShowCellular(const CellularNetwork& cellular,
                               bool focus_login) const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTabbedOptions)) {
    ShowTabbedNetworkSettings(cellular);
  } else {
    ShowNetworkConfigView(new NetworkConfigView(cellular), focus_login);
  }
}

void NetworkMenu::ShowEthernet(const EthernetNetwork& ethernet) const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTabbedOptions)) {
    ShowTabbedNetworkSettings(ethernet);
  } else {
    ShowNetworkConfigView(new NetworkConfigView(ethernet), false);
  }
}

void NetworkMenu::ShowOther() const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTabbedOptions)) {
    Browser* browser = BrowserList::GetLastActive();
    if (browser) {
      std::string page = StringPrintf("%s?networkType=%d",
                                      chrome::kInternetOptionsSubPage,
                                      chromeos::TYPE_WIFI);
      browser->ShowOptionsTab(page);
    }
  } else {
    const bool kFocusLogin = true;
    ShowNetworkConfigView(new NetworkConfigView(), kFocusLogin);
  }
}

}  // namespace chromeos
