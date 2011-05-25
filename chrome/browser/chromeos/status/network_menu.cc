// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu.h"

#include <algorithm>

#include "base/logging.h"
#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/choose_mobile_network_dialog.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/skbitmap_operations.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"
#include "views/window/window.h"

namespace {

// Amount to fade icons while connecting.
const double kConnectingImageAlpha = 0.5;

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

// Offsets for views menu ids (main menu and submenu ids use the same
// namespace).
const int kMainIndexOffset = 1000;
const int kVPNIndexOffset  = 2000;
const int kMoreIndexOffset = 3000;

// Default minimum width in pixels of the menu to prevent unnecessary
// resizing as networks are updated.
const int kDefaultMinimumWidth = 280;

// Menu item margins.
const int kTopMargin = 5;
const int kBottomMargin = 7;

}  // namespace

namespace chromeos {

class NetworkMenuModel : public views::MenuDelegate {
 public:
  struct NetworkInfo {
    NetworkInfo();
    ~NetworkInfo();

    // "ethernet" | "wifi" | "cellular" | "other".
    std::string network_type;
    // "connected" | "connecting" | "disconnected" | "error".
    std::string status;
    // status message or error message, empty if unknown status.
    std::string message;
    // IP address (if network is active, empty otherwise)
    std::string ip_address;
    // Remembered passphrase.
    std::string passphrase;
    // true if the network requires a passphrase.
    bool need_passphrase;
    // true if the network is currently remembered.
    bool remembered;
    // true if the network is auto connect (meaningful for Wifi only).
    bool auto_connect;
  };

  explicit NetworkMenuModel(NetworkMenu* owner);
  virtual ~NetworkMenuModel();

  // Connect or reconnect to the network at |index|.
  // If remember >= 0, set the favorite state of the network.
  void ConnectToNetworkAt(int index,
                          const std::string& passphrase,
                          const std::string& ssid,
                          int remember) const;

  // Called by NetworkMenu::RunMenu to initialize list of menu items.
  virtual void InitMenuItems(bool is_browser_mode,
                             bool should_open_button_options) = 0;

  // PopulateMenu() clears and reinstalls the menu items defined in this
  // instance by calling PopulateMenuItem() on each one.  Subclasses override
  // PopulateMenuItem(), transform command_id into the correct range for
  // the menu, and call the base class PopulateMenuItem().
  virtual void PopulateMenu(views::MenuItemView* menu);
  virtual void PopulateMenuItem(views::MenuItemView* menu,
                                int index,
                                int command_id);

  // Menu item field accessors.
  int GetItemCount() const;
  ui::MenuModel::ItemType GetTypeAt(int index) const;
  string16 GetLabelAt(int index) const;
  const gfx::Font* GetLabelFontAt(int index) const;
  bool IsItemCheckedAt(int index) const;
  bool GetIconAt(int index, SkBitmap* icon);
  bool IsEnabledAt(int index) const;
  NetworkMenuModel* GetSubmenuModelAt(int index) const;
  void ActivatedAt(int index);

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

  struct MenuItem {
    MenuItem()
        : type(ui::MenuModel::TYPE_SEPARATOR),
          sub_menu_model(NULL),
          flags(0) {}
    MenuItem(ui::MenuModel::ItemType type, string16 label, SkBitmap icon,
             const std::string& service_path, int flags)
        : type(type),
          label(label),
          icon(icon),
          service_path(service_path),
          sub_menu_model(NULL),
          flags(flags) {}
    MenuItem(ui::MenuModel::ItemType type, string16 label, SkBitmap icon,
             NetworkMenuModel* sub_menu_model, int flags)
        : type(type),
          label(label),
          icon(icon),
          sub_menu_model(sub_menu_model),
          flags(flags) {}

    ui::MenuModel::ItemType type;
    string16 label;
    SkBitmap icon;
    std::string service_path;
    NetworkMenuModel* sub_menu_model;  // Weak.
    int flags;
  };
  typedef std::vector<MenuItem> MenuItemVector;

  // Our menu items.
  MenuItemVector menu_items_;

  NetworkMenu* owner_;  // Weak pointer to NetworkMenu that owns this MenuModel.

  // Top up URL of the current carrier on empty string if there's none.
  std::string top_up_url_;

  // Carrier ID which top up URL is initialized for.
  // Used to update top up URL only when cellular carrier has changed.
  std::string carrier_id_;

 private:
  // Show a NetworkConfigView modal dialog instance.
  void ShowNetworkConfigView(NetworkConfigView* view) const;

  void ActivateCellular(const CellularNetwork* cellular) const;
  void ShowOther(ConnectionType type) const;
  void ShowOtherCellular() const;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuModel);
};


class MoreMenuModel : public NetworkMenuModel {
 public:
  explicit MoreMenuModel(NetworkMenu* owner);
  virtual ~MoreMenuModel() {}

  // NetworkMenuModel implementation.
  virtual void InitMenuItems(bool is_browser_mode,
                             bool should_open_button_options) OVERRIDE;
  virtual void PopulateMenuItem(views::MenuItemView* menu,
                                int index,
                                int command_id) OVERRIDE;

