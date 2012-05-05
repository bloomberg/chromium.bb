// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/choose_mobile_network_dialog.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

using content::BrowserThread;

namespace {

// Offsets for views menu ids (main menu and submenu ids use the same
// namespace).
const int kMainIndexMask = 0x1000;
const int kVPNIndexMask  = 0x2000;
const int kMoreIndexMask = 0x4000;

// Default minimum width in pixels of the menu to prevent unnecessary
// resizing as networks are updated.
const int kDefaultMinimumWidth = 280;

// Menu item margins.
const int kTopMargin = 5;
const int kBottomMargin = 7;

// Replace '&' in a string with "&&" to allow it to be a menu item label.
std::string EscapeAmpersands(const std::string& input) {
  std::string str = input;
  size_t found = str.find('&');
  while (found != std::string::npos) {
    str.replace(found, 1, "&&");
    found = str.find('&', found + 2);
  }
  return str;
}

// Set vertical menu margins for entire menu hierarchy.
void SetMenuMargins(views::MenuItemView* menu_item_view, int top, int bottom) {
  menu_item_view->SetMargins(top, bottom);
  if (menu_item_view->HasSubmenu()) {
    views::SubmenuView* submenu = menu_item_view->GetSubmenu();
    for (int i = 0; i < submenu->child_count(); ++i) {
      // Must skip separators.
      views::View* item = submenu->child_at(i);
      if (item->id() == views::MenuItemView::kMenuItemViewID) {
        views::MenuItemView* menu_item =
            static_cast<views::MenuItemView*>(item);
        SetMenuMargins(menu_item, top, bottom);
      }
    }
  }
}

// Activate a cellular network.
void ActivateCellular(const chromeos::CellularNetwork* cellular) {
  DCHECK(cellular);
  if (!chromeos::UserManager::Get()->IsSessionStarted())
    return;

  ash::Shell::GetInstance()->delegate()->OpenMobileSetup();
}

// Decides whether a network should be highlighted in the UI.
bool ShouldHighlightNetwork(const chromeos::Network* network) {
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  return cros->connected_network() ? network == cros->connected_network() :
                                     network == cros->connecting_network();
}

}  // namespace

namespace chromeos {

class NetworkMenuModel : public ui::MenuModel {
 public:
  struct MenuItem {
    MenuItem()
        : type(ui::MenuModel::TYPE_SEPARATOR),
          sub_menu_model(NULL),
          flags(0) {
    }
    MenuItem(ui::MenuModel::ItemType type, string16 label, SkBitmap icon,
             const std::string& service_path, int flags)
        : type(type),
          label(label),
          icon(icon),
          service_path(service_path),
          sub_menu_model(NULL),
          flags(flags) {
    }
    MenuItem(ui::MenuModel::ItemType type, string16 label, SkBitmap icon,
             NetworkMenuModel* sub_menu_model, int flags)
        : type(type),
          label(label),
          icon(icon),
          sub_menu_model(sub_menu_model),
          flags(flags) {
    }

    ui::MenuModel::ItemType type;
    string16 label;
    SkBitmap icon;
    std::string service_path;
    NetworkMenuModel* sub_menu_model;  // Weak ptr.
    int flags;
  };
  typedef std::vector<MenuItem> MenuItemVector;

  explicit NetworkMenuModel(const base::WeakPtr<NetworkMenu>& owner)
    : owner_(owner) {}
  virtual ~NetworkMenuModel() {}

  // Connect or reconnect to the network at |index|.
  // If remember >= 0, set the favorite state of the network.
  void ConnectToNetworkAt(int index,
                          const std::string& passphrase,
                          const std::string& ssid,
                          int remember) const;

  // Called by NetworkMenu::RunMenu to initialize list of menu items.
  virtual void InitMenuItems(bool should_open_button_options) = 0;

  // Menu item field accessors.
  const MenuItemVector& menu_items() const { return menu_items_; }

  // ui::MenuModel implementation
  // GetCommandIdAt() must be implemented by subclasses.
  virtual bool HasIcons() const OVERRIDE;
  virtual int GetItemCount() const OVERRIDE;
  virtual ui::MenuModel::ItemType GetTypeAt(int index) const OVERRIDE;
  virtual string16 GetLabelAt(int index) const OVERRIDE;
  virtual bool IsItemDynamicAt(int index) const OVERRIDE;
  virtual const gfx::Font* GetLabelFontAt(int index) const OVERRIDE;
  virtual bool GetAcceleratorAt(int index,
                                ui::Accelerator* accelerator) const OVERRIDE;
  virtual bool IsItemCheckedAt(int index) const OVERRIDE;
  virtual int GetGroupIdAt(int index) const OVERRIDE;
  virtual bool GetIconAt(int index, SkBitmap* icon) OVERRIDE;
  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(
      int index) const OVERRIDE;
  virtual bool IsEnabledAt(int index) const OVERRIDE;
  virtual bool IsVisibleAt(int index) const OVERRIDE;
  virtual ui::MenuModel* GetSubmenuModelAt(int index) const OVERRIDE;
  virtual void HighlightChangedTo(int index) OVERRIDE;
  virtual void ActivatedAt(int index) OVERRIDE;
  virtual void SetMenuModelDelegate(ui::MenuModelDelegate* delegate) OVERRIDE;

