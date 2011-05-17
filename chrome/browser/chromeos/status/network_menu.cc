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
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/skbitmap_operations.h"
#include "views/controls/menu/menu_2.h"
#include "views/window/window.h"

namespace {

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

}  // namespace

namespace chromeos {

class MoreMenuModel : public NetworkMenuModel {
 public:
  explicit MoreMenuModel(NetworkMenu* owner);
  virtual ~MoreMenuModel() {}

  // NetworkMenuModel implementation.
  virtual void InitMenuItems(bool is_browser_mode,
                             bool should_open_button_options);

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
                             bool should_open_button_options);

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
                             bool should_open_button_options);

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

bool NetworkMenuModel::ConnectToNetworkAt(int index,
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
        return true;
      }
      if (wifi->IsPassphraseRequired()) {
        // Show the connection UI if we require a passphrase.
        ShowNetworkConfigView(new NetworkConfigView(wifi));
        return true;
      } else {
        cros->ConnectToWifiNetwork(wifi);
        // Connection failures are responsible for updating the UI, including
        // reopening dialogs.
        return true;
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
        return true;
      } else if (cellular->connecting_or_connected()) {
        // Cellular network is connecting or connected,
        // so we show the config settings for the cellular network.
        owner_->ShowTabbedNetworkSettings(cellular);
        return true;
      }
      // Clicked on a disconnected cellular network, so connect to it.
      cros->ConnectToCellularNetwork(cellular);
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
        return true;
      }
      // Show the connection UI if info for a field is missing.
      if (vpn->NeedMoreInfoToConnect()) {
        ShowNetworkConfigView(new NetworkConfigView(vpn));
        return true;
      }
      cros->ConnectToVirtualNetwork(vpn);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
      return true;
    } else {
      // If we are attempting to connect to a network that no longer exists,
      // display a notification.
      LOG(WARNING) << "VPN does not exist to connect to: " << service_path;
      // TODO(stevenjb): Show notification.
    }
  }
  return true;
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

int NetworkMenuModel::GetCommandIdAt(int index) const {
  return index;
}

string16 NetworkMenuModel::GetLabelAt(int index) const {
  return menu_items_[index].label;
}

bool NetworkMenuModel::IsItemDynamicAt(int index) const {
  return true;
}

const gfx::Font* NetworkMenuModel::GetLabelFontAt(int index) const {
  return (menu_items_[index].flags & FLAG_ASSOCIATED) ?
      &ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BoldFont) :
      NULL;
}

bool NetworkMenuModel::GetAcceleratorAt(
    int index, ui::Accelerator* accelerator) const {
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

ui::MenuModel* NetworkMenuModel::GetSubmenuModelAt(int index) const {
  return menu_items_[index].sub_menu_model;
}

void NetworkMenuModel::HighlightChangedTo(int index) {}

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

void NetworkMenuModel::MenuWillShow() {}

void NetworkMenuModel::SetMenuModelDelegate(ui::MenuModelDelegate* delegate) {}

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

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableVPN)) {
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
const int NetworkMenu::kNumAnimatingImages = 10;

// static
SkBitmap NetworkMenu::kAnimatingImages[kNumAnimatingImages];

// static
SkBitmap NetworkMenu::kAnimatingImagesBlack[kNumAnimatingImages];

NetworkMenu::NetworkMenu() : min_width_(-1) {
  main_menu_model_.reset(new MainMenuModel(this));
  network_menu_.reset(new views::Menu2(main_menu_model_.get()));
}

NetworkMenu::~NetworkMenu() {
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
  main_menu_model_->InitMenuItems(IsBrowserMode(), ShouldOpenButtonOptions());
  network_menu_->Rebuild();
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
  // Draw animation of bars icon fading in and out.
  // We are fading between 0 bars and a third of the opacity of 4 bars.
  // Use the current value of the animation to calculate the alpha value
  // of how transparent the icon is.
  int index = static_cast<int>(animation_value *
      nextafter(static_cast<float>(kNumAnimatingImages), 0));
  index = std::max(std::min(index, kNumAnimatingImages - 1), 0);

  SkBitmap* images = black ? kAnimatingImagesBlack : kAnimatingImages;
  // Lazily cache images.
  if (images[index].empty()) {
    // Divide index (0-9) by 9 (assume kNumAnimatingImages==10) to get (0.0-1.0)
    // Then we take a third of that for the alpha value.
    double alpha = (static_cast<double>(index) / (kNumAnimatingImages - 1)) / 3;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    images[index] = SkBitmapOperations::CreateBlendedBitmap(
        *rb.GetBitmapNamed(black ? IDR_STATUSBAR_NETWORK_BARS0_BLACK :
                                   IDR_STATUSBAR_NETWORK_BARS0),
        *rb.GetBitmapNamed(black ? IDR_STATUSBAR_NETWORK_BARS4_BLACK :
                                   IDR_STATUSBAR_NETWORK_BARS4),
        alpha);
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
  refreshing_menu_ = true;
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->RequestNetworkScan();

  // Build initial menu items. They will be updated when UpdateMenu is
  // called from NetworkChanged.
  main_menu_model_->InitMenuItems(IsBrowserMode(), ShouldOpenButtonOptions());
  network_menu_->Rebuild();

  // Restore menu width, if it was set up.
  // NOTE: width isn't checked for correctness here since all width-related
  // logic implemented inside |network_menu_|.
  if (min_width_ != -1)
    network_menu_->SetMinimumWidth(min_width_);
  refreshing_menu_ = false;
  network_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

}  // namespace chromeos