 private:
  friend class MainMenuModel;
  DISALLOW_COPY_AND_ASSIGN(MoreMenuModel);
};

class VPNMenuModel : public NetworkMenuModel {
 public:
  explicit VPNMenuModel(NetworkMenu* owner);
  virtual ~VPNMenuModel() {}

  // NetworkMenuModel implementation.
  virtual void InitMenuItems(bool is_browser_mode,
                             bool should_open_button_options) OVERRIDE;
  virtual void PopulateMenuItem(views::MenuItemView* menu,
                                int index,
                                int command_id) OVERRIDE;

  static SkBitmap IconForDisplay(const Network* network);

 private:
  DISALLOW_COPY_AND_ASSIGN(VPNMenuModel);
};

class MainMenuModel : public NetworkMenuModel {
 public:
  explicit MainMenuModel(NetworkMenu* owner);
  virtual ~MainMenuModel() {}

  // NetworkMenuModel implementation.
  virtual void InitMenuItems(bool is_browser_mode,
                             bool should_open_button_options) OVERRIDE;
  virtual void PopulateMenuItem(views::MenuItemView* menu,
                                int index,
                                int command_id) OVERRIDE;

  // views::MenuDelegate implementation.
  virtual const gfx::Font& GetLabelFont(int id) const OVERRIDE;
  virtual bool IsItemChecked(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;

 private:
  scoped_ptr<NetworkMenuModel> vpn_menu_model_;
  scoped_ptr<MoreMenuModel> more_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(MainMenuModel);
};

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuModel::NetworkInfo

NetworkMenuModel::NetworkInfo::NetworkInfo()
    : need_passphrase(false), remembered(true), auto_connect(true) {
}

NetworkMenuModel::NetworkInfo::~NetworkInfo() {}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuModel, public methods:

NetworkMenuModel::NetworkMenuModel(NetworkMenu* owner) : owner_(owner) {}

NetworkMenuModel::~NetworkMenuModel() {}

void NetworkMenuModel::PopulateMenu(views::MenuItemView* menu) {
  if (menu->HasSubmenu()) {
    const int old_count = menu->GetSubmenu()->child_count();
    for (int i = 0; i < old_count; ++i)
      menu->RemoveMenuItemAt(0);
  }

  const int menu_items_count = GetItemCount();
  for (int i = 0; i < menu_items_count; ++i)
    PopulateMenuItem(menu, i, i);
}

void NetworkMenuModel::PopulateMenuItem(
    views::MenuItemView* menu,
    int index,
    int command_id) {
  DCHECK_GT(GetItemCount(), index);
  switch (GetTypeAt(index)) {
    case ui::MenuModel::TYPE_SEPARATOR:
      menu->AppendSeparator();
      break;
    case ui::MenuModel::TYPE_COMMAND: {
      views::MenuItemView* item = NULL;
      SkBitmap icon;
      if (GetIconAt(index, &icon)) {
        item = menu->AppendMenuItemWithIcon(command_id,
                                            UTF16ToWide(GetLabelAt(index)),
                                            icon);
      } else {
        item = menu->AppendMenuItemWithLabel(command_id,
                                             UTF16ToWide(GetLabelAt(index)));
      }
      item->set_margins(kTopMargin, kBottomMargin);
      break;
    }
    case ui::MenuModel::TYPE_SUBMENU: {
      views::MenuItemView* submenu = NULL;
      SkBitmap icon;
      if (GetIconAt(index, &icon)) {
        submenu = menu->AppendSubMenuWithIcon(command_id,
                                              UTF16ToWide(GetLabelAt(index)),
                                              icon);
      } else {
        submenu = menu->AppendSubMenu(command_id,
                                      UTF16ToWide(GetLabelAt(index)));
      }
      submenu->set_margins(kTopMargin, kBottomMargin);
      GetSubmenuModelAt(index)->PopulateMenu(submenu);
      break;
    }
    default:
      NOTREACHED();
  }
}

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
      if (wifi->connecting_or_connected()) {
        // Show the config settings for the active network.
        owner_->ShowTabbedNetworkSettings(wifi);
      } else if (wifi->IsPassphraseRequired()) {
        // Show the connection UI if we require a passphrase.
        ShowNetworkConfigView(new NetworkConfigView(wifi));
      } else {
        cros->ConnectToWifiNetwork(wifi);
        // Connection failures are responsible for updating the UI, including
        // reopening dialogs.
      }
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
      if ((cellular->activation_state() != ACTIVATION_STATE_ACTIVATED &&
           cellular->activation_state() != ACTIVATION_STATE_UNKNOWN) ||
          cellular->needs_new_plan()) {
        ActivateCellular(cellular);
      } else if (cellular->connecting_or_connected()) {
        // Cellular network is connecting or connected,
        // so we show the config settings for the cellular network.
        owner_->ShowTabbedNetworkSettings(cellular);
      } else {
        // Clicked on a disconnected cellular network, so connect to it.
        cros->ConnectToCellularNetwork(cellular);
      }
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
    ShowOtherCellular();
  } else if (flags & FLAG_ADD_VPN) {
    ShowOther(TYPE_VPN);
  } else if (flags & FLAG_VPN) {
    VirtualNetwork* vpn = cros->FindVirtualNetworkByPath(service_path);
    if (vpn) {
      // Connect or reconnect.
      if (vpn->connecting_or_connected()) {
        // Show the config settings for the connected network.
        if (cros->connected_network())
          owner_->ShowTabbedNetworkSettings(cros->connected_network());
      } else if (vpn->NeedMoreInfoToConnect()) {
      // Show the connection UI if info for a field is missing.
        ShowNetworkConfigView(new NetworkConfigView(vpn));
      } else {
        cros->ConnectToVirtualNetwork(vpn);
        // Connection failures are responsible for updating the UI, including
        // reopening dialogs.
      }
    } else {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      LOG(WARNING) << "VPN does not exist to connect to: " << service_path;
      // TODO(stevenjb): Show notification.
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuModel, ui::MenuModel implementation:

int NetworkMenuModel::GetItemCount() const {
  return static_cast<int>(menu_items_.size());
}

ui::MenuModel::ItemType NetworkMenuModel::GetTypeAt(int index) const {
  return menu_items_[index].type;
}

string16 NetworkMenuModel::GetLabelAt(int index) const {
  return menu_items_[index].label;
}

const gfx::Font* NetworkMenuModel::GetLabelFontAt(int index) const {
  return (menu_items_[index].flags & FLAG_ASSOCIATED) ?
      &ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BoldFont) :
      NULL;
}

bool NetworkMenuModel::IsItemCheckedAt(int index) const {
  // All ui::MenuModel::TYPE_CHECK menu items are checked.
  return true;
}

bool NetworkMenuModel::GetIconAt(int index, SkBitmap* icon) {
  if (!menu_items_[index].icon.empty()) {
    *icon = menu_items_[index].icon;
    return true;
  }
  return false;
}

bool NetworkMenuModel::IsEnabledAt(int index) const {
  return !(menu_items_[index].flags & FLAG_DISABLED);
}

NetworkMenuModel* NetworkMenuModel::GetSubmenuModelAt(int index) const {
  return menu_items_[index].sub_menu_model;
}

void NetworkMenuModel::ActivatedAt(int index) {
  // When we are refreshing the menu, ignore menu item activation.
  if (owner_->refreshing_menu_)
    return;

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  int flags = menu_items_[index].flags;
  if (flags & FLAG_OPTIONS) {
    owner_->OpenButtonOptions();
  } else if (flags & FLAG_TOGGLE_ETHERNET) {
    cros->EnableEthernetNetworkDevice(!cros->ethernet_enabled());
  } else if (flags & FLAG_TOGGLE_WIFI) {
    cros->EnableWifiNetworkDevice(!cros->wifi_enabled());
  } else if (flags & FLAG_TOGGLE_CELLULAR) {
    const NetworkDevice* cellular = cros->FindCellularDevice();
    if (!cellular) {
      LOG(ERROR) << "Not found cellular device, it should be available.";
      cros->EnableCellularNetworkDevice(!cros->cellular_enabled());
    } else {
      if (cellular->sim_lock_state() == SIM_UNLOCKED ||
          cellular->sim_lock_state() == SIM_UNKNOWN) {
        cros->EnableCellularNetworkDevice(!cros->cellular_enabled());
      } else {
        SimDialogDelegate::ShowDialog(owner_->GetNativeWindow(),
            SimDialogDelegate::SIM_DIALOG_UNLOCK);
      }
    }
  } else if (flags & FLAG_TOGGLE_OFFLINE) {
    cros->EnableOfflineMode(!cros->offline_mode());
  } else if (flags & FLAG_ETHERNET) {
    if (cros->ethernet_connected()) {
      owner_->ShowTabbedNetworkSettings(cros->ethernet_network());
    }
  } else if (flags & (FLAG_WIFI | FLAG_ADD_WIFI |
                      FLAG_CELLULAR | FLAG_ADD_CELLULAR |
                      FLAG_VPN | FLAG_ADD_VPN)) {
    ConnectToNetworkAt(index, std::string(), std::string(), -1);
  } else if (flags & FLAG_DISCONNECT_VPN) {
    const VirtualNetwork* active_vpn = cros->virtual_network();
    if (active_vpn)
      cros->DisconnectFromNetwork(active_vpn);
  } else if (flags & FLAG_VIEW_ACCOUNT) {
    Browser* browser = BrowserList::GetLastActive();
    if (browser)
      browser->ShowSingletonTab(GURL(top_up_url_));
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuModel, private methods:

// TODO(stevenjb): deprecate this once we've committed to tabbed settings
// and the embedded menu UI (and fully deprecated NetworkConfigView).
// Meanwhile, if MenuUI::IsEnabled() is true, always show the settings UI,
// otherwise show NetworkConfigView only to get passwords when not connected.
void NetworkMenuModel::ShowNetworkConfigView(NetworkConfigView* view) const {
  views::Window* window = browser::CreateViewsWindow(
      owner_->GetNativeWindow(), gfx::Rect(), view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void NetworkMenuModel::ActivateCellular(const CellularNetwork* cellular) const {
  DCHECK(cellular);
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;
  browser->OpenMobilePlanTabAndActivate();
}

void NetworkMenuModel::ShowOther(ConnectionType type) const {
  ShowNetworkConfigView(new NetworkConfigView(type));
}

void NetworkMenuModel::ShowOtherCellular() const {
  ChooseMobileNetworkDialog::ShowDialog(owner_->GetNativeWindow());
}

////////////////////////////////////////////////////////////////////////////////
// MainMenuModel

MainMenuModel::MainMenuModel(NetworkMenu* owner)
    : NetworkMenuModel(owner),
      vpn_menu_model_(new VPNMenuModel(owner)),
      more_menu_model_(new MoreMenuModel(owner)) {
}

void MainMenuModel::InitMenuItems(bool is_browser_mode,
                                  bool should_open_button_options) {
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
    const SkBitmap* icon = rb.GetBitmapNamed(IDR_STATUSBAR_WIRED_BLACK);
    const SkBitmap* badge = ethernet_connecting || ethernet_connected ?
        NULL : rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED);
    int flag = FLAG_ETHERNET;
    if (ethernet_connecting || ethernet_connected)
      flag |= FLAG_ASSOCIATED;
    menu_items_.push_back(
        MenuItem(ui::MenuModel::TYPE_COMMAND, label,
                 NetworkMenu::IconForDisplay(icon, badge), std::string(),
                 flag));
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

      const SkBitmap* icon = NetworkMenu::IconForNetworkStrength(
          wifi_networks[i], true);
      const SkBitmap* badge = wifi_networks[i]->encrypted() ?
          rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE) : NULL;
      int flag = FLAG_WIFI;
      // If a network is not connectable from login/oobe, we disable it.
      // We do not allow configuring a network (e.g. 802.1x) from login/oobe.
      if (!owner_->IsBrowserMode() && !wifi_networks[i]->connectable())
        flag |= FLAG_DISABLED;
      if (active_wifi
          && wifi_networks[i]->service_path() == active_wifi->service_path())
        flag |= FLAG_ASSOCIATED;
      menu_items_.push_back(
          MenuItem(ui::MenuModel::TYPE_COMMAND, label,
                   NetworkMenu::IconForDisplay(icon, badge),
                   wifi_networks[i]->service_path(), flag));
    }
    if (!separator_added && !menu_items_.empty())
      menu_items_.push_back(MenuItem());
    menu_items_.push_back(MenuItem(
        ui::MenuModel::TYPE_COMMAND,
        l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_OTHER_WIFI_NETWORKS),
        *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0_BLACK),
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

      // If we are on the OOBE/login screen, do not show activating 3G option.
      if (!is_browser_mode && activation_state != ACTIVATION_STATE_ACTIVATED)
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

      const SkBitmap* icon = NetworkMenu::IconForNetworkStrength(
          cell_networks[i], true);
      const SkBitmap* badge = NetworkMenu::BadgeForNetworkTechnology(
          cell_networks[i]);
      const SkBitmap* roaming_badge = NetworkMenu::BadgeForRoamingStatus(
          cell_networks[i]);
      int flag = FLAG_CELLULAR;
      bool isActive = active_cellular &&
          cell_networks[i]->service_path() == active_cellular->service_path() &&
          (cell_networks[i]->connecting() || cell_networks[i]->connected());
      bool supports_data_plan =
          active_cellular && active_cellular->SupportsDataPlan();
      if (isActive)
        flag |= FLAG_ASSOCIATED;
      menu_items_.push_back(
          MenuItem(ui::MenuModel::TYPE_COMMAND, label,
                   NetworkMenu::IconForDisplay(icon, badge, roaming_badge,
                                               NULL),
                   cell_networks[i]->service_path(), flag));
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
      // Add "View Account" with top up URL if we know that.
      ServicesCustomizationDocument* customization =
          ServicesCustomizationDocument::GetInstance();
      if (is_browser_mode && customization->IsReady()) {
        std::string carrier_id = cros->GetCellularHomeCarrierId();
        // If we don't have top up URL cached.
        if (carrier_id != carrier_id_) {
          // Mark that we've checked this carrier ID.
          carrier_id_ = carrier_id;
          top_up_url_.clear();
          // Ignoring deal restrictions, use any carrier information available.
          const ServicesCustomizationDocument::CarrierDeal* deal =
              customization->GetCarrierDeal(carrier_id, false);
          if (deal && !deal->top_up_url.empty())
            top_up_url_ = deal->top_up_url;
        }
        if (!top_up_url_.empty()) {
          menu_items_.push_back(MenuItem(
              ui::MenuModel::TYPE_COMMAND,
              l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_VIEW_ACCOUNT),
              SkBitmap(),
              std::string(), FLAG_VIEW_ACCOUNT));
        }
      }

      if (cellular_device->support_network_scan()) {
        // For GSM add mobile network scan.
        if (!separator_added && !menu_items_.empty())
          menu_items_.push_back(MenuItem());

        menu_items_.push_back(MenuItem(
            ui::MenuModel::TYPE_COMMAND,
            l10n_util::GetStringUTF16(
                IDS_OPTIONS_SETTINGS_OTHER_CELLULAR_NETWORKS),
            *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0_BLACK),
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

  // If there's a connected network, add submenu for Private Networks.
  const Network* connected_network = cros->connected_network();
  if (connected_network) {
    menu_items_.push_back(MenuItem());  // Separator
    menu_items_.push_back(MenuItem(
        ui::MenuModel::TYPE_SUBMENU,
        l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_PRIVATE_NETWORKS),
        VPNMenuModel::IconForDisplay(connected_network),
        vpn_menu_model_.get(), FLAG_NONE));
    vpn_menu_model_->InitMenuItems(
        is_browser_mode, should_open_button_options);
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
      const NetworkDevice* cellular = cros->FindCellularDevice();
      bool is_locked = false;
      if (!cellular) {
        LOG(ERROR) << "Not found cellular device, it should be available.";
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
        icon = NetworkMenu::IconForDisplay(
            rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE), NULL);
      }
      menu_items_.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
          icon, std::string(), FLAG_TOGGLE_CELLULAR));
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
  menu_items_.push_back(MenuItem());  // Separator
  more_menu_model_->InitMenuItems(is_browser_mode, should_open_button_options);
  if (is_browser_mode) {
    // In browser mode we do not want separate submenu, inline items.
    menu_items_.insert(
        menu_items_.end(),
        more_menu_model_->menu_items_.begin(),
        more_menu_model_->menu_items_.end());
  } else {
    if (!more_menu_model_->menu_items_.empty()) {
      menu_items_.push_back(MenuItem(
          ui::MenuModel::TYPE_SUBMENU,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_MORE),
          SkBitmap(), more_menu_model_.get(), FLAG_NONE));
    }
  }
}