 protected:
  enum MenuItemFlags {
    FLAG_NONE              = 0,
    FLAG_DISABLED          = 1 << 0,
    FLAG_TOGGLE_ETHERNET   = 1 << 1,
    FLAG_TOGGLE_WIFI       = 1 << 2,
    FLAG_TOGGLE_CELLULAR   = 1 << 3,
    FLAG_TOGGLE_OFFLINE    = 1 << 4,
    FLAG_ASSOCIATED        = 1 << 5,
    FLAG_ETHERNET          = 1 << 6,
    FLAG_WIFI              = 1 << 7,
    FLAG_CELLULAR          = 1 << 8,
    FLAG_OPTIONS           = 1 << 9,
    FLAG_ADD_WIFI          = 1 << 10,
    FLAG_ADD_CELLULAR      = 1 << 11,
    FLAG_VPN               = 1 << 12,
    FLAG_ADD_VPN           = 1 << 13,
    FLAG_DISCONNECT_VPN    = 1 << 14,
    FLAG_VIEW_ACCOUNT      = 1 << 15,
  };

  // Our menu items.
  MenuItemVector menu_items_;

  // Weak pointer to NetworkMenu that owns this MenuModel.
  base::WeakPtr<NetworkMenu> owner_;

  // Top up URL of the current carrier on empty string if there's none.
  std::string top_up_url_;

  // Carrier ID which top up URL is initialized for.
  // Used to update top up URL only when cellular carrier has changed.
  std::string carrier_id_;

 private:
  // Open a dialog to set up and connect to a network.
  void ShowOther(ConnectionType type) const;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuModel);
};

class MoreMenuModel : public NetworkMenuModel {
 public:
  explicit MoreMenuModel(const base::WeakPtr<NetworkMenu> owner)
    : NetworkMenuModel(owner) {}
  virtual ~MoreMenuModel() {}

  // NetworkMenuModel implementation.
  virtual void InitMenuItems(bool should_open_button_options) OVERRIDE;

  // ui::MenuModel implementation
  virtual int GetCommandIdAt(int index) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MoreMenuModel);
};

class VPNMenuModel : public NetworkMenuModel {
 public:
  explicit VPNMenuModel(const base::WeakPtr<NetworkMenu>& owner)
    : NetworkMenuModel(owner) {}
  virtual ~VPNMenuModel() {}

  // NetworkMenuModel implementation.
  virtual void InitMenuItems(bool should_open_button_options) OVERRIDE;

  // ui::MenuModel implementation
  virtual int GetCommandIdAt(int index) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(VPNMenuModel);
};

class MainMenuModel : public NetworkMenuModel {
 public:
  explicit MainMenuModel(const base::WeakPtr<NetworkMenu>& owner)
      : NetworkMenuModel(owner),
        vpn_menu_model_(new VPNMenuModel(owner)),
        more_menu_model_(new MoreMenuModel(owner)) {
  }
  virtual ~MainMenuModel() {}

  // NetworkMenuModel implementation.
  virtual void InitMenuItems(bool should_open_button_options) OVERRIDE;

  // ui::MenuModel implementation
  virtual int GetCommandIdAt(int index) const OVERRIDE;

 private:
  scoped_ptr<NetworkMenuModel> vpn_menu_model_;
  scoped_ptr<MoreMenuModel> more_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(MainMenuModel);
};

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuModel, public methods:

void NetworkMenuModel::ConnectToNetworkAt(int index,
                                          const std::string& passphrase,
                                          const std::string& ssid,
                                          int auto_connect) const {
  int flags = menu_items_[index].flags;
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const std::string& service_path = menu_items_[index].service_path;
  if (flags & FLAG_WIFI) {
    WifiNetwork* wifi = cros->FindWifiNetworkByPath(service_path);
    if (wifi) {
      // Connect or reconnect.
      if (auto_connect >= 0)
        wifi->SetAutoConnect(auto_connect ? true : false);
      owner_->ConnectToNetwork(wifi);
    } else {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      LOG(WARNING) << "Wi-fi network does not exist to connect to: "
                   << service_path;
      // TODO(stevenjb): Show notification.
    }
  } else if (flags & FLAG_CELLULAR) {
    CellularNetwork* cellular = cros->FindCellularNetworkByPath(
        service_path);
    if (cellular) {
      owner_->ConnectToNetwork(cellular);
    } else {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      LOG(WARNING) << "Cellular network does not exist to connect to: "
                   << service_path;
      // TODO(stevenjb): Show notification.
    }
  } else if (flags & FLAG_ADD_WIFI) {
    ShowOther(TYPE_WIFI);
  } else if (flags & FLAG_ADD_CELLULAR) {
    ShowOther(TYPE_CELLULAR);
  } else if (flags & FLAG_ADD_VPN) {
    ShowOther(TYPE_VPN);
  } else if (flags & FLAG_VPN) {
    VirtualNetwork* vpn = cros->FindVirtualNetworkByPath(service_path);
    if (vpn) {
      owner_->ConnectToNetwork(vpn);
    } else {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      LOG(WARNING) << "VPN does not exist to connect to: " << service_path;
      // TODO(stevenjb): Show notification.
    }
  }
}

