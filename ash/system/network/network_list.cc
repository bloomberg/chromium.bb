// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list.h"

#include <memory>

#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_info.h"
#include "ash/system/network/network_state_list_detailed_view.h"
#include "ash/system/networking_config_delegate.h"

#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/throbber_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"

#include "ui/views/view.h"

using chromeos::LoginState;
using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::ManagedNetworkConfigurationHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {
namespace {

bool IsProhibitedByPolicy(const chromeos::NetworkState* network) {
  if (!NetworkTypePattern::WiFi().MatchesType(network->type()))
    return false;
  if (!LoginState::IsInitialized() || !LoginState::Get()->IsUserLoggedIn())
    return false;
  ManagedNetworkConfigurationHandler* managed_configuration_handler =
      NetworkHandler::Get()->managed_network_configuration_handler();
  const base::DictionaryValue* global_network_config =
      managed_configuration_handler->GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);
  bool policy_prohibites_unmanaged = false;
  if (global_network_config) {
    global_network_config->GetBooleanWithoutPathExpansion(
        ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
        &policy_prohibites_unmanaged);
  }
  if (!policy_prohibites_unmanaged)
    return false;
  return !managed_configuration_handler->FindPolicyByGuidAndProfile(
      network->guid(), network->profile_path(), nullptr /* onc_source */);
}

// TODO(varkha|mohsen): Consolidate with a similar method in
// BluetoothDetailedView (see https://crbug.com/686924).
void SetupConnectedItem(HoverHighlightView* container,
                        const base::string16& text,
                        const gfx::ImageSkia& image) {
  container->AddIconAndLabels(
      image, text,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::CAPTION);
  style.set_color_style(TrayPopupItemStyle::ColorStyle::CONNECTED);
  style.SetupLabel(container->sub_text_label());
}

// TODO(varkha|mohsen): Consolidate with a similar method in
// BluetoothDetailedView (see https://crbug.com/686924).
void SetupConnectingItem(HoverHighlightView* container,
                         const base::string16& text,
                         const gfx::ImageSkia& image) {
  container->AddIconAndLabels(
      image, text,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING));
  ThrobberView* throbber = new ThrobberView;
  throbber->Start();
  container->AddRightView(throbber);
}

}  // namespace

// A header row for sections in network detailed view which contains a title and
// a toggle button to turn on/off the section. Subclasses are given the
// opportunity to add extra buttons before the toggle button is added.
class NetworkListView::SectionHeaderRowView : public views::View,
                                              public views::ButtonListener {
 public:
  explicit SectionHeaderRowView(int title_id)
      : title_id_(title_id),
        container_(nullptr),
        toggle_(nullptr),
        style_(
            new TrayPopupItemStyle(TrayPopupItemStyle::FontStyle::SUB_HEADER)) {
  }

  ~SectionHeaderRowView() override {}

  void Init(bool enabled) {
    InitializeLayout();
    AddExtraButtons(enabled);
    AddToggleButton(enabled);
  }

  virtual void SetIsOn(bool enabled) {
    toggle_->SetEnabled(true);
    toggle_->SetIsOn(enabled, true);
  }

 protected:
  // This is called before the toggle button is added to give subclasses an
  // opportunity to add more buttons before the toggle button. Subclasses can
  // add buttons to container() using AddChildView().
  virtual void AddExtraButtons(bool enabled) {}

  // Called when |toggle_| is clicked and toggled. Subclasses can override to
  // enabled/disable their respective technology, for example.
  virtual void OnToggleToggled(bool is_on) = 0;

  TriView* container() const { return container_; }
  TrayPopupItemStyle* style() const { return style_.get(); }

  int GetHeightForWidth(int w) const override {
    // Make row height fixed avoiding layout manager adjustments.
    return GetPreferredSize().height();
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(toggle_, sender);
    // In the event of frequent clicks, helps to prevent a toggle button state
    // from becoming inconsistent with the async operation of enabling /
    // disabling of mobile radio. The toggle will get re-enabled in the next
    // call to NetworkListView::Update().
    toggle_->SetEnabled(false);
    OnToggleToggled(toggle_->is_on());
  }

 private:
  void InitializeLayout() {
    TrayPopupUtils::ConfigureAsStickyHeader(this);
    SetLayoutManager(new views::FillLayout);
    container_ = TrayPopupUtils::CreateSubHeaderRowView(false);
    AddChildView(container_);

    views::Label* label = TrayPopupUtils::CreateDefaultLabel();
    style()->SetupLabel(label);
    label->SetText(l10n_util::GetStringUTF16(title_id_));
    container_->AddView(TriView::Container::CENTER, label);
  }

  void AddToggleButton(bool enabled) {
    toggle_ = TrayPopupUtils::CreateToggleButton(this, title_id_);
    toggle_->SetIsOn(enabled, false);
    container_->AddView(TriView::Container::END, toggle_);
  }

  // Resource ID for the string to use as the title of the section and for the
  // accessible text on the section header toggle button.
  const int title_id_;

  // View containing header row views, including title, toggle, and extra
  // buttons.
  TriView* container_;

  // ToggleButton to toggle section on or off.
  views::ToggleButton* toggle_;

  // TrayPopupItemStyle used to configure labels and buttons.
  std::unique_ptr<TrayPopupItemStyle> style_;

  DISALLOW_COPY_AND_ASSIGN(SectionHeaderRowView);
};