void MainMenuModel::PopulateMenuItem(
      views::MenuItemView* menu,
      int index,
      int command_id) {
  NetworkMenuModel::PopulateMenuItem(menu, index,
                                     command_id + kMainIndexOffset);
}

// views::MenuDelegate implementation.

const gfx::Font& MainMenuModel::GetLabelFont(int id) const {
  DCHECK_GT(kMoreIndexOffset, kVPNIndexOffset);
  DCHECK_GT(kVPNIndexOffset, kMainIndexOffset);
  const gfx::Font* font = NULL;
  if (id >= kMoreIndexOffset)
    font = more_menu_model_->GetLabelFontAt(id - kMoreIndexOffset);
  else if (id >= kVPNIndexOffset)
    font = vpn_menu_model_->GetLabelFontAt(id - kVPNIndexOffset);
  else if (id >= kMainIndexOffset)
    font = GetLabelFontAt(id - kMainIndexOffset);

  return font ? *font : views::MenuDelegate::GetLabelFont(id);
}


bool MainMenuModel::IsItemChecked(int id) const {
  DCHECK_GT(kMoreIndexOffset, kVPNIndexOffset);
  DCHECK_GT(kVPNIndexOffset, kMainIndexOffset);
  if (id >= kMoreIndexOffset)
    return more_menu_model_->IsItemCheckedAt(id - kMoreIndexOffset);
  else if (id >= kVPNIndexOffset)
    return vpn_menu_model_->IsItemCheckedAt(id - kVPNIndexOffset);
  else if (id >= kMainIndexOffset)
    return IsItemCheckedAt(id - kMainIndexOffset);

  return views::MenuDelegate::IsItemChecked(id);
}