void NetworkMenu::ConnectToNetwork(Network* network) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  switch (network->type()) {
    case TYPE_ETHERNET: {
      ShowTabbedNetworkSettings(network);
      break;
    }
    case TYPE_WIFI: {
      WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
      if (wifi->connecting_or_connected()) {
        ShowTabbedNetworkSettings(wifi);
      } else {
        wifi->SetEnrollmentDelegate(
            CreateEnrollmentDelegate(delegate()->GetNativeWindow(),
                                     wifi->name(),
                                     ProfileManager::GetLastUsedProfile()));
        wifi->AttemptConnection(base::Bind(&NetworkMenu::DoConnect,
                                           weak_pointer_factory_.GetWeakPtr(),
                                           wifi));
      }
      break;
    }

    case TYPE_CELLULAR: {
      CellularNetwork* cell = static_cast<CellularNetwork*>(network);
      if (cell->NeedsActivation()) {
        ActivateCellular(cell);
      } else if (cell->connecting_or_connected()) {
        // Cellular network is connecting or connected,
        // so we show the config settings for the cellular network.
        ShowTabbedNetworkSettings(cell);
      } else {
        // Clicked on a disconnected cellular network, so connect to it.
        cros->ConnectToCellularNetwork(cell);
      }
      break;
    }

    case TYPE_VPN: {
      VirtualNetwork* vpn = static_cast<VirtualNetwork*>(network);
      // Connect or reconnect.
      if (vpn->connecting_or_connected()) {
        ShowTabbedNetworkSettings(vpn);
      } else {
        vpn->SetEnrollmentDelegate(
            CreateEnrollmentDelegate(delegate()->GetNativeWindow(),
                                     vpn->name(),
                                     ProfileManager::GetLastUsedProfile()));
        vpn->AttemptConnection(base::Bind(&NetworkMenu::DoConnect,
                                          weak_pointer_factory_.GetWeakPtr(),
                                          vpn));
      }
      break;
    }

    default:
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuModel, ui::MenuModel implementation:

bool NetworkMenuModel::HasIcons() const {
  return true;
}

int NetworkMenuModel::GetItemCount() const {
  return static_cast<int>(menu_items_.size());
}

ui::MenuModel::ItemType NetworkMenuModel::GetTypeAt(int index) const {
  return menu_items_[index].type;
}

string16 NetworkMenuModel::GetLabelAt(int index) const {
  return menu_items_[index].label;
}

bool NetworkMenuModel::IsItemDynamicAt(int index) const {
  return false;
}

const gfx::Font* NetworkMenuModel::GetLabelFontAt(int index) const {
  const gfx::Font* font = NULL;
  if (menu_items_[index].flags & FLAG_ASSOCIATED) {
    ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
    font = &resource_bundle.GetFont(
        browser_defaults::kAssociatedNetworkFontStyle);
  }

  return font;
}

bool NetworkMenuModel::GetAcceleratorAt(int index,
                                        ui::Accelerator* accelerator) const {
  return false;
}

bool NetworkMenuModel::IsItemCheckedAt(int index) const {
  // All ui::MenuModel::TYPE_CHECK menu items are checked.
  return true;
}

int NetworkMenuModel::GetGroupIdAt(int index) const {
  return 0;
}

bool NetworkMenuModel::GetIconAt(int index, SkBitmap* icon) {
  if (!menu_items_[index].icon.empty()) {
    *icon = menu_items_[index].icon;
    return true;
  }
  return false;
}

ui::ButtonMenuItemModel* NetworkMenuModel::GetButtonMenuItemAt(
    int index) const {
  return NULL;
}

bool NetworkMenuModel::IsEnabledAt(int index) const {
  return !(menu_items_[index].flags & FLAG_DISABLED);
}

bool NetworkMenuModel::IsVisibleAt(int index) const {
  return true;
}

ui::MenuModel* NetworkMenuModel::GetSubmenuModelAt(int index) const {
  return menu_items_[index].sub_menu_model;
}

void NetworkMenuModel::HighlightChangedTo(int index) {
}

void NetworkMenuModel::ActivatedAt(int index) {
  // When we are refreshing the menu, ignore menu item activation.
  if (owner_->refreshing_menu_)
    return;

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  int flags = menu_items_[index].flags;
  if (flags & FLAG_OPTIONS) {
    owner_->delegate()->OpenButtonOptions();
  } else if (flags & FLAG_TOGGLE_ETHERNET) {
    cros->EnableEthernetNetworkDevice(!cros->ethernet_enabled());
  } else if (flags & FLAG_TOGGLE_WIFI) {
    owner_->ToggleWifi();
  } else if (flags & FLAG_TOGGLE_CELLULAR) {
    owner_->ToggleCellular();
  } else if (flags & FLAG_TOGGLE_OFFLINE) {
    cros->EnableOfflineMode(!cros->offline_mode());
  } else if (flags & FLAG_ETHERNET) {
    if (cros->ethernet_connected())
      owner_->ShowTabbedNetworkSettings(cros->ethernet_network());
  } else if (flags & (FLAG_WIFI | FLAG_ADD_WIFI |
                      FLAG_CELLULAR | FLAG_ADD_CELLULAR |
                      FLAG_VPN | FLAG_ADD_VPN)) {
    ConnectToNetworkAt(index, std::string(), std::string(), -1);
  } else if (flags & FLAG_DISCONNECT_VPN) {
    const VirtualNetwork* active_vpn = cros->virtual_network();
    if (active_vpn)
      cros->DisconnectFromNetwork(active_vpn);
  } else if (flags & FLAG_VIEW_ACCOUNT) {
    Browser* browser = Browser::GetOrCreateTabbedBrowser(
        ProfileManager::GetDefaultProfileOrOffTheRecord());
    browser->ShowSingletonTab(GURL(top_up_url_));
  }
}

void NetworkMenuModel::SetMenuModelDelegate(ui::MenuModelDelegate* delegate) {
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuModel, private methods:

void NetworkMenuModel::ShowOther(ConnectionType type) const {
  if (type == TYPE_CELLULAR)
    owner_->ShowOtherCellular();
  else
    owner_->ShowOtherWifi();
}

////////////////////////////////////////////////////////////////////////////////
// MainMenuModel

void MainMenuModel::InitMenuItems(bool should_open_button_options) {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().

  menu_items_.clear();

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (cros->IsLocked()) {
    menu_items_.push_back(
        MenuItem(ui::MenuModel::TYPE_COMMAND,
                 l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_LOCKED),
                 SkBitmap(), std::string(), FLAG_DISABLED));
    return;
  }

  // Populate our MenuItems with the current list of networks.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  string16 label;

  // Ethernet
  // Only display an ethernet icon if enabled, and an ethernet network exists.
  bool ethernet_enabled = cros->ethernet_enabled();
  const chromeos::EthernetNetwork* ethernet_network = cros->ethernet_network();
  if (ethernet_enabled && ethernet_network) {
    bool ethernet_connecting = cros->ethernet_connecting();

    if (ethernet_connecting) {
      label = l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING));
    } else {
      label = l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    }
    int flag = FLAG_ETHERNET;
    if (ShouldHighlightNetwork(ethernet_network))
      flag |= FLAG_ASSOCIATED;
    SkBitmap icon;
    icon = NetworkMenuIcon::GetBitmap(ethernet_network,
                                      NetworkMenuIcon::COLOR_DARK);
    menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND,
                                   label, icon, std::string(), flag));
  }

  // Wifi Networks
  bool wifi_available = cros->wifi_available();
  bool wifi_enabled = cros->wifi_enabled();
  if (wifi_available && wifi_enabled) {
    const WifiNetworkVector& wifi_networks = cros->wifi_networks();

    bool separator_added = false;
    // List Wifi networks.
    for (size_t i = 0; i < wifi_networks.size(); ++i) {
      // Ampersand is a valid character in an SSID, but menu2 uses it to mark
      // "mnemonics" for keyboard shortcuts.
      std::string wifi_name = EscapeAmpersands(wifi_networks[i]->name());
      if (wifi_networks[i]->connecting()) {
        label = l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
            UTF8ToUTF16(wifi_name),
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING));
      } else {
        label = UTF8ToUTF16(wifi_name);
      }

      // First add a separator if necessary.
      if (!separator_added) {
        separator_added = true;
        if (!menu_items_.empty()) {  // Don't add if first menu item.
          menu_items_.push_back(MenuItem());  // Separator
        }
      }

      int flag = FLAG_WIFI;
      // If a network is not connectable (e.g. it requires certificates and
      // the user is not logged in), we disable it.
      if (!cros->CanConnectToNetwork(wifi_networks[i]))
        flag |= FLAG_DISABLED;
      if (ShouldHighlightNetwork(wifi_networks[i]))
        flag |= FLAG_ASSOCIATED;
      const SkBitmap icon = NetworkMenuIcon::GetBitmap(wifi_networks[i],
          NetworkMenuIcon::COLOR_DARK);
      menu_items_.push_back(
          MenuItem(ui::MenuModel::TYPE_COMMAND,
                   label, icon, wifi_networks[i]->service_path(), flag));
    }
    if (!separator_added && !menu_items_.empty())
      menu_items_.push_back(MenuItem());
    menu_items_.push_back(MenuItem(
        ui::MenuModel::TYPE_COMMAND,
        l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_OTHER_WIFI_NETWORKS),
        NetworkMenuIcon::GetConnectedBitmap(NetworkMenuIcon::ARCS,
                                            NetworkMenuIcon::COLOR_DARK),
        std::string(), FLAG_ADD_WIFI));
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

      // This is currently only used in the OOBE/login screen, do not show
      // activating 3G option.
      if (activation_state != ACTIVATION_STATE_ACTIVATED)
        continue;

      // Ampersand is a valid character in a network name, but menu2 uses it
      // to mark "mnemonics" for keyboard shortcuts.  http://crosbug.com/14697
      std::string network_name = EscapeAmpersands(cell_networks[i]->name());
      if (activation_state == ACTIVATION_STATE_NOT_ACTIVATED ||
          activation_state == ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
        label = l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_DEVICE_ACTIVATE,
            UTF8ToUTF16(network_name));
      } else if (activation_state == ACTIVATION_STATE_ACTIVATING) {
        label = l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
            UTF8ToUTF16(network_name),
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ACTIVATING));
      } else if (cell_networks[i]->connecting()) {
        label = l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
            UTF8ToUTF16(network_name),
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING));
      } else {
        label = UTF8ToUTF16(network_name);
      }

      // First add a separator if necessary.
      if (!separator_added) {
        separator_added = true;
        if (!menu_items_.empty()) {  // Don't add if first menu item.
          menu_items_.push_back(MenuItem());  // Separator
        }
      }

      int flag = FLAG_CELLULAR;
      // If wifi is associated, then cellular is not active.
      bool isActive = ShouldHighlightNetwork(cell_networks[i]);
      bool supports_data_plan =
          active_cellular && active_cellular->SupportsDataPlan();
      if (isActive)
        flag |= FLAG_ASSOCIATED;
      const SkBitmap icon = NetworkMenuIcon::GetBitmap(cell_networks[i],
          NetworkMenuIcon::COLOR_DARK);
      menu_items_.push_back(
          MenuItem(ui::MenuModel::TYPE_COMMAND,
                   label, icon, cell_networks[i]->service_path(), flag));
      if (isActive && supports_data_plan) {
        label.clear();
        if (active_cellular->needs_new_plan()) {
          label = l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_NO_PLAN_LABEL);
        } else {
          const chromeos::CellularDataPlan* plan =
              cros->GetSignificantDataPlan(active_cellular->service_path());
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
    const NetworkDevice* cellular_device = cros->FindCellularDevice();
    if (cellular_device) {
      // NOTE: This is currently only used in login/OOBE screen. So do not add
      // "View Account" with top up URL.

      if (cellular_device->support_network_scan()) {
        // For GSM add mobile network scan.
        if (!separator_added && !menu_items_.empty())
          menu_items_.push_back(MenuItem());

        menu_items_.push_back(MenuItem(
            ui::MenuModel::TYPE_COMMAND,
            l10n_util::GetStringUTF16(
                IDS_OPTIONS_SETTINGS_OTHER_CELLULAR_NETWORKS),
            NetworkMenuIcon::GetDisconnectedBitmap(NetworkMenuIcon::BARS,
                                                   NetworkMenuIcon::COLOR_DARK),
            std::string(), FLAG_ADD_CELLULAR));
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

  bool show_wifi_scanning = wifi_available && cros->wifi_scanning();
  // Do not show disable wifi during oobe
  bool show_toggle_wifi = wifi_available &&
      (should_open_button_options || !wifi_enabled);
  // Do not show disable cellular during oobe
  bool show_toggle_cellular = cellular_available &&
      (should_open_button_options || !cellular_enabled);

  if (show_wifi_scanning || show_toggle_wifi || show_toggle_cellular) {
    menu_items_.push_back(MenuItem());  // Separator

    if (show_wifi_scanning) {
      // Add 'Scanning...'
      label = l10n_util::GetStringUTF16(IDS_STATUSBAR_WIFI_SCANNING_MESSAGE);
      menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), FLAG_DISABLED));
    }

    if (show_toggle_wifi) {
      int id = wifi_enabled ? IDS_STATUSBAR_NETWORK_DEVICE_DISABLE :
                              IDS_STATUSBAR_NETWORK_DEVICE_ENABLE;
      label = l10n_util::GetStringFUTF16(id,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI));
      int flag = FLAG_TOGGLE_WIFI;
      if (cros->wifi_busy())
        flag |= FLAG_DISABLED;
      menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
          SkBitmap(), std::string(), flag));
    }

    if (show_toggle_cellular) {
      const NetworkDevice* cellular = cros->FindCellularDevice();
      bool is_locked = false;
      if (!cellular) {
        LOG(ERROR) << "Didn't find cellular device.";
      } else {
        // If cellular is SIM locked then show "Enable" action.
        is_locked = cellular->sim_lock_state() == SIM_LOCKED_PIN ||
                    cellular->sim_lock_state() == SIM_LOCKED_PUK;
      }
      int id;
      if (cellular_enabled && !is_locked)
        id = IDS_STATUSBAR_NETWORK_DEVICE_DISABLE;
      else
        id = IDS_STATUSBAR_NETWORK_DEVICE_ENABLE;
      label = l10n_util::GetStringFUTF16(id,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CELLULAR));
      SkBitmap icon;
      if (is_locked) {
        icon = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE);
      }
      int flag = FLAG_TOGGLE_CELLULAR;
      if (cros->cellular_busy())
        flag |= FLAG_DISABLED;
      menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
          icon, std::string(), flag));
    }
  }

  // Offline mode.
  // TODO(chocobo): Uncomment once we figure out how to do offline mode.
  // menu_items_.push_back(MenuItem(cros->offline_mode() ?
  //     ui::MenuModel::TYPE_CHECK : ui::MenuModel::TYPE_COMMAND,
  //     l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_OFFLINE_MODE),
  //     SkBitmap(), std::string(), FLAG_TOGGLE_OFFLINE));

  // Additional links like:
  // * Network settings;
  // * IP Address on active interface;
  // * Hardware addresses for wifi and ethernet.
  more_menu_model_->InitMenuItems(should_open_button_options);
  if (!more_menu_model_->menu_items().empty()) {
    menu_items_.push_back(MenuItem());  // Separator
    menu_items_.push_back(MenuItem(
        ui::MenuModel::TYPE_SUBMENU,
        l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_MORE),
        SkBitmap(), more_menu_model_.get(), FLAG_NONE));
  }
}

