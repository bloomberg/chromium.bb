// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dom_ui/network_menu_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/browser/ui/views/window.h"
#include "gfx/canvas_skia.h"
#include "gfx/skbitmap_operations.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
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
const int NetworkMenu::kNumBarsImages = 4;

// NOTE: Use an array rather than just calculating a resource number to avoid
// creating implicit ordering dependencies on the resource values.
// static
const int NetworkMenu::kBarsImages[kNumBarsImages] = {
  IDR_STATUSBAR_NETWORK_BARS1,
  IDR_STATUSBAR_NETWORK_BARS2,
  IDR_STATUSBAR_NETWORK_BARS3,
  IDR_STATUSBAR_NETWORK_BARS4,
};
// static
const int NetworkMenu::kBarsImagesBlack[kNumBarsImages] = {
  IDR_STATUSBAR_NETWORK_BARS1_BLACK,
  IDR_STATUSBAR_NETWORK_BARS2_BLACK,
  IDR_STATUSBAR_NETWORK_BARS3_BLACK,
  IDR_STATUSBAR_NETWORK_BARS4_BLACK,
};
/*
// static
const int NetworkMenu::kBarsImagesLowData[kNumBarsImages] = {
  IDR_STATUSBAR_NETWORK_BARS1_ORANGE,
  IDR_STATUSBAR_NETWORK_BARS2_ORANGE,
  IDR_STATUSBAR_NETWORK_BARS3_ORANGE,
  IDR_STATUSBAR_NETWORK_BARS4_ORANGE,
};
// static
const int NetworkMenu::kBarsImagesVLowData[kNumBarsImages] = {
  IDR_STATUSBAR_NETWORK_BARS1_RED,
  IDR_STATUSBAR_NETWORK_BARS2_RED,
  IDR_STATUSBAR_NETWORK_BARS3_RED,
  IDR_STATUSBAR_NETWORK_BARS4_RED,
};
*/