bool MainMenuModel::IsCommandEnabled(int id) const {
  DCHECK_GT(kMoreIndexOffset, kVPNIndexOffset);
  DCHECK_GT(kVPNIndexOffset, kMainIndexOffset);
  if (id >= kMoreIndexOffset)
    return more_menu_model_->IsEnabledAt(id - kMoreIndexOffset);
  else if (id >= kVPNIndexOffset)
    return vpn_menu_model_->IsEnabledAt(id - kVPNIndexOffset);
  else if (id >= kMainIndexOffset)
    return IsEnabledAt(id - kMainIndexOffset);

  return views::MenuDelegate::IsCommandEnabled(id);
}

void MainMenuModel::ExecuteCommand(int id) {
  DCHECK_GT(kMoreIndexOffset, kVPNIndexOffset);
  DCHECK_GT(kVPNIndexOffset, kMainIndexOffset);
  if (id >= kMoreIndexOffset)
    more_menu_model_->ActivatedAt(id - kMoreIndexOffset);
  else if (id >= kVPNIndexOffset)
    vpn_menu_model_->ActivatedAt(id - kVPNIndexOffset);
  else if (id >= kMainIndexOffset)
    ActivatedAt(id - kMainIndexOffset);
}

////////////////////////////////////////////////////////////////////////////////
// VPNMenuModel