int MainMenuModel::GetCommandIdAt(int index) const {
  return index + kMainIndexMask;
}

////////////////////////////////////////////////////////////////////////////////
// VPNMenuModel

void VPNMenuModel::InitMenuItems(bool should_open_button_options) {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().

  menu_items_.clear();

  // Populate our MenuItems with the current list of virtual networks.
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const VirtualNetworkVector& virtual_networks = cros->virtual_networks();
  const VirtualNetwork* active_vpn = cros->virtual_network();

  string16 label;

  for (size_t i = 0; i < virtual_networks.size(); ++i) {
    const VirtualNetwork* vpn = virtual_networks[i];
    if (vpn->connecting()) {
      label = l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
          UTF8ToUTF16(vpn->name()),
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING));
    } else {
      label = UTF8ToUTF16(vpn->name());
    }

    int flag = FLAG_VPN;
    if (!cros->CanConnectToNetwork(vpn))
      flag |= FLAG_DISABLED;
    if (ShouldHighlightNetwork(vpn))
      flag |= FLAG_ASSOCIATED;
    const SkBitmap icon = NetworkMenuIcon::GetBitmap(vpn,
        NetworkMenuIcon::COLOR_DARK);
    menu_items_.push_back(
        MenuItem(ui::MenuModel::TYPE_COMMAND,
                 label, icon, vpn->service_path(), flag));
  }

  // Add option to add/disconnect from vpn.
  if (!menu_items_.empty()) {  // Add separator if menu is not empty.
    menu_items_.push_back(MenuItem());
  }
  // Can only connect to a VPN if we have a connected network.
  if (cros->connected_network()) {
    menu_items_.push_back(MenuItem(
        ui::MenuModel::TYPE_COMMAND,
        l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_ADD_VPN),
        SkBitmap(), std::string(), FLAG_ADD_VPN));
  }
  // Show disconnect if we have an active VPN.
  if (active_vpn) {
    menu_items_.push_back(MenuItem(
        ui::MenuModel::TYPE_COMMAND,
        l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DISCONNECT_VPN),
        SkBitmap(), std::string(), FLAG_DISCONNECT_VPN));
  }
}