namespace {

class CellularHeaderRowView : public NetworkListView::SectionHeaderRowView {
 public:
  CellularHeaderRowView()
      : SectionHeaderRowView(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE) {}

  ~CellularHeaderRowView() override {}

  const char* GetClassName() const override { return "CellularHeaderRowView"; }

 protected:
  void OnToggleToggled(bool is_on) override {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    handler->SetTechnologyEnabled(NetworkTypePattern::Cellular(), is_on,
                                  chromeos::network_handler::ErrorCallback());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CellularHeaderRowView);
};

class TetherHeaderRowView : public NetworkListView::SectionHeaderRowView {
 public:
  TetherHeaderRowView()
      : SectionHeaderRowView(IDS_ASH_STATUS_TRAY_NETWORK_TETHER) {}

  ~TetherHeaderRowView() override {}

  const char* GetClassName() const override { return "TetherHeaderRowView"; }

 protected:
  void OnToggleToggled(bool is_on) override {
    // TODO (hansberry): Persist toggle to settings/preferences.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherHeaderRowView);
};

class WifiHeaderRowView : public NetworkListView::SectionHeaderRowView {
 public:
  WifiHeaderRowView()
      : SectionHeaderRowView(IDS_ASH_STATUS_TRAY_NETWORK_WIFI),
        join_(nullptr) {}

  ~WifiHeaderRowView() override {}

  void SetIsOn(bool enabled) override {
    join_->SetEnabled(enabled);
    SectionHeaderRowView::SetIsOn(enabled);
  }

  const char* GetClassName() const override { return "WifiHeaderRowView"; }

 protected:
  // SectionHeaderRowView:
  void OnToggleToggled(bool is_on) override {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(), is_on,
                                  chromeos::network_handler::ErrorCallback());
  }

  void AddExtraButtons(bool enabled) override {
    const SkColor prominent_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
    gfx::ImageSkia normal_image = network_icon::GetImageForNewWifiNetwork(
        SkColorSetA(prominent_color, kJoinIconAlpha),
        SkColorSetA(prominent_color, kJoinBadgeAlpha));
    gfx::ImageSkia disabled_image = network_icon::GetImageForNewWifiNetwork(
        SkColorSetA(prominent_color, kDisabledJoinIconAlpha),
        SkColorSetA(prominent_color, kDisabledJoinBadgeAlpha));
    join_ = new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED,
                                 normal_image, disabled_image,
                                 IDS_ASH_STATUS_TRAY_OTHER_WIFI);
    join_->SetInkDropColor(prominent_color);
    join_->SetEnabled(enabled);
    container()->AddView(TriView::Container::END, join_);
  }

  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == join_) {
      ShellPort::Get()->RecordUserMetricsAction(
          UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED);
      Shell::Get()->system_tray_controller()->ShowNetworkCreate(
          shill::kTypeWifi);
      return;
    }
    SectionHeaderRowView::ButtonPressed(sender, event);
  }

 private:
  // Full opacity for badge.
  static constexpr int kJoinBadgeAlpha = 0xFF;

  // .30 opacity for icon.
  static constexpr int kJoinIconAlpha = 0x4D;

  // .38 opacity for disabled badge.
  static constexpr int kDisabledJoinBadgeAlpha = 0x61;

  // .30 * .38 opacity for disabled icon.
  static constexpr int kDisabledJoinIconAlpha = 0x1D;

  // A button to invoke "Join Wi-Fi network" dialog.
  SystemMenuButton* join_;

  DISALLOW_COPY_AND_ASSIGN(WifiHeaderRowView);
};

}  // namespace

// NetworkListView:

NetworkListView::NetworkListView(SystemTrayItem* owner, LoginStatus login)
    : NetworkStateListDetailedView(owner, LIST_TYPE_NETWORK, login),
      needs_relayout_(false),
      no_wifi_networks_view_(nullptr),
      no_cellular_networks_view_(nullptr),
      cellular_header_view_(nullptr),
      tether_header_view_(nullptr),
      wifi_header_view_(nullptr),
      cellular_separator_view_(nullptr),
      tether_separator_view_(nullptr),
      wifi_separator_view_(nullptr),
      connection_warning_(nullptr) {}

NetworkListView::~NetworkListView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::UpdateNetworkList() {
  CHECK(scroll_content());

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  NetworkStateHandler::NetworkStateList network_list;
  handler->GetVisibleNetworkList(&network_list);
  UpdateNetworks(network_list);

  // Add Tether networks.
  NetworkStateHandler::NetworkStateList tether_network_list;
  handler->GetVisibleNetworkListByType(NetworkTypePattern::Tether(),
                                       &tether_network_list);
  for (const auto* tether_network : tether_network_list) {
    network_list_.push_back(
        base::MakeUnique<NetworkInfo>(tether_network->guid()));
  }

  UpdateNetworkIcons();
  OrderNetworks();
  UpdateNetworkListInternal();
}

bool NetworkListView::IsNetworkEntry(views::View* view,
                                     std::string* guid) const {
  std::map<views::View*, std::string>::const_iterator found =
      network_map_.find(view);
  if (found == network_map_.end())
    return false;
  *guid = found->second;
  return true;
}

void NetworkListView::UpdateNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  SCOPED_NET_LOG_IF_SLOW();
  // |network_list_| contains all the info and is going to be cleared and
  // recreated. Save them to |last_network_info_map_|.
  last_network_info_map_.clear();
  for (auto& info : network_list_)
    last_network_info_map_[info->guid] = std::move(info);

  network_list_.clear();
  for (const auto* network : networks) {
    if (!NetworkTypePattern::NonVirtual().MatchesType(network->type()))
      continue;

    // Do not add Wi-Fi networks that are associated with a Tether network.
    if (NetworkTypePattern::WiFi().MatchesType(network->type()) &&
        !network->tether_guid().empty())
      continue;

    network_list_.push_back(base::MakeUnique<NetworkInfo>(network->guid()));
  }
}

void NetworkListView::OrderNetworks() {
  struct CompareNetwork {
    explicit CompareNetwork(NetworkStateHandler* handler) : handler_(handler) {}

    // Returns true if |network1| is less than (i.e. is ordered before)
    // |network2|.
    bool operator()(const std::unique_ptr<NetworkInfo>& network1,
                    const std::unique_ptr<NetworkInfo>& network2) {
      const int order1 =
          GetOrder(handler_->GetNetworkStateFromGuid(network1->guid));
      const int order2 =
          GetOrder(handler_->GetNetworkStateFromGuid(network2->guid));
      if (order1 != order2)
        return order1 < order2;
      if (network1->connected != network2->connected)
        return network1->connected;
      if (network1->connecting != network2->connecting)
        return network1->connecting;
      return network1->guid.compare(network2->guid) < 0;
    }

   private:
    static int GetOrder(const chromeos::NetworkState* network) {
      if (!network)
        return 999;
      if (network->Matches(NetworkTypePattern::Ethernet()))
        return 0;
      if (network->Matches(NetworkTypePattern::Cellular()))
        return 1;
      if (network->Matches(NetworkTypePattern::Mobile()))
        return 2;
      if (network->Matches(NetworkTypePattern::WiFi()))
        return 3;
      return 4;
    }

    NetworkStateHandler* handler_;
  };
  std::sort(network_list_.begin(), network_list_.end(),
            CompareNetwork(NetworkHandler::Get()->network_state_handler()));
}