NetworkMenu::NetworkMenu()
    : min_width_(-1) {
  use_settings_ui_ = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions);
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
      info->ip_address = cros->ethernet_network() ?
          cros->ethernet_network()->ip_address() : std::string();
    }
    info->need_passphrase = false;
    info->remembered = true;
  } else if (flags & FLAG_WIFI) {
    WifiNetwork* wifi = cros->FindWifiNetworkByPath(
        menu_items_[index].wireless_path);
    if (wifi) {
      info->network_type = kNetworkTypeWifi;
      if (cros->wifi_network() &&
          wifi->service_path() == cros->wifi_network()->service_path()) {
        if (cros->wifi_connected()) {
          info->status = kNetworkStatusConnected;
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED);
        } else if (cros->wifi_connecting()) {
          info->status = kNetworkStatusConnecting;
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING);
        } else if (wifi->state() == STATE_FAILURE) {
          info->status = kNetworkStatusError;
          info->message = wifi->GetErrorString();
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
      info->need_passphrase = wifi->IsPassphraseRequired();
      info->ip_address = wifi->ip_address();
      info->remembered = wifi->favorite();
      info->auto_connect = wifi->auto_connect();
    } else {
      res = false;  // Network not found, hide entry.
    }
  } else if (flags & FLAG_CELLULAR) {
    CellularNetwork* cellular = cros->FindCellularNetworkByPath(
        menu_items_[index].wireless_path);
    if (cellular) {
      info->network_type = kNetworkTypeCellular;
      if (cros->cellular_network() && cellular->service_path() ==
          cros->cellular_network()->service_path()) {
        if (cros->cellular_connected()) {
          info->status = kNetworkStatusConnected;
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED);
        } else if (cros->cellular_connecting()) {
          // TODO(stevenjb): Eliminate status message, or localize properly.
          info->status = kNetworkStatusConnecting;
          info->message = l10n_util::GetStringUTF8(
              IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING)
              + ": " + cellular->GetStateString();
        } else if (cellular->state() == STATE_FAILURE) {
          info->status = kNetworkStatusError;
          info->message = cellular->GetErrorString();
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
      info->ip_address = cellular->ip_address();
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
                                     int auto_connect) const {
  int flags = menu_items_[index].flags;
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (flags & FLAG_WIFI) {
    WifiNetwork* wifi = cros->FindWifiNetworkByPath(
        menu_items_[index].wireless_path);
    if (wifi) {
      // Connect or reconnect.
      if (auto_connect >= 0)
        wifi->set_auto_connect(auto_connect ? true : false);
      if (wifi->connecting_or_connected()) {
        // Show the config settings for the active network.
        ShowTabbedNetworkSettings(wifi);
        return true;
      }
      bool connected = false;
      if (wifi->IsPassphraseRequired()) {
        // Show the connection UI if we require a passphrase.
        ShowNetworkConfigView(new NetworkConfigView(wifi));
        return true;
      } else {
        connected = cros->ConnectToWifiNetwork(
            wifi, passphrase, std::string(), std::string());
      }
      if (!connected) {
        if (!MenuUI::IsEnabled()) {
          // Show the wifi dialog on a failed attempt for non DOM UI menus.
          ShowNetworkConfigView(new NetworkConfigView(wifi));
          return true;
        } else {
          // If the connection attempt failed immediately (e.g. short password)
          // keep the menu open so that a retry can be attempted.
          return false;
        }
      }
    } else {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      // TODO(stevenjb): Show notification.
    }
  } else if (flags & FLAG_CELLULAR) {
    CellularNetwork* cellular = cros->FindCellularNetworkByPath(
        menu_items_[index].wireless_path);
    if (cellular) {
      if ((cellular->activation_state() != ACTIVATION_STATE_ACTIVATED &&
           cellular->activation_state() != ACTIVATION_STATE_UNKNOWN) ||
          cellular->needs_new_plan()) {
        ActivateCellular(cellular);
        return true;
      } else if (cellular->connecting_or_connected()) {
        // Cellular network is connecting or connected,
        // so we show the config settings for the cellular network.
        ShowTabbedNetworkSettings(cellular);
        return true;
      }
      // Clicked on a disconnected cellular network, so connect to it.
      cros->ConnectToCellularNetwork(cellular);
    } else {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      // TODO(stevenjb): Show notification.
    }
  } else if (flags & FLAG_OTHER_NETWORK) {
    bool connected = false;
    if (MenuUI::IsEnabled()) {
      // default is true
      bool auto_connect_bool = auto_connect == 0 ? false : true;
      connected = cros->ConnectToWifiNetwork(
          passphrase.empty() ? SECURITY_NONE : SECURITY_UNKNOWN,
          ssid, passphrase, std::string(), std::string(), auto_connect_bool);
    }
    if (!connected) {
      ShowOther();
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenu, ui::MenuModel implementation:

int NetworkMenu::GetItemCount() const {
  return static_cast<int>(menu_items_.size());
}

ui::MenuModel::ItemType NetworkMenu::GetTypeAt(int index) const {
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
  // All ui::MenuModel::TYPE_CHECK menu items are checked.
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
  } else if (flags & FLAG_ETHERNET) {
    if (cros->ethernet_connected()) {
      ShowTabbedNetworkSettings(cros->ethernet_network());
    }
  } else if (flags & FLAG_WIFI) {
    ConnectToNetworkAt(index, std::string(), std::string(), -1);
  } else if (flags & FLAG_OTHER_NETWORK) {
    ConnectToNetworkAt(index, std::string(), std::string(), -1);
  } else if (flags & FLAG_CELLULAR) {
    ConnectToNetworkAt(index, std::string(), std::string(), -1);
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
SkBitmap NetworkMenu::IconForNetworkStrength(const WifiNetwork* wifi,
                                             bool black) {
  DCHECK(wifi);
  if (wifi->strength() == 0) {
    return *ResourceBundle::GetSharedInstance().GetBitmapNamed(
        black ? IDR_STATUSBAR_NETWORK_BARS0_BLACK :
                IDR_STATUSBAR_NETWORK_BARS0);
  }
  int index = static_cast<int>(wifi->strength() / 100.0 *
      nextafter(static_cast<float>(kNumBarsImages), 0));
  index = std::max(std::min(index, kNumBarsImages - 1), 0);
  const int* images = black ? kBarsImagesBlack : kBarsImages;
  return *ResourceBundle::GetSharedInstance().GetBitmapNamed(images[index]);
}

// static
SkBitmap NetworkMenu::IconForNetworkStrength(const CellularNetwork* cellular,
                                             bool black) {
  DCHECK(cellular);
  // If no data, then we show 0 bars.
  if (cellular->strength() == 0 ||
      cellular->GetDataLeft() == CellularNetwork::DATA_NONE) {
    return *ResourceBundle::GetSharedInstance().GetBitmapNamed(
        black ? IDR_STATUSBAR_NETWORK_BARS0_BLACK :
                IDR_STATUSBAR_NETWORK_BARS0);
  }
  int index = static_cast<int>(cellular->strength() / 100.0 *
      nextafter(static_cast<float>(kNumBarsImages), 0));
  index = std::max(std::min(index, kNumBarsImages - 1), 0);
  const int* images = black ? kBarsImagesBlack : kBarsImages;
  return *ResourceBundle::GetSharedInstance().GetBitmapNamed(images[index]);
}

// static
SkBitmap NetworkMenu::IconForNetworkConnecting(double animation_value,
                                               bool black) {
  // Draw animation of bars icon fading in and out.
  // We are fading between 0 bars and a third of the opacity of 4 bars.
  // Use the current value of the animation to calculate the alpha value
  // of how transparent the icon is.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return SkBitmapOperations::CreateBlendedBitmap(
      *rb.GetBitmapNamed(black ? IDR_STATUSBAR_NETWORK_BARS0_BLACK :
                                 IDR_STATUSBAR_NETWORK_BARS0),
      *rb.GetBitmapNamed(black ? IDR_STATUSBAR_NETWORK_BARS4_BLACK :
                                 IDR_STATUSBAR_NETWORK_BARS4),
      animation_value / 3);
}

// static
// TODO(ers) update for GSM when we have the necessary images
SkBitmap NetworkMenu::BadgeForNetworkTechnology(
    const CellularNetwork* cellular) {
  if (!cellular)
    return SkBitmap();

  int id = -1;
  if (cellular->network_technology() == NETWORK_TECHNOLOGY_EVDO) {
    switch (cellular->GetDataLeft()) {
      case CellularNetwork::DATA_NONE:
        id = IDR_STATUSBAR_NETWORK_3G_ERROR;
        break;
      case CellularNetwork::DATA_VERY_LOW:
      case CellularNetwork::DATA_LOW:
      case CellularNetwork::DATA_NORMAL:
        id = IDR_STATUSBAR_NETWORK_3G;
        break;
      case CellularNetwork::DATA_UNKNOWN:
        id = IDR_STATUSBAR_NETWORK_3G_UNKNOWN;
        break;
    }
  } else if (cellular->network_technology() == NETWORK_TECHNOLOGY_1XRTT) {
    switch (cellular->GetDataLeft()) {
      case CellularNetwork::DATA_NONE:
        id = IDR_STATUSBAR_NETWORK_1X_ERROR;
        break;
      case CellularNetwork::DATA_VERY_LOW:
      case CellularNetwork::DATA_LOW:
      case CellularNetwork::DATA_NORMAL:
        id = IDR_STATUSBAR_NETWORK_1X;
        break;
      case CellularNetwork::DATA_UNKNOWN:
        id = IDR_STATUSBAR_NETWORK_1X_UNKNOWN;
        break;
    }
  }
  if (id == -1)
    return SkBitmap();
  else
    return *ResourceBundle::GetSharedInstance().GetBitmapNamed(id);
}

// static
SkBitmap NetworkMenu::IconForDisplay(SkBitmap icon, SkBitmap badge) {
  // Draw badge at (14,14).
  static const int kBadgeX = 14;
  static const int kBadgeY = 14;

  gfx::CanvasSkia canvas(icon.width(), icon.height(), false);
  canvas.DrawBitmapInt(icon, 0, 0);
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

  // Build initial menu items. They will be updated when UpdateMenu is
  // called from NetworkChanged.
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

  string16 label;

  if (cros->IsLocked()) {
    label = l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_LOCKED);
    menu_items_.push_back(
        MenuItem(ui::MenuModel::TYPE_COMMAND,
                 label, SkBitmap(),
                 std::string(), FLAG_DISABLED));
    return;
  }

  // Ethernet
  bool ethernet_available = cros->ethernet_available();
  bool ethernet_enabled = cros->ethernet_enabled();
  if (ethernet_available && ethernet_enabled) {
    bool ethernet_connected = cros->ethernet_connected();
    bool ethernet_connecting = cros->ethernet_connecting();

    if (ethernet_connecting) {
      label = l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING));
    } else {
      label = l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    }
    SkBitmap icon = *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED_BLACK);
    SkBitmap badge = ethernet_connecting || ethernet_connected ?
        SkBitmap() : *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED);
    int flag = FLAG_ETHERNET;
    if (ethernet_connecting || ethernet_connected)
      flag |= FLAG_ASSOCIATED;
    menu_items_.push_back(
        MenuItem(ui::MenuModel::TYPE_COMMAND, label,
                 IconForDisplay(icon, badge), std::string(), flag));
  }

  // Wifi Networks
  bool wifi_available = cros->wifi_available();
  bool wifi_enabled = cros->wifi_enabled();
  if (wifi_available && wifi_enabled) {
    const WifiNetworkVector& wifi_networks = cros->wifi_networks();
    const WifiNetwork* active_wifi = cros->wifi_network();

    bool separator_added = false;
    // List Wifi networks.
    for (size_t i = 0; i < wifi_networks.size(); ++i) {
      if (wifi_networks[i]->connecting()) {
        label = l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
            ASCIIToUTF16(wifi_networks[i]->name()),
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING));
      } else {
        label = ASCIIToUTF16(wifi_networks[i]->name());
      }

      // First add a separator if necessary.
      if (!separator_added) {
        separator_added = true;
        if (!menu_items_.empty()) {  // Don't add if first menu item.
          menu_items_.push_back(MenuItem());  // Separator
        }
      }

      SkBitmap icon = IconForNetworkStrength(wifi_networks[i], true);
      SkBitmap badge = wifi_networks[i]->encrypted() ?
          *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE) : SkBitmap();
      int flag = FLAG_WIFI;
      if (!wifi_networks[i]->connectable())
        flag |= FLAG_DISABLED;
      if (active_wifi
          && wifi_networks[i]->service_path() == active_wifi->service_path())
        flag |= FLAG_ASSOCIATED;
      menu_items_.push_back(
          MenuItem(ui::MenuModel::TYPE_COMMAND, label,
                   IconForDisplay(icon, badge),
                   wifi_networks[i]->service_path(), flag));
    }
  }

  // Cellular Networks
  bool cellular_available = cros->cellular_available();
  bool cellular_enabled = cros->cellular_enabled();
  if (cellular_available && cellular_enabled) {
    const CellularNetworkVector& cell_networks = cros->cellular_networks();
    const CellularNetwork* active_cellular = cros->cellular_network();

    bool separator_added = false;
    // List Cellular networks.
    for (size_t i = 0; i < cell_networks.size(); ++i) {
      chromeos::ActivationState activation_state =
          cell_networks[i]->activation_state();

      // If we are on the OOBE/login screen, do not show activating 3G option.
      if (!IsBrowserMode() && activation_state != ACTIVATION_STATE_ACTIVATED)
        continue;

      if (activation_state == ACTIVATION_STATE_NOT_ACTIVATED ||
          activation_state == ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
        label = l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_DEVICE_ACTIVATE,
            ASCIIToUTF16(cell_networks[i]->name()));
      } else if (activation_state == ACTIVATION_STATE_ACTIVATING) {
        label = l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
            ASCIIToUTF16(cell_networks[i]->name()),
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ACTIVATING));
      } else if (cell_networks[i]->connecting()) {
        label = l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
            ASCIIToUTF16(cell_networks[i]->name()),
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING));
      } else {
        label = ASCIIToUTF16(cell_networks[i]->name());
      }

      // First add a separator if necessary.
      if (!separator_added) {
        separator_added = true;
        if (!menu_items_.empty()) {  // Don't add if first menu item.
          menu_items_.push_back(MenuItem());  // Separator
        }
      }

      SkBitmap icon = IconForNetworkStrength(cell_networks[i], true);
      SkBitmap badge = BadgeForNetworkTechnology(cell_networks[i]);
      int flag = FLAG_CELLULAR;
      if (!cell_networks[i]->connectable())
        flag |= FLAG_DISABLED;
      bool isActive = active_cellular &&
          cell_networks[i]->service_path() == active_cellular->service_path() &&
          (cell_networks[i]->connecting() || cell_networks[i]->connected());
      if (isActive)
        flag |= FLAG_ASSOCIATED;
      menu_items_.push_back(
          MenuItem(ui::MenuModel::TYPE_COMMAND, label,
                   IconForDisplay(icon, badge),
                   cell_networks[i]->service_path(), flag));
      if (isActive) {
        label.clear();
        if (active_cellular->needs_new_plan()) {
          label = l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_NO_PLAN_LABEL);
        } else {
          const chromeos::CellularDataPlan* plan =
              active_cellular->GetSignificantDataPlan();
          if (plan)
            label = plan->GetUsageInfo();
        }
        if (label.length()) {
          menu_items_.push_back(
              MenuItem(ui::MenuModel::TYPE_COMMAND,
                       label, SkBitmap(),
                       std::string(), FLAG_DISABLED));
        }
      }
    }
  }

  // No networks available message.
  if (menu_items_.empty()) {
    label = l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_MENU_ITEM_INDENT,
                l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_NETWORKS_MESSAGE));
    menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
        SkBitmap(), std::string(), FLAG_DISABLED));
  }

  // Add network.
  if (wifi_available && wifi_enabled) {
    menu_items_.push_back(MenuItem());  // Separator
    menu_items_.push_back(MenuItem(
        ui::MenuModel::TYPE_COMMAND,
        l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_OTHER_NETWORKS),
        IconForDisplay(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0_BLACK),
                       SkBitmap()),
        std::string(), FLAG_OTHER_NETWORK));
  }

  // Enable / disable wireless.
  if (wifi_available || cellular_available) {
    menu_items_.push_back(MenuItem());  // Separator

    if (wifi_available) {
      // Add 'Scanning...'
      if (cros->wifi_scanning()) {
        label = l10n_util::GetStringUTF16(IDS_STATUSBAR_WIFI_SCANNING_MESSAGE);
        menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
            SkBitmap(), std::string(), FLAG_DISABLED));
      }

      int id = wifi_enabled ? IDS_STATUSBAR_NETWORK_DEVICE_DISABLE :
                              IDS_STATUSBAR_NETWORK_DEVICE_ENABLE;
      label = l10n_util::GetStringFUTF16(id,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI));
      menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), FLAG_TOGGLE_WIFI));
    }

    if (cellular_available) {
      int id = cellular_enabled ? IDS_STATUSBAR_NETWORK_DEVICE_DISABLE :
                                  IDS_STATUSBAR_NETWORK_DEVICE_ENABLE;
      label = l10n_util::GetStringFUTF16(id,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CELLULAR));
      menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), FLAG_TOGGLE_CELLULAR));
    }
  }

  // Offline mode.
  // TODO(chocobo): Uncomment once we figure out how to do offline mode.
  // menu_items_.push_back(MenuItem(cros->offline_mode() ?
  //     ui::MenuModel::TYPE_CHECK : ui::MenuModel::TYPE_COMMAND,
  //     l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_OFFLINE_MODE),
  //     SkBitmap(), std::string(), FLAG_TOGGLE_OFFLINE));

  bool connected = cros->Connected();  // always call for test expectations.
  bool oobe = !ShouldOpenButtonOptions();  // we don't show options for OOBE.
  // Network settings. (And IP Address)
  if (!oobe) {
    menu_items_.push_back(MenuItem());  // Separator

    if (!MenuUI::IsEnabled() && connected) {
      std::string ip_address = cros->IPAddress();
      if (!ip_address.empty()) {
        menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND,
            ASCIIToUTF16(cros->IPAddress()), SkBitmap(), std::string(),
                         FLAG_DISABLED));
      }
    }

    label = l10n_util::GetStringUTF16(IsBrowserMode() ?
        IDS_STATUSBAR_NETWORK_OPEN_OPTIONS_DIALOG :
        IDS_STATUSBAR_NETWORK_OPEN_PROXY_SETTINGS_DIALOG);
    menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
                                   SkBitmap(), std::string(), FLAG_OPTIONS));
  }
}