int VPNMenuModel::GetCommandIdAt(int index) const {
  return index + kVPNIndexMask;
}

////////////////////////////////////////////////////////////////////////////////
// MoreMenuModel

void MoreMenuModel::InitMenuItems(bool should_open_button_options) {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().

  menu_items_.clear();
  MenuItemVector link_items;
  MenuItemVector address_items;

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  bool connected = cros->Connected();  // always call for test expectations.

  int message_id = -1;
  if (connected)
    message_id = IDS_STATUSBAR_NETWORK_OPEN_PROXY_SETTINGS_DIALOG;
  if (message_id != -1) {
    link_items.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND,
                                  l10n_util::GetStringUTF16(message_id),
                                  SkBitmap(), std::string(), FLAG_OPTIONS));
  }

  if (connected) {
    std::string ip_address = cros->IPAddress();
    if (!ip_address.empty()) {
      address_items.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND,
          ASCIIToUTF16(cros->IPAddress()), SkBitmap(), std::string(),
                       FLAG_DISABLED));
    }
  }

  const NetworkDevice* ether = cros->FindEthernetDevice();
  if (ether) {
    std::string hardware_address;
    cros->GetIPConfigs(ether->device_path(), &hardware_address,
        NetworkLibrary::FORMAT_COLON_SEPARATED_HEX);
    if (!hardware_address.empty()) {
      std::string label = l10n_util::GetStringUTF8(
          IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET) + " " + hardware_address;
      address_items.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND,
          UTF8ToUTF16(label), SkBitmap(), std::string(), FLAG_DISABLED));
    }
  }

  if (cros->wifi_enabled()) {
    const NetworkDevice* wifi = cros->FindWifiDevice();
    if (wifi) {
      std::string hardware_address;
      cros->GetIPConfigs(wifi->device_path(),
          &hardware_address, NetworkLibrary::FORMAT_COLON_SEPARATED_HEX);
      if (!hardware_address.empty()) {
        std::string label = l10n_util::GetStringUTF8(
            IDS_STATUSBAR_NETWORK_DEVICE_WIFI) + " " + hardware_address;
        address_items.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND,
            UTF8ToUTF16(label), SkBitmap(), std::string(), FLAG_DISABLED));
      }
    }
  }

  menu_items_ = link_items;
  if (!menu_items_.empty() && address_items.size() > 1)
    menu_items_.push_back(MenuItem());  // Separator
  menu_items_.insert(menu_items_.end(),
      address_items.begin(), address_items.end());
}

