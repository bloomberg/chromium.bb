// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_state_list_detailed_view.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/chromeos/network/network_icon.h"
#include "ash/system/chromeos/network/network_icon_animation.h"
#include "ash/system/chromeos/network/tray_network_state_observer.h"
#include "ash/system/tray/fixed_sized_scroll_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_popup_header_button.h"
#include "ash/system/tray/tray_popup_label_button.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using chromeos::NetworkState;
using chromeos::NetworkStateHandler;

namespace ash {
namespace internal {
namespace tray {

namespace {

// Height of the list of networks in the popup.
const int kNetworkListHeight = 203;

// Create a label with the font size and color used in the network info bubble.
views::Label* CreateInfoBubbleLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  label->SetFont(rb.GetFont(ui::ResourceBundle::SmallFont));
  label->SetEnabledColor(SkColorSetARGB(127, 0, 0, 0));
  return label;
}

// Create a label formatted for info items in the menu
views::Label* CreateMenuInfoLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  label->set_border(views::Border::CreateEmptyBorder(
      ash::kTrayPopupPaddingBetweenItems,
      ash::kTrayPopupPaddingHorizontal,
      ash::kTrayPopupPaddingBetweenItems, 0));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SkColorSetARGB(192, 0, 0, 0));
  return label;
}

// Create a row of labels for the network info bubble.
views::View* CreateInfoBubbleLine(const string16& text_label,
                                  const std::string& text_string) {
  views::View* view = new views::View;
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 1));
  view->AddChildView(CreateInfoBubbleLabel(text_label));
  view->AddChildView(CreateInfoBubbleLabel(UTF8ToUTF16(": ")));
  view->AddChildView(CreateInfoBubbleLabel(UTF8ToUTF16(text_string)));
  return view;
}

// A bubble that cannot be activated.
class NonActivatableSettingsBubble : public views::BubbleDelegateView {
 public:
  NonActivatableSettingsBubble(views::View* anchor, views::View* content)
      : views::BubbleDelegateView(anchor, views::BubbleBorder::TOP_RIGHT) {
    set_use_focusless(true);
    set_parent_window(ash::Shell::GetContainer(
        anchor->GetWidget()->GetNativeWindow()->GetRootWindow(),
        ash::internal::kShellWindowId_SettingBubbleContainer));
    SetLayoutManager(new views::FillLayout());
    AddChildView(content);
  }

  virtual ~NonActivatableSettingsBubble() {}

  virtual bool CanActivate() const OVERRIDE { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NonActivatableSettingsBubble);
};

}  // namespace


//------------------------------------------------------------------------------

struct NetworkInfo {
  NetworkInfo(const std::string& path)
      : service_path(path),
        disable(false),
        highlight(false) {
  }

  std::string service_path;
  string16 label;
  gfx::ImageSkia image;
  bool disable;
  bool highlight;
};

//------------------------------------------------------------------------------
// NetworkStateListDetailedView

NetworkStateListDetailedView::NetworkStateListDetailedView(
    SystemTrayItem* owner,
    ListType list_type,
    user::LoginStatus login)
    : NetworkDetailedView(owner),
      list_type_(list_type),
      login_(login),
      info_icon_(NULL),
      button_wifi_(NULL),
      button_mobile_(NULL),
      view_mobile_account_(NULL),
      setup_mobile_account_(NULL),
      other_wifi_(NULL),
      turn_on_wifi_(NULL),
      other_mobile_(NULL),
      other_vpn_(NULL),
      settings_(NULL),
      proxy_settings_(NULL),
      scanning_view_(NULL),
      no_wifi_networks_view_(NULL),
      no_cellular_networks_view_(NULL),
      info_bubble_(NULL) {
  network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
}