VPNMenuModel::VPNMenuModel(NetworkMenu* owner)
    : NetworkMenuModel(owner) {
}

void VPNMenuModel::InitMenuItems(bool is_browser_mode,
                                 bool should_open_button_options) {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().

  menu_items_.clear();

  // VPN only applies if there's a connected underlying network.
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const Network* connected_network = cros->connected_network();
  if (!connected_network)
    return;

  // Populate our MenuItems with the current list of virtual networks.
  const VirtualNetworkVector& virtual_networks = cros->virtual_networks();
  const VirtualNetwork* active_vpn = cros->virtual_network();
  SkBitmap icon = VPNMenuModel::IconForDisplay(connected_network);
  bool separator_added = false;
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

    // First add a separator if necessary.
    if (!separator_added) {
      separator_added = true;
      if (!menu_items_.empty()) {  // Don't add if first menu item.
        menu_items_.push_back(MenuItem());  // Separator
      }
    }

    int flag = FLAG_VPN;
    if (!vpn->connectable())
      flag |= FLAG_DISABLED;
    if (active_vpn && vpn->service_path() == active_vpn->service_path())
      flag |= FLAG_ASSOCIATED;
    menu_items_.push_back(
        MenuItem(ui::MenuModel::TYPE_COMMAND, label, icon, vpn->service_path(),
                 flag));
  }

  // Add option to add/disconnect from vpn.
  if (!menu_items_.empty()) {  // Add separator if menu is not empty.
    menu_items_.push_back(MenuItem());
  }
  menu_items_.push_back(MenuItem(
      ui::MenuModel::TYPE_COMMAND,
      l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_ADD_VPN),
      SkBitmap(), std::string(), FLAG_ADD_VPN));
  if (active_vpn) {
    menu_items_.push_back(MenuItem(
        ui::MenuModel::TYPE_COMMAND,
        l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DISCONNECT_VPN),
        SkBitmap(), std::string(), FLAG_DISCONNECT_VPN));
  }
}