void NetworkMenu::ShowTabbedNetworkSettings(const Network* network) const {
  DCHECK(network);
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;
  std::string page = StringPrintf("%s?servicePath=%s&networkType=%d",
      chrome::kInternetOptionsSubPage,
      EscapeUrlEncodedData(network->service_path()).c_str(),
      network->type());
  browser->ShowOptionsTab(page);
}

// TODO(stevenjb): deprecate this once we've committed to tabbed settings
// and the embedded menu UI (and fully deprecated NetworkConfigView).
// Meanwhile, if MenuUI::IsEnabled() is true, always show the settings UI,
// otherwise show NetworkConfigView only to get passwords when not connected.
void NetworkMenu::ShowNetworkConfigView(NetworkConfigView* view) const {
  view->set_browser_mode(IsBrowserMode());
  views::Window* window = browser::CreateViewsWindow(
      GetNativeWindow(), gfx::Rect(), view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void NetworkMenu::ActivateCellular(const CellularNetwork* cellular) const {
  DCHECK(cellular);
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;
  browser->OpenMobilePlanTabAndActivate();
}

void NetworkMenu::ShowOther() const {
  if (use_settings_ui_ && MenuUI::IsEnabled()) {
    Browser* browser = BrowserList::GetLastActive();
    if (browser) {
      std::string page = StringPrintf("%s?networkType=%d",
                                      chrome::kInternetOptionsSubPage,
                                      chromeos::TYPE_WIFI);
      browser->ShowOptionsTab(page);
    }
  } else {
    ShowNetworkConfigView(new NetworkConfigView());
  }
}

}  // namespace chromeos