NetworkStateListDetailedView::~NetworkStateListDetailedView() {
  if (info_bubble_)
    info_bubble_->GetWidget()->CloseNow();
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkStateListDetailedView::ManagerChanged() {
  UpdateNetworkList();
  UpdateHeaderButtons();
  UpdateMobileAccount();
  UpdateNetworkExtra();
  Layout();
}

void NetworkStateListDetailedView::NetworkListChanged() {
  NetworkStateList network_list;
  NetworkStateHandler::Get()->GetNetworkList(&network_list);
  UpdateNetworks(network_list);
  UpdateNetworkList();
  UpdateHeaderButtons();
  UpdateMobileAccount();
  UpdateNetworkExtra();
  Layout();
}

void NetworkStateListDetailedView::NetworkServiceChanged(
    const NetworkState* network) {
  UpdateNetworkList();
  Layout();
}

void NetworkStateListDetailedView::NetworkIconChanged() {
  UpdateNetworkList();
  Layout();
}

// Overridden from NetworkDetailedView:

void NetworkStateListDetailedView::Init() {
  CreateScrollableList();
  CreateNetworkExtra();
  CreateHeaderEntry();
  CreateHeaderButtons();
  CreateMobileAccount();
  NetworkStateHandler* handler = NetworkStateHandler::Get();
  NetworkStateList network_list;
  handler->RequestScan();
  handler->GetNetworkList(&network_list);
  UpdateNetworks(network_list);
  UpdateNetworkList();
  UpdateHeaderButtons();
  UpdateMobileAccount();
  UpdateNetworkExtra();
}

NetworkDetailedView::DetailedViewType
NetworkStateListDetailedView::GetViewType() const {
  return STATE_LIST_VIEW;
}

// Views overrides

void NetworkStateListDetailedView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender == info_icon_) {
    ToggleInfoBubble();
    return;
  }

  // If the info bubble was visible, close it when some other item is clicked.
  ResetInfoBubble();

  NetworkStateHandler* handler = NetworkStateHandler::Get();
  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
  if (sender == button_wifi_) {
    bool enabled = handler->TechnologyEnabled(flimflam::kTypeWifi);
    handler->SetTechnologyEnabled(
        flimflam::kTypeWifi, !enabled,
        chromeos::network_handler::ErrorCallback());
  } else if (sender == turn_on_wifi_) {
    handler->SetTechnologyEnabled(
        flimflam::kTypeWifi, true,
        chromeos::network_handler::ErrorCallback());
  } else if (sender == button_mobile_) {
    // TODO: This needs to be fixed to use
    // NetworkStateHandler::SetTechnologyEnabled instead. Currently
    // ToggleMobile has code to handle the locked SIM case, which cannot
    // be moved here yet due to dependencies on src/chrome/* - see,
    // crbug.com/222540.
    delegate->ToggleMobile();
  } else if (sender == settings_) {
    delegate->ShowNetworkSettings();
  } else if (sender == proxy_settings_) {
    delegate->ChangeProxySettings();
  } else if (sender == other_mobile_) {
    delegate->ShowOtherCellular();
  } else if (sender == other_wifi_) {
    delegate->ShowOtherWifi();
  } else if (sender == other_vpn_) {
    delegate->ShowOtherVPN();
  } else {
    NOTREACHED();
  }
}

void NetworkStateListDetailedView::OnViewClicked(views::View* sender) {
  // If the info bubble was visible, close it when some other item is clicked.
  ResetInfoBubble();

  if (sender == footer()->content()) {
    RootWindowController::ForWindow(GetWidget()->GetNativeView())->
        GetSystemTray()->ShowDefaultView(BUBBLE_USE_EXISTING);
    return;
  }

  if (login_ == user::LOGGED_IN_LOCKED)
    return;

  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
  if (sender == view_mobile_account_) {
    delegate->ShowCellularURL(topup_url_);
  } else if (sender == setup_mobile_account_) {
    delegate->ShowCellularURL(setup_url_);
  } else {
    std::map<views::View*, std::string>::iterator found =
        network_map_.find(sender);
    if (found != network_map_.end())
      ConnectToNetwork(found->second);
  }
}

// Create UI components.

void NetworkStateListDetailedView::CreateHeaderEntry() {
  CreateSpecialRow(IDS_ASH_STATUS_TRAY_NETWORK, this);
}