void VPNMenuModel::PopulateMenuItem(
      views::MenuItemView* menu,
      int index,
      int command_id) {
  NetworkMenuModel::PopulateMenuItem(menu, index,
                                     command_id + kVPNIndexOffset);
}

// static
SkBitmap VPNMenuModel::IconForDisplay(const Network* network) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const SkBitmap* icon = NULL;
  const SkBitmap* bottom_right_badge = NULL;
  const SkBitmap* top_left_badge = NULL;
  // We know for sure |network| is the active network, so no more checking
  // is needed by BadgeForPrivateNetworkStatus, hence pass in NULL.
  const SkBitmap* bottom_left_badge =
      NetworkMenu::BadgeForPrivateNetworkStatus(NULL);

  switch (network->type()) {
    case TYPE_ETHERNET :
      icon = rb.GetBitmapNamed(IDR_STATUSBAR_WIRED_BLACK);
      break;
    case TYPE_WIFI :
      icon = NetworkMenu::IconForNetworkStrength(
          static_cast<const WifiNetwork*>(network), true);
      break;
    case TYPE_CELLULAR : {
      const CellularNetwork* cellular =
          static_cast<const CellularNetwork*>(network);
      icon = NetworkMenu::IconForNetworkStrength(cellular, true);
      bottom_right_badge = NetworkMenu::BadgeForNetworkTechnology(cellular);
      top_left_badge = NetworkMenu::BadgeForRoamingStatus(cellular);
      break;
    }
    default:
      LOG(WARNING) << "VPN not handled for connection type " << network->type();
      return SkBitmap();
  }

  return NetworkMenu::IconForDisplay(icon, bottom_right_badge, top_left_badge,
                                     bottom_left_badge);
}

////////////////////////////////////////////////////////////////////////////////
// MoreMenuModel

MoreMenuModel::MoreMenuModel(NetworkMenu* owner)
    : NetworkMenuModel(owner) {
}

void MoreMenuModel::InitMenuItems(
    bool is_browser_mode, bool should_open_button_options) {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().

  menu_items_.clear();
  MenuItemVector link_items;
  MenuItemVector address_items;

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  bool oobe = !should_open_button_options;  // we don't show options for OOBE.
  if (!oobe) {
    string16 label = l10n_util::GetStringUTF16(is_browser_mode ?
        IDS_STATUSBAR_NETWORK_OPEN_OPTIONS_DIALOG :
        IDS_STATUSBAR_NETWORK_OPEN_PROXY_SETTINGS_DIALOG);
    link_items.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND, label,
                                  SkBitmap(), std::string(), FLAG_OPTIONS));
  }

  bool connected = cros->Connected();  // always call for test expectations.
  if (connected) {
    std::string ip_address = cros->IPAddress();
    if (!ip_address.empty()) {
      address_items.push_back(MenuItem(ui::MenuModel::TYPE_COMMAND,
          ASCIIToUTF16(cros->IPAddress()), SkBitmap(), std::string(),
                       FLAG_DISABLED));
    }
  }

  if (!is_browser_mode) {
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
  }

  menu_items_ = link_items;
  if (!menu_items_.empty() && address_items.size() > 1)
    menu_items_.push_back(MenuItem());  // Separator
  menu_items_.insert(menu_items_.end(),
      address_items.begin(), address_items.end());
}

