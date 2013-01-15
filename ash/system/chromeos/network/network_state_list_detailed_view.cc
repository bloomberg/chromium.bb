// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_state_list_detailed_view.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/chromeos/network/network_icon.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
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

namespace ash {
namespace internal {
namespace tray {

namespace {

// Height of the list of networks in the popup.
const int kNetworkListHeight = 203;

// Create a label with the font size and color used in the network info bubble.
views::Label* CreateInfoBubbleLabel(const string16& text) {
  const SkColor text_color = SkColorSetARGB(127, 0, 0, 0);
  views::Label* label = new views::Label(text);
  label->SetFont(label->font().DeriveFont(-1));
  label->SetEnabledColor(text_color);
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
  NetworkInfo(const std::string& path, const string16& description)
      : service_path(path),
        description(description),
        highlight(false) {
  }

  std::string service_path;
  string16 description;
  gfx::ImageSkia image;
  bool highlight;
};

//------------------------------------------------------------------------------
// NetworkStateListDetailedView

NetworkStateListDetailedView::NetworkStateListDetailedView(
    SystemTrayItem* owner, user::LoginStatus login)
    : NetworkDetailedView(owner),
      login_(login),
      wifi_enabled_(false),
      wifi_scanning_(false),
      info_icon_(NULL),
      button_wifi_(NULL),
      button_mobile_(NULL),
      view_mobile_account_(NULL),
      setup_mobile_account_(NULL),
      other_wifi_(NULL),
      turn_on_wifi_(NULL),
      other_mobile_(NULL),
      settings_(NULL),
      proxy_settings_(NULL),
      scanning_view_(NULL),
      info_bubble_(NULL) {
}

NetworkStateListDetailedView::~NetworkStateListDetailedView() {
  if (info_bubble_)
    info_bubble_->GetWidget()->CloseNow();
}

void NetworkStateListDetailedView::ManagerChanged() {
  chromeos::NetworkStateHandler* handler = chromeos::NetworkStateHandler::Get();
  bool wifi_was_enabled = wifi_enabled_;
  wifi_enabled_ = handler->TechnologyEnabled(flimflam::kTypeWifi);
  if (wifi_enabled_ != wifi_was_enabled) {
    bool wifi_was_scanning = wifi_scanning_;
    if (wifi_enabled_ && !wifi_scanning_)
      wifi_scanning_ = handler->RequestWifiScan();
    else if (!wifi_enabled_ && wifi_scanning_)
      wifi_scanning_ = false;
    if (wifi_scanning_ != wifi_was_scanning)
      RefreshNetworkList();
  }
  UpdateHeaderButtons();
  UpdateNetworkEntries();
  UpdateNetworkExtra();
  Layout();
}

void NetworkStateListDetailedView::NetworkListChanged(
    const NetworkStateList& network_list) {
  wifi_scanning_ = false;
  UpdateNetworks(network_list);
  Layout();
}

void NetworkStateListDetailedView::NetworkServiceChanged(
    const chromeos::NetworkState* network) {
  RefreshNetworkList();
  Layout();
}

void NetworkStateListDetailedView::NetworkIconChanged() {
  UpdateNetworkIcons();
  RefreshNetworkList();
  Layout();
}

// Overridden from NetworkDetailedView:

void NetworkStateListDetailedView::Init() {
  CreateNetworkEntries();
  CreateNetworkExtra();
  CreateHeaderEntry();
  CreateHeaderButtons();
  chromeos::NetworkStateHandler* handler = chromeos::NetworkStateHandler::Get();
  NetworkStateList network_list;
  wifi_enabled_ = handler->TechnologyEnabled(flimflam::kTypeWifi);
  if (wifi_enabled_)
    wifi_scanning_ = handler->RequestWifiScan();
  handler->GetNetworkList(&network_list);
  UpdateNetworks(network_list);
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

  chromeos::NetworkStateHandler* handler = chromeos::NetworkStateHandler::Get();
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
    bool enabled = handler->TechnologyEnabled(flimflam::kTypeCellular);
    handler->SetTechnologyEnabled(
        flimflam::kTypeCellular, !enabled,
        chromeos::network_handler::ErrorCallback());
  } else if (sender == settings_) {
    delegate->ShowNetworkSettings();
  } else if (sender == proxy_settings_) {
    delegate->ChangeProxySettings();
  } else if (sender == other_mobile_) {
    delegate->ShowOtherCellular();
  } else if (sender == other_wifi_) {
    delegate->ShowOtherWifi();
  } else {
    NOTREACHED();
  }
}

void NetworkStateListDetailedView::ClickedOn(views::View* sender) {
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
    if (found != network_map_.end()) {
      std::string service_path = found->second;
      const chromeos::NetworkState* network =
          chromeos::NetworkStateHandler::Get()->GetNetworkState(service_path);
      if (!network)
        return;
      if (!network->IsConnectedState()) {
        chromeos::NetworkConfigurationHandler::Get()->Connect(
            service_path,
            base::Closure(),
            chromeos::network_handler::ErrorCallback());
      } else {
        // This will show the settings UI for a connected network.
        // TODO(stevenjb): Change the API to explicitly show network settings.
        delegate->ConnectToNetwork(service_path);
      }
    }
  }
}

// Create UI components.

void NetworkStateListDetailedView::CreateHeaderEntry() {
  CreateSpecialRow(IDS_ASH_STATUS_TRAY_NETWORK, this);
}

void NetworkStateListDetailedView::CreateHeaderButtons() {
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

void NetworkStateListDetailedView::CreateNetworkEntries() {
  CreateScrollableList();

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

  other_wifi_ = new TrayPopupLabelButton(
      this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_WIFI));
  bottom_row->AddChildView(other_wifi_);

  turn_on_wifi_ = new TrayPopupLabelButton(
      this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_TURN_ON_WIFI));
  bottom_row->AddChildView(turn_on_wifi_);