void NetworkStateListDetailedView::CreateHeaderButtons() {
  if (list_type_ == LIST_TYPE_NETWORK) {
    button_wifi_ = new TrayPopupHeaderButton(
        this,
        IDR_AURA_UBER_TRAY_WIFI_ENABLED,
        IDR_AURA_UBER_TRAY_WIFI_DISABLED,
        IDR_AURA_UBER_TRAY_WIFI_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_WIFI_DISABLED_HOVER,
        IDS_ASH_STATUS_TRAY_WIFI);
    button_wifi_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_WIFI));
    button_wifi_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_WIFI));
    footer()->AddButton(button_wifi_);

    button_mobile_ = new TrayPopupHeaderButton(
        this,
        IDR_AURA_UBER_TRAY_CELLULAR_ENABLED,
        IDR_AURA_UBER_TRAY_CELLULAR_DISABLED,
        IDR_AURA_UBER_TRAY_CELLULAR_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_CELLULAR_DISABLED_HOVER,
        IDS_ASH_STATUS_TRAY_CELLULAR);
    button_mobile_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_MOBILE));
    button_mobile_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_MOBILE));
    footer()->AddButton(button_mobile_);
  }

  info_icon_ = new TrayPopupHeaderButton(
      this,
      IDR_AURA_UBER_TRAY_NETWORK_INFO,
      IDR_AURA_UBER_TRAY_NETWORK_INFO,
      IDR_AURA_UBER_TRAY_NETWORK_INFO_HOVER,
      IDR_AURA_UBER_TRAY_NETWORK_INFO_HOVER,
      IDS_ASH_STATUS_TRAY_NETWORK_INFO);
  info_icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_INFO));
  footer()->AddButton(info_icon_);
}

void NetworkStateListDetailedView::CreateMobileAccount() {
  if (list_type_ != LIST_TYPE_NETWORK)
    return;

  HoverHighlightView* container = new HoverHighlightView(this);
  container->AddLabel(
      ui::ResourceBundle::GetSharedInstance().
      GetLocalizedString(IDS_ASH_STATUS_TRAY_MOBILE_VIEW_ACCOUNT),
      gfx::Font::NORMAL);
  AddChildView(container);
  view_mobile_account_ = container;

  container = new HoverHighlightView(this);
  container->AddLabel(ui::ResourceBundle::GetSharedInstance().
                      GetLocalizedString(IDS_ASH_STATUS_TRAY_SETUP_MOBILE),
                      gfx::Font::NORMAL);
  AddChildView(container);
  setup_mobile_account_ = container;
}

void NetworkStateListDetailedView::CreateNetworkExtra() {
  if (login_ == user::LOGGED_IN_LOCKED)
    return;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::View* bottom_row = new views::View();
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal,
      kTrayMenuBottomRowPadding,
      kTrayMenuBottomRowPadding,
      kTrayMenuBottomRowPaddingBetweenItems);
  layout->set_spread_blank_space(true);
  bottom_row->SetLayoutManager(layout);

  if (list_type_ == LIST_TYPE_NETWORK) {
    other_wifi_ = new TrayPopupLabelButton(
        this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_WIFI));
    bottom_row->AddChildView(other_wifi_);

    turn_on_wifi_ = new TrayPopupLabelButton(
        this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_TURN_ON_WIFI));
    bottom_row->AddChildView(turn_on_wifi_);

    other_mobile_ = new TrayPopupLabelButton(
        this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_MOBILE));
    bottom_row->AddChildView(other_mobile_);
  } else if (list_type_ == LIST_TYPE_VPN) {
    other_vpn_ = new TrayPopupLabelButton(
        this,
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_OTHER_VPN));
    bottom_row->AddChildView(other_vpn_);
  }

  CreateSettingsEntry();
  DCHECK(settings_ || proxy_settings_);
  bottom_row->AddChildView(settings_ ? settings_ : proxy_settings_);

  AddChildView(bottom_row);
}

// Update UI components.

void NetworkStateListDetailedView::UpdateHeaderButtons() {
  NetworkStateHandler* handler = NetworkStateHandler::Get();
  if (button_wifi_) {
    button_wifi_->SetToggled(
        !handler->TechnologyEnabled(flimflam::kTypeWifi));
  }
  if (button_mobile_) {
    button_mobile_->SetToggled(!handler->TechnologyEnabled(
        NetworkStateHandler::kMatchTypeMobile));
    button_mobile_->SetVisible(handler->TechnologyAvailable(
        NetworkStateHandler::kMatchTypeMobile));
  }
  if (proxy_settings_)
    proxy_settings_->SetEnabled(handler->DefaultNetwork() != NULL);

  static_cast<views::View*>(footer())->Layout();
}

void NetworkStateListDetailedView::UpdateNetworks(
    const NetworkStateList& networks) {
  network_list_.clear();
  for (NetworkStateList::const_iterator iter = networks.begin();
       iter != networks.end(); ++iter) {
    const NetworkState* network = *iter;
    if ((list_type_ == LIST_TYPE_NETWORK &&
        network->type() != flimflam::kTypeVPN) ||
        (list_type_ == LIST_TYPE_VPN &&
         network->type() == flimflam::kTypeVPN)) {
      NetworkInfo* info = new NetworkInfo(network->path());
      network_list_.push_back(info);
    }
  }
}