int MoreMenuModel::GetCommandIdAt(int index) const {
  return index + kMoreIndexMask;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenu

NetworkMenu::NetworkMenu(Delegate* delegate)
    : delegate_(delegate),
      refreshing_menu_(false),
      menu_item_view_(NULL),
      min_width_(kDefaultMinimumWidth),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_pointer_factory_(this)) {
  main_menu_model_.reset(new MainMenuModel(weak_pointer_factory_.GetWeakPtr()));
  menu_model_adapter_.reset(
      new views::MenuModelAdapter(main_menu_model_.get()));
  menu_item_view_ = new views::MenuItemView(menu_model_adapter_.get());
  menu_item_view_->set_has_icons(true);
  menu_runner_.reset(new views::MenuRunner(menu_item_view_));
}

NetworkMenu::~NetworkMenu() {
}

ui::MenuModel* NetworkMenu::GetMenuModel() {
  return main_menu_model_.get();
}

void NetworkMenu::CancelMenu() {
  menu_runner_->Cancel();
}

void NetworkMenu::UpdateMenu() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  refreshing_menu_ = true;
  main_menu_model_->InitMenuItems(delegate_->ShouldOpenButtonOptions());

  menu_model_adapter_->BuildMenu(menu_item_view_);
  SetMenuMargins(menu_item_view_, kTopMargin, kBottomMargin);
  menu_item_view_->ChildrenChanged();

  refreshing_menu_ = false;
}