void NetworkListView::UpdateNetworkIcons() {
  SCOPED_NET_LOG_IF_SLOW();
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // First, update state for all networks.
  bool animating = false;

  for (auto& info : network_list_) {
    const chromeos::NetworkState* network =
        handler->GetNetworkStateFromGuid(info->guid);
    if (!network)
      continue;
    bool prohibited_by_policy = IsProhibitedByPolicy(network);
    info->label = network_icon::GetLabelForNetwork(
        network, network_icon::ICON_TYPE_MENU_LIST);
    info->image =
        network_icon::GetImageForNetwork(network, network_icon::ICON_TYPE_LIST);
    info->disable =
        (network->activation_state() == shill::kActivationStateActivating) ||
        prohibited_by_policy;
    info->connected = network->IsConnectedState();
    info->connecting = network->IsConnectingState();
    if (network->Matches(NetworkTypePattern::WiFi()))
      info->type = NetworkInfo::Type::WIFI;
    else if (network->Matches(NetworkTypePattern::Cellular()))
      info->type = NetworkInfo::Type::CELLULAR;
    else if (network->Matches(NetworkTypePattern::Tether()))
      info->type = NetworkInfo::Type::TETHER;
    if (prohibited_by_policy) {
      info->tooltip =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_PROHIBITED);
    }
    if (!animating && network->IsConnectingState())
      animating = true;
  }
  if (animating)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::UpdateNetworkListInternal() {
  SCOPED_NET_LOG_IF_SLOW();
  // Get the updated list entries.
  needs_relayout_ = false;
  network_map_.clear();
  std::unique_ptr<std::set<std::string>> new_guids = UpdateNetworkListEntries();

  // Remove old children.
  std::set<std::string> remove_guids;
  for (const auto& iter : network_guid_map_) {
    if (new_guids->find(iter.first) == new_guids->end()) {
      remove_guids.insert(iter.first);
      network_map_.erase(iter.second);
      delete iter.second;
      needs_relayout_ = true;
    }
  }

  for (const auto& remove_iter : remove_guids)
    network_guid_map_.erase(remove_iter);

  if (!needs_relayout_)
    return;

  views::View* selected_view = nullptr;
  for (const auto& iter : network_guid_map_) {
    if (iter.second->IsMouseHovered()) {
      selected_view = iter.second;
      break;
    }
  }
  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
  if (selected_view)
    scroll_content()->ScrollRectToVisible(selected_view->bounds());
}

std::unique_ptr<std::set<std::string>>
NetworkListView::UpdateNetworkListEntries() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // Keep an index where the next child should be inserted.
  int index = 0;

  // Show a warning that the connection might be monitored if connected to a VPN
  // or if the default network has a proxy installed.
  const bool using_vpn =
      !!NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
          NetworkTypePattern::VPN());
  // TODO(jamescook): Eliminate the null check by creating UIProxyConfigService
  // under mash. This will require the mojo pref service to work with prefs in
  // Local State. http://crbug.com/718072
  const bool using_proxy = NetworkHandler::Get()->ui_proxy_config_service() &&
                           NetworkHandler::Get()
                               ->ui_proxy_config_service()
                               ->HasDefaultNetworkProxyConfigured();
  if (using_vpn || using_proxy) {
    if (!connection_warning_)
      connection_warning_ = CreateConnectionWarning();
    PlaceViewAtIndex(connection_warning_, index++);
  }

  // First add high-priority networks (not Wi-Fi nor cellular).
  std::unique_ptr<std::set<std::string>> new_guids =
      UpdateNetworkChildren(NetworkInfo::Type::UNKNOWN, index);
  index += new_guids->size();

  if (handler->IsTechnologyAvailable(NetworkTypePattern::Cellular())) {
    index = UpdateSectionHeaderRow(
        NetworkTypePattern::Cellular(),
        handler->IsTechnologyEnabled(NetworkTypePattern::Cellular()), index,
        &cellular_header_view_, &cellular_separator_view_);
  }

  // Cellular initializing.
  int cellular_message_id = network_icon::GetCellularUninitializedMsg();
  if (!cellular_message_id &&
      handler->IsTechnologyEnabled(NetworkTypePattern::Mobile()) &&
      !handler->FirstNetworkByType(NetworkTypePattern::Mobile())) {
    cellular_message_id = IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS;
  }
  UpdateInfoLabel(cellular_message_id, index, &no_cellular_networks_view_);
  if (cellular_message_id)
    ++index;

  // Add cellular networks.
  std::unique_ptr<std::set<std::string>> new_cellular_guids =
      UpdateNetworkChildren(NetworkInfo::Type::CELLULAR, index);
  index += new_cellular_guids->size();
  new_guids->insert(new_cellular_guids->begin(), new_cellular_guids->end());

  // TODO (hansberry): Audit existing usage of NonVirtual and consider changing
  // it to include Tether. See crbug.com/693647.
  if (handler->IsTechnologyAvailable(NetworkTypePattern::Tether())) {
    index = UpdateSectionHeaderRow(
        NetworkTypePattern::Tether(),
        handler->IsTechnologyEnabled(NetworkTypePattern::Tether()), index,
        &tether_header_view_, &tether_separator_view_);

    // TODO (hansberry): Should a message similar to
    // IDS_ASH_STATUS_TRAY_NO_CELLULAR_NETWORKS be shown if Tether technology is
    // enabled but no networks are around?

    // Add Tether networks.
    std::unique_ptr<std::set<std::string>> new_tether_guids =
        UpdateNetworkChildren(NetworkInfo::Type::TETHER, index);
    index += new_tether_guids->size();
    new_guids->insert(new_tether_guids->begin(), new_tether_guids->end());
  }

  index = UpdateSectionHeaderRow(
      NetworkTypePattern::WiFi(),
      handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()), index,
      &wifi_header_view_, &wifi_separator_view_);

  // "Wifi Enabled / Disabled".
  int wifi_message_id = 0;
  if (network_list_.empty()) {
    wifi_message_id = handler->IsTechnologyEnabled(NetworkTypePattern::WiFi())
                          ? IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED
                          : IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
  }
  UpdateInfoLabel(wifi_message_id, index, &no_wifi_networks_view_);
  if (wifi_message_id)
    ++index;

  // Add Wi-Fi networks.
  std::unique_ptr<std::set<std::string>> new_wifi_guids =
      UpdateNetworkChildren(NetworkInfo::Type::WIFI, index);
  index += new_wifi_guids->size();
  new_guids->insert(new_wifi_guids->begin(), new_wifi_guids->end());

  // No networks or other messages (fallback).
  if (index == 0) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NO_NETWORKS, index,
                    &no_wifi_networks_view_);
  }

  return new_guids;
}