void MoreMenuModel::PopulateMenuItem(
      views::MenuItemView* menu,
      int index,
      int command_id) {
  NetworkMenuModel::PopulateMenuItem(menu, index,
                                     command_id + kMoreIndexOffset);
}

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

// static
const int NetworkMenu::kBarsImagesOrange[kNumBarsImages] = {
  IDR_STATUSBAR_NETWORK_BARS1_ORANGE,
  IDR_STATUSBAR_NETWORK_BARS2_ORANGE,
  IDR_STATUSBAR_NETWORK_BARS3_ORANGE,
  IDR_STATUSBAR_NETWORK_BARS4_ORANGE,
};
#if 0
// static
const int NetworkMenu::kBarsImagesVLowData[kNumBarsImages] = {
  IDR_STATUSBAR_NETWORK_BARS1_RED,
  IDR_STATUSBAR_NETWORK_BARS2_RED,
  IDR_STATUSBAR_NETWORK_BARS3_RED,
  IDR_STATUSBAR_NETWORK_BARS4_RED,
};
#endif

// static
SkBitmap NetworkMenu::kAnimatingImages[kNumBarsImages];

// static
SkBitmap NetworkMenu::kAnimatingImagesBlack[kNumBarsImages];

NetworkMenu::NetworkMenu() : min_width_(kDefaultMinimumWidth) {
  main_menu_model_.reset(new MainMenuModel(this));
  network_menu_.reset(new views::MenuItemView(main_menu_model_.get()));
  network_menu_->set_has_icons(true);
}

NetworkMenu::~NetworkMenu() {
}

void NetworkMenu::SetFirstLevelMenuWidth(int width) {
  min_width_ = width;
}

void NetworkMenu::CancelMenu() {
  network_menu_->Cancel();
}

void NetworkMenu::UpdateMenu() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  refreshing_menu_ = true;
  main_menu_model_->InitMenuItems(IsBrowserMode(), ShouldOpenButtonOptions());
  main_menu_model_->PopulateMenu(network_menu_.get());
  network_menu_->ChildrenChanged();
  refreshing_menu_ = false;
}

// static
const SkBitmap* NetworkMenu::IconForNetworkStrength(const WifiNetwork* wifi,
                                                    bool black) {
  DCHECK(wifi);
  if (wifi->strength() == 0) {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        black ? IDR_STATUSBAR_NETWORK_BARS0_BLACK :
                IDR_STATUSBAR_NETWORK_BARS0);
  }
  int index = static_cast<int>(wifi->strength() / 100.0 *
      nextafter(static_cast<float>(kNumBarsImages), 0));
  index = std::max(std::min(index, kNumBarsImages - 1), 0);
  const int* images = black ? kBarsImagesBlack : kBarsImages;
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(images[index]);
}

// static
const SkBitmap* NetworkMenu::IconForNetworkStrength(
    const CellularNetwork* cellular, bool black) {
  DCHECK(cellular);
  // If no data, then we show 0 bars.
  if (cellular->strength() == 0 ||
      cellular->data_left() == CellularNetwork::DATA_NONE) {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        black ? IDR_STATUSBAR_NETWORK_BARS0_BLACK :
                IDR_STATUSBAR_NETWORK_BARS0);
  }
  int index = static_cast<int>(cellular->strength() / 100.0 *
      nextafter(static_cast<float>(kNumBarsImages), 0));
  index = std::max(std::min(index, kNumBarsImages - 1), 0);
  const int* images = black ? kBarsImagesBlack : kBarsImages;
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(images[index]);
}

// static
const SkBitmap* NetworkMenu::IconForNetworkConnecting(double animation_value,
                                                      bool black) {
  // Fade bars a bit and show the different bar states.
  const int* source_image_ids = black ? kBarsImagesBlack : kBarsImages;
  SkBitmap* images = black ? kAnimatingImagesBlack : kAnimatingImages;
  int index = static_cast<int>(animation_value *
      nextafter(static_cast<float>(kNumBarsImages), 0));
  index = std::max(std::min(index, kNumBarsImages - 1), 0);

  // Lazily cache images.
  if (images[index].empty()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SkBitmap source = *rb.GetBitmapNamed(source_image_ids[index]);

    // Create an empty image to fade against.
    SkBitmap empty_image;
    empty_image.setConfig(SkBitmap::kARGB_8888_Config,
                          source.width(),
                          source.height(),
                          0);
    empty_image.allocPixels();
    empty_image.eraseARGB(0, 0, 0, 0);

    images[index] =
        SkBitmapOperations::CreateBlendedBitmap(
            empty_image,
            source,
            kConnectingImageAlpha);
  }
  return &images[index];
}