void NetworkStateListDetailedView::UpdateNetworkList() {
  NetworkStateHandler* handler = NetworkStateHandler::Get();

  // First, update state for all networks
  for (size_t i = 0; i < network_list_.size(); ++i) {
    NetworkInfo* info = network_list_[i];
    const NetworkState* network =
        handler->GetNetworkState(info->service_path);
    if (!network)
      continue;
    info->image = network_icon::GetImageForNetwork(
        network, network_icon::ICON_TYPE_LIST);
    info->label = network_icon::GetLabelForNetwork(
        network, network_icon::ICON_TYPE_LIST);
    info->highlight =
        network->IsConnectedState() || network->IsConnectingState();
    info->disable =
        network->activation_state() == flimflam::kActivationStateActivating;
  }

  // Get the updated list entries
  network_map_.clear();
  std::set<std::string> new_service_paths;
  bool needs_relayout = UpdateNetworkListEntries(&new_service_paths);

  // Remove old children
  std::set<std::string> remove_service_paths;
  for (ServicePathMap::const_iterator it = service_path_map_.begin();
       it != service_path_map_.end(); ++it) {
    if (new_service_paths.find(it->first) == new_service_paths.end()) {
      remove_service_paths.insert(it->first);
      network_map_.erase(it->second);
      scroll_content()->RemoveChildView(it->second);
      needs_relayout = true;
    }
  }

  for (std::set<std::string>::const_iterator remove_it =
           remove_service_paths.begin();
       remove_it != remove_service_paths.end(); ++remove_it) {
    service_path_map_.erase(*remove_it);
  }

  if (needs_relayout) {
    views::View* selected_view = NULL;
    for (ServicePathMap::const_iterator iter = service_path_map_.begin();
         iter != service_path_map_.end(); ++iter) {
      if (iter->second->hover()) {
        selected_view = iter->second;
        break;
      }
    }
    scroll_content()->SizeToPreferredSize();
    static_cast<views::View*>(scroller())->Layout();
    if (selected_view)
      scroll_content()->ScrollRectToVisible(selected_view->bounds());
  }
}

bool NetworkStateListDetailedView::CreateOrUpdateInfoLabel(
    int index, const string16& text, views::Label** label) {
  if (*label == NULL) {
    *label = CreateMenuInfoLabel(text);
    scroll_content()->AddChildViewAt(*label, index);
    return true;
  } else {
    (*label)->SetText(text);
    return OrderChild(*label, index);
  }
}

bool NetworkStateListDetailedView::UpdateNetworkChild(
    int index, const NetworkInfo* info) {
  bool needs_relayout = false;
  HoverHighlightView* container = NULL;
  ServicePathMap::const_iterator found =
      service_path_map_.find(info->service_path);
  gfx::Font::FontStyle font =
      info->highlight ? gfx::Font::BOLD : gfx::Font::NORMAL;
  if (found == service_path_map_.end()) {
    container = new HoverHighlightView(this);
    container->AddIconAndLabel(info->image, info->label, font);
    scroll_content()->AddChildViewAt(container, index);
    container->set_border(views::Border::CreateEmptyBorder(
        0, kTrayPopupPaddingHorizontal, 0, 0));
    needs_relayout = true;
  } else {
    container = found->second;
    container->RemoveAllChildViews(true);
    container->AddIconAndLabel(info->image, info->label, font);
    container->Layout();
    container->SchedulePaint();
    needs_relayout = OrderChild(container, index);
  }
  if (info->disable)
    container->SetEnabled(false);
  network_map_[container] = info->service_path;
  service_path_map_[info->service_path] = container;
  return needs_relayout;
}

bool NetworkStateListDetailedView::OrderChild(views::View* view, int index) {
  if (scroll_content()->child_at(index) != view) {
    scroll_content()->ReorderChildView(view, index);
    return true;
  }
  return false;
}