HoverHighlightView* NetworkListView::CreateViewForNetwork(
    const NetworkInfo& info) {
  HoverHighlightView* container = new HoverHighlightView(this);
  if (info.connected)
    SetupConnectedItem(container, info.label, info.image);
  else if (info.connecting)
    SetupConnectingItem(container, info.label, info.image);
  else
    container->AddIconAndLabel(info.image, info.label);
  container->SetTooltipText(info.tooltip);
  views::View* controlled_icon = CreateControlledByExtensionView(info);
  if (controlled_icon)
    container->AddRightView(controlled_icon);
  return container;
}

void NetworkListView::UpdateViewForNetwork(HoverHighlightView* view,
                                           const NetworkInfo& info) {
  DCHECK(!view->is_populated());
  if (info.connected)
    SetupConnectedItem(view, info.label, info.image);
  else if (info.connecting)
    SetupConnectingItem(view, info.label, info.image);
  else
    view->AddIconAndLabel(info.image, info.label);
  views::View* controlled_icon = CreateControlledByExtensionView(info);
  view->SetTooltipText(info.tooltip);
  if (controlled_icon)
    view->AddChildView(controlled_icon);
}

views::View* NetworkListView::CreateControlledByExtensionView(
    const NetworkInfo& info) {
  NetworkingConfigDelegate* networking_config_delegate =
      Shell::Get()->system_tray_delegate()->GetNetworkingConfigDelegate();
  if (!networking_config_delegate)
    return nullptr;
  std::unique_ptr<const NetworkingConfigDelegate::ExtensionInfo>
      extension_info =
          networking_config_delegate->LookUpExtensionForNetwork(info.guid);
  if (!extension_info)
    return nullptr;

  views::ImageView* controlled_icon = TrayPopupUtils::CreateMainImageView();
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kCaptivePortalIcon, kMenuIconColor));
  controlled_icon->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_EXTENSION_CONTROLLED_WIFI,
      base::UTF8ToUTF16(extension_info->extension_name)));
  return controlled_icon;
}

std::unique_ptr<std::set<std::string>> NetworkListView::UpdateNetworkChildren(
    NetworkInfo::Type type,
    int index) {
  std::unique_ptr<std::set<std::string>> new_guids(new std::set<std::string>);
  for (const auto& info : network_list_) {
    if (info->type != type)
      continue;
    UpdateNetworkChild(index++, info.get());
    new_guids->insert(info->guid);
  }
  return new_guids;
}

void NetworkListView::UpdateNetworkChild(int index, const NetworkInfo* info) {
  HoverHighlightView* network_view = nullptr;
  NetworkGuidMap::const_iterator found = network_guid_map_.find(info->guid);
  if (found == network_guid_map_.end()) {
    network_view = CreateViewForNetwork(*info);
  } else {
    network_view = found->second;
    if (NeedUpdateViewForNetwork(*info)) {
      network_view->Reset();
      UpdateViewForNetwork(network_view, *info);
      network_view->Layout();
      network_view->SchedulePaint();
    }
  }
  PlaceViewAtIndex(network_view, index);
  if (info->disable)
    network_view->SetEnabled(false);
  network_map_[network_view] = info->guid;
  network_guid_map_[info->guid] = network_view;
}