// static
const SkBitmap* NetworkMenu::BadgeForNetworkTechnology(
    const CellularNetwork* cellular) {
  if (!cellular)
    return NULL;

  int id = -1;
  switch (cellular->network_technology()) {
    case NETWORK_TECHNOLOGY_EVDO:
      switch (cellular->data_left()) {
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
      break;
    case NETWORK_TECHNOLOGY_1XRTT:
      switch (cellular->data_left()) {
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
      break;
      // Note: we may not be able to obtain data usage info
      // from GSM carriers, so there may not be a reason
      // to create _ERROR or _UNKNOWN versions of the following
      // icons.
    case NETWORK_TECHNOLOGY_GPRS:
      id = IDR_STATUSBAR_NETWORK_GPRS;
      break;
    case NETWORK_TECHNOLOGY_EDGE:
      id = IDR_STATUSBAR_NETWORK_EDGE;
      break;
    case NETWORK_TECHNOLOGY_UMTS:
      id = IDR_STATUSBAR_NETWORK_3G;
      break;
    case NETWORK_TECHNOLOGY_HSPA:
      id = IDR_STATUSBAR_NETWORK_HSPA;
      break;
    case NETWORK_TECHNOLOGY_HSPA_PLUS:
      id = IDR_STATUSBAR_NETWORK_HSPA_PLUS;
      break;
    case NETWORK_TECHNOLOGY_LTE:
      id = IDR_STATUSBAR_NETWORK_LTE;
      break;
    case NETWORK_TECHNOLOGY_LTE_ADVANCED:
      id = IDR_STATUSBAR_NETWORK_LTE_ADVANCED;
      break;
    case NETWORK_TECHNOLOGY_UNKNOWN:
      break;
  }
  if (id == -1)
    return NULL;
  else
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(id);
}

// static
const SkBitmap* NetworkMenu::BadgeForRoamingStatus(
    const CellularNetwork* cellular) {
  if (cellular->roaming_state() == ROAMING_STATE_ROAMING)
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_STATUSBAR_NETWORK_ROAMING);
  else
    return NULL;
}

const SkBitmap* NetworkMenu::BadgeForPrivateNetworkStatus(
      const Network* network) {
  // If network is not null, check if it's the active network with vpn on it.
  if (network) {
      NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
      if (!(cros->virtual_network() && network == cros->connected_network()))
        return NULL;
  }
  // TODO(kuan): yellow lock icon not ready yet; for now, return the black one
  // used by secure wifi network.
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUSBAR_NETWORK_SECURE);
}

// static
SkBitmap NetworkMenu::IconForDisplay(const SkBitmap* icon,
                                     const SkBitmap* badge) {
  return IconForDisplay(icon, badge, NULL, NULL);
}

// static
SkBitmap NetworkMenu::IconForDisplay(const SkBitmap* icon,
                                     const SkBitmap* bottom_right_badge,
                                     const SkBitmap* top_left_badge,
                                     const SkBitmap* bottom_left_badge) {
  DCHECK(icon);
  if (bottom_right_badge == NULL && top_left_badge == NULL &&
      bottom_left_badge == NULL)
    return *icon;

  static const int kTopLeftBadgeX = 0;
  static const int kTopLeftBadgeY = 0;
  static const int kBottomRightBadgeX = 14;
  static const int kBottomRightBadgeY = 14;
  static const int kBottomLeftBadgeX = 0;
  static const int kBottomLeftBadgeY = 14;

  gfx::CanvasSkia canvas(icon->width(), icon->height(), false);
  canvas.DrawBitmapInt(*icon, 0, 0);
  if (bottom_right_badge != NULL)
    canvas.DrawBitmapInt(*bottom_right_badge,
                         kBottomRightBadgeX,
                         kBottomRightBadgeY);
  if (top_left_badge != NULL)
    canvas.DrawBitmapInt(*top_left_badge, kTopLeftBadgeX, kTopLeftBadgeY);
  if (bottom_left_badge != NULL)
    canvas.DrawBitmapInt(*bottom_left_badge, kBottomLeftBadgeX,
                         kBottomLeftBadgeY);
  return canvas.ExtractBitmap();
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

////////////////////////////////////////////////////////////////////////////////
// NetworkMenu, views::ViewMenuDelegate implementation:

void NetworkMenu::RunMenu(views::View* source, const gfx::Point& pt) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->RequestNetworkScan();

  UpdateMenu();

  // TODO(rhashimoto): Remove this workaround when WebUI provides a
  // top-level widget on the ChromeOS login screen that is a window.
  // The current BackgroundView class for the ChromeOS login screen
  // creates a owning Widget that has a native GtkWindow but is not a
  // Window.  This makes it impossible to get the NativeWindow via
  // the views API.  This workaround casts the top-level NativeWidget
  // to a NativeWindow that we can pass to MenuItemView::RunMenuAt().
  gfx::NativeWindow window = GTK_WINDOW(source->GetWidget()->GetNativeView());

  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(source, &screen_loc);
  gfx::Rect bounds(screen_loc, source->size());
  network_menu_->GetSubmenu()->set_minimum_preferred_width(min_width_);
  network_menu_->RunMenuAt(window, GetMenuButton(), bounds,
      views::MenuItemView::TOPRIGHT, true);
}

}  // namespace chromeos