bool NetworkStateListDetailedView::UpdateNetworkListEntries(
    std::set<std::string>* new_service_paths) {
  bool needs_relayout = false;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NetworkStateHandler* handler = NetworkStateHandler::Get();

  // Insert child views
  int index = 0;

  // Highlighted networks
  for (size_t i = 0; i < network_list_.size(); ++i) {
    const NetworkInfo* info = network_list_[i];
    if (info->highlight) {
      if (UpdateNetworkChild(index++, info))
        needs_relayout = true;
      new_service_paths->insert(info->service_path);
    }
  }

  if (list_type_ == LIST_TYPE_NETWORK) {
    // Cellular initializing
    int status_message_id = network_icon::GetCellularUninitializedMsg();
    if (!status_message_id &&
        handler->TechnologyEnabled(NetworkStateHandler::kMatchTypeMobile) &&
        !handler->FirstNetworkByType(NetworkStateHandler::kMatchTypeMobile)) {
      status_message_id = IDS_ASH_STATUS_TRAY_NO_CELLULAR_NETWORKS;
    }
    if (status_message_id) {
      string16 text = rb.GetLocalizedString(status_message_id);
      if (CreateOrUpdateInfoLabel(index++, text, &no_cellular_networks_view_))
        needs_relayout = true;
    } else if (no_cellular_networks_view_) {
      scroll_content()->RemoveChildView(no_cellular_networks_view_);
      no_cellular_networks_view_ = NULL;
      needs_relayout = true;
    }

    // "Wifi Enabled / Disabled"
    if (network_list_.empty()) {
      int message_id = handler->TechnologyEnabled(flimflam::kTypeWifi) ?
          IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED :
          IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
      string16 text = rb.GetLocalizedString(message_id);
      if (CreateOrUpdateInfoLabel(index++, text, &no_wifi_networks_view_))
        needs_relayout = true;
    } else if (no_wifi_networks_view_) {
      scroll_content()->RemoveChildView(no_wifi_networks_view_);
      no_wifi_networks_view_ = NULL;
      needs_relayout = true;
    }

    // "Wifi Scanning"
    if (handler->GetScanningByType(flimflam::kTypeWifi)) {
      string16 text =
          rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_WIFI_SCANNING_MESSAGE);
      if (CreateOrUpdateInfoLabel(index++, text, &scanning_view_))
        needs_relayout = true;
    } else if (scanning_view_ != NULL) {
      scroll_content()->RemoveChildView(scanning_view_);
      scanning_view_ = NULL;
      needs_relayout = true;
    }
  }

  // Un-highlighted networks
  for (size_t i = 0; i < network_list_.size(); ++i) {
    const NetworkInfo* info = network_list_[i];
    if (!info->highlight) {
      if (UpdateNetworkChild(index++, info))
        needs_relayout = true;
      new_service_paths->insert(info->service_path);
    }
  }

  // No networks or other messages (fallback)
  if (index == 0) {
    string16 text = rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NO_NETWORKS);
    if (CreateOrUpdateInfoLabel(index++, text, &scanning_view_))
      needs_relayout = true;
  }

  return needs_relayout;
}

void NetworkStateListDetailedView::UpdateMobileAccount() {
  if (list_type_ != LIST_TYPE_NETWORK)
    return;

  view_mobile_account_->SetVisible(false);
  setup_mobile_account_->SetVisible(false);

  if (login_ == user::LOGGED_IN_NONE)
    return;

  // TODO(stevenjb): Migrate this code to src/chromeos.
  std::string carrier_id, topup_url, setup_url;
  if (Shell::GetInstance()->system_tray_delegate()->
      GetCellularCarrierInfo(&carrier_id, &topup_url, &setup_url)) {
    if (carrier_id != carrier_id_) {
      carrier_id_ = carrier_id;
      if (!topup_url.empty())
        topup_url_ = topup_url;
    }

    if (!setup_url.empty())
      setup_url_ = setup_url;

    if (!topup_url_.empty())
      view_mobile_account_->SetVisible(true);
    if (!setup_url_.empty())
      setup_mobile_account_->SetVisible(true);
  }
}