  other_mobile_ = new TrayPopupLabelButton(
      this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_MOBILE));
  bottom_row->AddChildView(other_mobile_);

  CreateSettingsEntry();
  DCHECK(settings_ || proxy_settings_);
  bottom_row->AddChildView(settings_ ? settings_ : proxy_settings_);

  AddChildView(bottom_row);
}

// Update UI components.

void NetworkStateListDetailedView::UpdateHeaderButtons() {
  chromeos::NetworkStateHandler* handler = chromeos::NetworkStateHandler::Get();
  button_wifi_->SetToggled(
      !handler->TechnologyEnabled(flimflam::kTypeWifi));
  button_mobile_->SetToggled(
      !handler->TechnologyEnabled(flimflam::kTypeCellular));
  button_mobile_->SetVisible(
      handler->TechnologyAvailable(flimflam::kTypeCellular));
  if (proxy_settings_)
    proxy_settings_->SetEnabled(handler->DefaultNetwork() != NULL);
}

void NetworkStateListDetailedView::UpdateNetworks(
    const NetworkStateList& networks) {
  network_list_.clear();
  for (NetworkStateList::const_iterator iter = networks.begin();
       iter != networks.end(); ++iter) {
    const chromeos::NetworkState* network = *iter;
    if (network->type() == flimflam::kTypeVPN)
      continue;  // Skip VPNs
    string16 description;
    if (network->IsConnectingState()) {
      description = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING,
          UTF8ToUTF16(network->name()));
    } else {
      description = UTF8ToUTF16(network->name());
    }
    NetworkInfo* info = new NetworkInfo(network->path(), description);
    info->highlight =
        network->IsConnectedState() || network->IsConnectingState();
    info->image = network_icon::GetImageForNetwork(network,
                                                   network_icon::COLOR_DARK,
                                                   this);
    network_list_.push_back(info);
  }
  RefreshNetworkList();

  UpdateHeaderButtons();
  UpdateNetworkEntries();
  UpdateNetworkExtra();
}