void NetworkListView::PlaceViewAtIndex(views::View* view, int index) {
  if (view->parent() != scroll_content()) {
    scroll_content()->AddChildViewAt(view, index);
  } else {
    if (scroll_content()->child_at(index) == view)
      return;
    scroll_content()->ReorderChildView(view, index);
  }
  needs_relayout_ = true;
}

void NetworkListView::UpdateInfoLabel(int message_id,
                                      int insertion_index,
                                      views::Label** label_ptr) {
  views::Label* label = *label_ptr;
  if (!message_id) {
    if (label) {
      needs_relayout_ = true;
      delete label;
      *label_ptr = nullptr;
    }
    return;
  }
  base::string16 text = l10n_util::GetStringUTF16(message_id);
  if (!label) {
    // TODO(mohsen): Update info label to follow MD specs. See
    // https://crbug.com/687778.
    label = new views::Label();
    label->SetBorder(views::CreateEmptyBorder(
        kTrayPopupPaddingBetweenItems, kTrayPopupPaddingHorizontal,
        kTrayPopupPaddingBetweenItems, 0));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetEnabledColor(SkColorSetARGB(192, 0, 0, 0));
  }
  label->SetText(text);
  PlaceViewAtIndex(label, insertion_index);
  *label_ptr = label;
}

int NetworkListView::UpdateSectionHeaderRow(NetworkTypePattern pattern,
                                            bool enabled,
                                            int child_index,
                                            SectionHeaderRowView** view,
                                            views::Separator** separator_view) {
  if (!*view) {
    if (pattern.Equals(NetworkTypePattern::Cellular()))
      *view = new CellularHeaderRowView();
    else if (pattern.Equals(NetworkTypePattern::Tether()))
      *view = new TetherHeaderRowView();
    else if (pattern.Equals(NetworkTypePattern::WiFi()))
      *view = new WifiHeaderRowView();
    else
      NOTREACHED();
    (*view)->Init(enabled);
  }
  // Show or hide a separator above the header. The separator should only be
  // visible when the header row is not at the top of the list.
  if (child_index > 0) {
    if (!*separator_view)
      *separator_view = TrayPopupUtils::CreateListSubHeaderSeparator();
    PlaceViewAtIndex(*separator_view, child_index++);
  } else {
    if (*separator_view)
      delete *separator_view;
    *separator_view = nullptr;
  }

  (*view)->SetIsOn(enabled);
  PlaceViewAtIndex(*view, child_index++);
  return child_index;
}

void NetworkListView::NetworkIconChanged() {
  Update();
}

bool NetworkListView::NeedUpdateViewForNetwork(const NetworkInfo& info) const {
  NetworkInfoMap::const_iterator found = last_network_info_map_.find(info.guid);
  if (found == last_network_info_map_.end()) {
    // If we cannot find |info| in |last_network_info_map_|, just return true
    // since this is a new network so we have nothing to compare.
    return true;
  } else {
    return *found->second != info;
  }
}

TriView* NetworkListView::CreateConnectionWarning() {
  // Set up layout and apply sticky row property.
  TriView* connection_warning = TrayPopupUtils::CreateDefaultRowView();
  TrayPopupUtils::ConfigureAsStickyHeader(connection_warning);
  connection_warning->set_background(
      views::Background::CreateSolidBackground(kHeaderBackgroundColor));

  // Set 'info' icon on left side.
  views::ImageView* image_view = TrayPopupUtils::CreateMainImageView();
  image_view->SetImage(
      gfx::CreateVectorIcon(kSystemMenuInfoIcon, kMenuIconColor));
  image_view->set_background(
      views::Background::CreateSolidBackground(SK_ColorTRANSPARENT));
  connection_warning->AddView(TriView::Container::START, image_view);

  // Set message label in middle of row.
  views::Label* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING));
  label->set_background(
      views::Background::CreateSolidBackground(SK_ColorTRANSPARENT));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  style.SetupLabel(label);
  connection_warning->AddView(TriView::Container::CENTER, label);

  // Nothing to the right of the text.
  connection_warning->SetContainerVisible(TriView::Container::END, false);
  return connection_warning;
}

}  // namespace tray
}  // namespace ash