void NetworkStateListDetailedView::UpdateNetworkExtra() {
  if (login_ == user::LOGGED_IN_LOCKED)
    return;

  View* layout_parent = NULL;  // All these buttons have the same parent.
  NetworkStateHandler* handler = NetworkStateHandler::Get();
  if (other_wifi_) {
    DCHECK(turn_on_wifi_);
    if (!handler->TechnologyAvailable(flimflam::kTypeWifi)) {
      turn_on_wifi_->SetVisible(false);
      other_wifi_->SetVisible(false);
    } else if (!handler->TechnologyEnabled(flimflam::kTypeWifi)) {
      turn_on_wifi_->SetVisible(true);
      other_wifi_->SetVisible(false);
    } else {
      turn_on_wifi_->SetVisible(false);
      other_wifi_->SetVisible(true);
    }
    layout_parent = other_wifi_->parent();
  }

  if (other_mobile_) {
    bool show_other_mobile = false;
    if (handler->TechnologyAvailable(NetworkStateHandler::kMatchTypeMobile)) {
      const chromeos::DeviceState* device =
          handler->GetDeviceStateByType(NetworkStateHandler::kMatchTypeMobile);
      show_other_mobile = (device && device->support_network_scan());
    }
    if (show_other_mobile) {
      other_mobile_->SetVisible(true);
      other_mobile_->SetEnabled(
          handler->TechnologyEnabled(NetworkStateHandler::kMatchTypeMobile));
    } else {
      other_mobile_->SetVisible(false);
    }
    if (!layout_parent)
      layout_parent = other_wifi_->parent();
  }

  if (layout_parent)
    layout_parent->Layout();
}

void NetworkStateListDetailedView::CreateSettingsEntry() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (login_ != user::LOGGED_IN_NONE) {
    // Settings, only if logged in.
    settings_ = new TrayPopupLabelButton(
        this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS));
  } else {
    proxy_settings_ = new TrayPopupLabelButton(
        this,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_PROXY_SETTINGS));
  }
}

void NetworkStateListDetailedView::ToggleInfoBubble() {
  if (ResetInfoBubble())
    return;

  info_bubble_ = new NonActivatableSettingsBubble(
      info_icon_, CreateNetworkInfoView());
  views::BubbleDelegateView::CreateBubble(info_bubble_)->Show();
}

bool NetworkStateListDetailedView::ResetInfoBubble() {
  if (!info_bubble_)
    return false;
  info_bubble_->GetWidget()->Close();
  info_bubble_ = NULL;
  return true;
}

views::View* NetworkStateListDetailedView::CreateNetworkInfoView() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  NetworkStateHandler* handler = NetworkStateHandler::Get();

  std::string ip_address("0.0.0.0");
  const NetworkState* network = handler->DefaultNetwork();
  if (network)
    ip_address = network->ip_address();

  views::View* container = new views::View;
  container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
  container->set_border(views::Border::CreateEmptyBorder(0, 5, 0, 5));

  std::string ethernet_address, wifi_address, vpn_address;
  if (list_type_ == LIST_TYPE_NETWORK) {
    ethernet_address =
        handler->FormattedHardwareAddressForType(flimflam::kTypeEthernet);
    wifi_address =
        handler->FormattedHardwareAddressForType(flimflam::kTypeWifi);
  } else if (list_type_ == LIST_TYPE_VPN) {
    vpn_address =
        handler->FormattedHardwareAddressForType(flimflam::kTypeVPN);
  }

  // GetNetworkAddresses returns empty strings if no information is available.
  if (!ip_address.empty()) {
    container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_IP), ip_address));
  }
  if (!ethernet_address.empty()) {
    container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_ETHERNET), ethernet_address));
  }
  if (!wifi_address.empty()) {
    container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_WIFI), wifi_address));
  }
  if (!vpn_address.empty()) {
    container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_VPN), vpn_address));
  }

  // Avoid an empty bubble in the unlikely event that there is no network
  // information at all.
  if (!container->has_children()) {
    container->AddChildView(CreateInfoBubbleLabel(bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_NO_NETWORKS)));
  }

  return container;
}

void NetworkStateListDetailedView::ConnectToNetwork(
    const std::string& service_path) {
  NetworkStateHandler* handler = NetworkStateHandler::Get();
  const NetworkState* network = handler->GetNetworkState(service_path);
  if (!network)
    return;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kUseNewNetworkConfigurationHandlers) &&
      !network->IsConnectedState()) {
    handler->set_connecting_network(service_path);
    chromeos::NetworkConfigurationHandler::Get()->Connect(
        service_path,
        base::Bind(&base::DoNothing),
        chromeos::network_handler::ErrorCallback());
  } else {
    // This will show the settings UI for a connected network.
    // TODO(stevenjb): Change the API to explicitly show network settings.
    Shell::GetInstance()->system_tray_delegate()->ConnectToNetwork(
        service_path);
  }
}


}  // namespace tray
}  // namespace internal
}  // namespace ash