void NetworkMenu::RunMenu(views::View* source) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->RequestNetworkScan();

  UpdateMenu();

  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());
  menu_item_view_->GetSubmenu()->set_minimum_preferred_width(min_width_);
  if (menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
          delegate_->GetMenuButton(), bounds, views::MenuItemView::TOPRIGHT,
          views::MenuRunner::HAS_MNEMONICS) == views::MenuRunner::MENU_DELETED)
    return;
}

void NetworkMenu::ShowTabbedNetworkSettings(const Network* network) const {
  if (!UserManager::Get()->IsSessionStarted())
    return;

  DCHECK(network);
  Browser* browser = GetAppropriateBrowser();

  std::string network_name(network->name());
  if (network_name.empty() && network->type() == chromeos::TYPE_ETHERNET) {
    network_name = l10n_util::GetStringUTF8(
        IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  }
  std::string page = StringPrintf(
      "%s?servicePath=%s&networkType=%d&networkName=%s",
      chrome::kInternetOptionsSubPage,
      net::EscapeUrlEncodedData(network->service_path(), true).c_str(),
      network->type(),
      net::EscapeUrlEncodedData(network_name, false).c_str());
  browser->ShowOptionsTab(page);
}

void NetworkMenu::DoConnect(Network* network) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (network->type() == TYPE_VPN) {
    VirtualNetwork* vpn = static_cast<VirtualNetwork*>(network);
    if (vpn->NeedMoreInfoToConnect()) {
      // Show the connection UI if info for a field is missing.
      NetworkConfigView* view = new NetworkConfigView(vpn);
      views::Widget* window = views::Widget::CreateWindowWithParent(
          view, delegate()->GetNativeWindow());
      window->SetAlwaysOnTop(true);
      window->Show();
    } else {
      cros->ConnectToVirtualNetwork(vpn);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
    }
  }
  if (network->type() == TYPE_WIFI) {
    WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
    if (wifi->IsPassphraseRequired()) {
      // Show the connection UI if we require a passphrase.
      NetworkConfigView* view = new NetworkConfigView(wifi);
      views::Widget* window = views::Widget::CreateWindowWithParent(
          view, delegate()->GetNativeWindow());
      window->SetAlwaysOnTop(true);
      window->Show();
    } else {
      cros->ConnectToWifiNetwork(wifi);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
    }
  }
}

void NetworkMenu::ToggleWifi() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->EnableWifiNetworkDevice(!cros->wifi_enabled());
}

void NetworkMenu::ToggleCellular() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const NetworkDevice* cellular = cros->FindCellularDevice();
  if (!cellular) {
    LOG(ERROR) << "No cellular device found, it should be available.";
    cros->EnableCellularNetworkDevice(!cros->cellular_enabled());
  } else if (!cellular->is_sim_locked()) {
    if (cellular->is_sim_absent()) {
      if (!chromeos::UserManager::Get()->IsSessionStarted())
        return;
      std::string setup_url;
      MobileConfig* config = MobileConfig::GetInstance();
      if (config->IsReady()) {
        const MobileConfig::LocaleConfig* locale_config =
            config->GetLocaleConfig();
        if (locale_config)
          setup_url = locale_config->setup_url();
      }
      if (!setup_url.empty()) {
        GetAppropriateBrowser()->ShowSingletonTab(GURL(setup_url));
      } else {
        // TODO(nkostylev): Show generic error message. http://crosbug.com/15444
      }
    } else {
      cros->EnableCellularNetworkDevice(!cros->cellular_enabled());
    }
  } else {
    SimDialogDelegate::ShowDialog(delegate()->GetNativeWindow(),
                                  SimDialogDelegate::SIM_DIALOG_UNLOCK);
  }
}

void NetworkMenu::ShowOtherWifi() {
  NetworkConfigView* view = new NetworkConfigView(TYPE_WIFI);
  views::Widget* window = views::Widget::CreateWindowWithParent(
      view, delegate_->GetNativeWindow());
  window->SetAlwaysOnTop(true);
  window->Show();
}

void NetworkMenu::ShowOtherCellular() {
  ChooseMobileNetworkDialog::ShowDialog(delegate_->GetNativeWindow());
}

bool NetworkMenu::ShouldHighlightNetwork(const Network* network) {
  return ::ShouldHighlightNetwork(network);
}

Browser* NetworkMenu::GetAppropriateBrowser() const {
  DCHECK(chromeos::UserManager::Get()->IsSessionStarted());
  return Browser::GetOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord());
}

}  // namespace chromeos