void NetworkStateListDetailedView::RefreshNetworkList() {
  network_map_.clear();
  std::set<std::string> new_service_paths;

  bool needs_relayout = false;
  views::View* highlighted_view = NULL;

  if (service_path_map_.empty()) {
    scroll_content()->RemoveAllChildViews(true);
    scanning_view_ = NULL;
  }

  // Insert child views. Order is:
  // * Highlit networks (connected and connecting)
  // * "Scanning..."
  // * Un-highlit networks (not connected). Usually empty while scanning.

  if (wifi_scanning_ && scanning_view_ == NULL) {
    scanning_view_ = new views::Label(
        ui::ResourceBundle::GetSharedInstance().
        GetLocalizedString(IDS_ASH_STATUS_TRAY_WIFI_SCANNING_MESSAGE));
    scanning_view_->set_border(views::Border::CreateEmptyBorder(20, 0, 10, 0));
    scanning_view_->SetFont(
        scanning_view_->font().DeriveFont(0, gfx::Font::ITALIC));
    // Initially insert "scanning" first.
    scroll_content()->AddChildViewAt(scanning_view_, 0);
    needs_relayout = true;
  } else if (!wifi_scanning_ && scanning_view_ != NULL) {
    scroll_content()->RemoveChildView(scanning_view_);
    scanning_view_ = NULL;
    needs_relayout = true;
  }

  int child_index_offset = 0;
  for (size_t i = 0; i < network_list_.size(); ++i) {
    NetworkInfo* info = network_list_[i];
    if (scanning_view_ && child_index_offset == 0 && !info->highlight)
      child_index_offset = 1;
    // |child_index| determines the position of the view, which is the same
    // as the list index for highlit views, and offset by one for any
    // non-highlit views when scanning.
    int child_index = child_index_offset + i;
    std::map<std::string, HoverHighlightView*>::const_iterator it =
        service_path_map_.find(info->service_path);
    HoverHighlightView* container = NULL;
    std::string description;
    if (it == service_path_map_.end()) {
      // Create a new view.
      container = new HoverHighlightView(this);
      container->AddIconAndLabel(
          info->image,
          info->description,
          info->highlight ? gfx::Font::BOLD : gfx::Font::NORMAL);
      scroll_content()->AddChildViewAt(container, child_index);
      container->set_border(views::Border::CreateEmptyBorder(
          0, kTrayPopupPaddingHorizontal, 0, 0));
      needs_relayout = true;
    } else {
      container = it->second;
      container->RemoveAllChildViews(true);
      container->AddIconAndLabel(
          info->image,
          info->description,
          info->highlight ? gfx::Font::BOLD : gfx::Font::NORMAL);
      container->Layout();
      container->SchedulePaint();

      // Reordering the view if necessary.
      views::View* child = scroll_content()->child_at(child_index);
      if (child != container) {
        scroll_content()->ReorderChildView(container, child_index);
        needs_relayout = true;
      }
    }

    if (info->highlight)
      highlighted_view = container;
    network_map_[container] = info->service_path;
    service_path_map_[info->service_path] = container;
    new_service_paths.insert(info->service_path);
  }

  std::set<std::string> remove_service_paths;
  for (std::map<std::string, HoverHighlightView*>::const_iterator it =
           service_path_map_.begin(); it != service_path_map_.end(); ++it) {
    if (new_service_paths.find(it->first) == new_service_paths.end()) {
      remove_service_paths.insert(it->first);
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
    scroll_content()->SizeToPreferredSize();
    static_cast<views::View*>(scroller())->Layout();
    if (highlighted_view)
      scroll_content()->ScrollRectToVisible(highlighted_view->bounds());
  }
}

void NetworkStateListDetailedView::UpdateNetworkEntries() {
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

  chromeos::NetworkStateHandler* handler = chromeos::NetworkStateHandler::Get();
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
  turn_on_wifi_->parent()->Layout();

  bool show_other_mobile = false;
  if (handler->TechnologyAvailable(flimflam::kTypeCellular)) {
    const chromeos::DeviceState* device =
        handler->GetDeviceStateByType(flimflam::kTypeCellular);
    show_other_mobile = (device && device->support_network_scan());
  }
  if (show_other_mobile) {
    other_mobile_->SetVisible(true);
    other_mobile_->SetEnabled(
        handler->TechnologyEnabled(flimflam::kTypeCellular));
  } else {
    other_mobile_->SetVisible(false);
  }
}

void NetworkStateListDetailedView::UpdateNetworkIcons() {
  chromeos::NetworkStateHandler* handler = chromeos::NetworkStateHandler::Get();
  for (size_t i = 0; i < network_list_.size(); ++i) {
    NetworkInfo* info = network_list_[i];
    const chromeos::NetworkState* network =
        handler->GetNetworkState(info->service_path);
    if (!network)
      continue;
    info->image = network_icon::GetImageForNetwork(network,
                                                   network_icon::COLOR_DARK,
                                                   this);
    info->highlight =
        network->IsConnectedState() || network->IsConnectingState();
  }
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
  views::BubbleDelegateView::CreateBubble(info_bubble_);
  info_bubble_->Show();
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
  chromeos::NetworkStateHandler* handler = chromeos::NetworkStateHandler::Get();

  std::string ip_address("0.0.0.0");
  const chromeos::NetworkState* network = handler->DefaultNetwork();
  if (network)
    ip_address = network->ip_address();
  std::string ethernet_address =
      handler->FormattedHardwareAddressForType(flimflam::kTypeEthernet);
  std::string wifi_address =
      handler->FormattedHardwareAddressForType(flimflam::kTypeWifi);

  views::View* container = new views::View;
  container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
  container->set_border(views::Border::CreateEmptyBorder(0, 5, 0, 5));

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

  // Avoid an empty bubble in the unlikely event that there is no network
  // information at all.
  if (!container->has_children()) {
    container->AddChildView(CreateInfoBubbleLabel(bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_NO_NETWORKS)));
  }

  return container;
}

}  // namespace tray
}  // namespace internal
}  // namespace ash
