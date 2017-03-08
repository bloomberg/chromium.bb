// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/network_list_md.h"

#include <stddef.h>

#include "ash/common/system/chromeos/network/network_icon.h"
#include "ash/common/system/chromeos/network/network_icon_animation.h"
#include "ash/common/system/chromeos/network/network_list_delegate.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ptr_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

using chromeos::LoginState;
using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::ManagedNetworkConfigurationHandler;
using chromeos::NetworkTypePattern;

namespace ash {

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
      network->guid(), network->profile_path());
}

}  // namespace

// A header row for sections in network detailed view which contains a title and
// a toggle button to turn on/off the section. Subclasses are given the
// opportunity to add extra buttons before the toggle button is added.
class NetworkListViewMd::SectionHeaderRowView : public views::View,
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
    // call to NetworkListViewMd::Update().
    toggle_->SetEnabled(false);
    OnToggleToggled(toggle_->is_on());
  }

 private:
  void InitializeLayout() {
    // TODO(mohsen): Consider using TriView class and adding a utility function
    // to TrayPopupUtils to simplify creation of the following layout. See
    // https://crbug.com/614453.
    TrayPopupUtils::ConfigureAsStickyHeader(this);
    SetLayoutManager(new views::FillLayout);
    container_ = TrayPopupUtils::CreateSubHeaderRowView();
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

class CellularHeaderRowView : public NetworkListViewMd::SectionHeaderRowView {
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

class TetherHeaderRowView : public NetworkListViewMd::SectionHeaderRowView {
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

class WifiHeaderRowView : public NetworkListViewMd::SectionHeaderRowView {
 public:
  explicit WifiHeaderRowView(NetworkListDelegate* network_list_delegate)
      : SectionHeaderRowView(IDS_ASH_STATUS_TRAY_NETWORK_WIFI),
        network_list_delegate_(network_list_delegate),
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
      network_list_delegate_->OnOtherWifiClicked();
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

  NetworkListDelegate* network_list_delegate_;

  // A button to invoke "Join Wi-Fi network" dialog.
  SystemMenuButton* join_;

  DISALLOW_COPY_AND_ASSIGN(WifiHeaderRowView);
};

}  // namespace

// NetworkListViewMd:

NetworkListViewMd::NetworkListViewMd(NetworkListDelegate* delegate)
    : needs_relayout_(false),
      delegate_(delegate),
      no_wifi_networks_view_(nullptr),
      no_cellular_networks_view_(nullptr),
      cellular_header_view_(nullptr),
      tether_header_view_(nullptr),
      wifi_header_view_(nullptr),
      cellular_separator_view_(nullptr),
      tether_separator_view_(nullptr),
      wifi_separator_view_(nullptr) {
  CHECK(delegate_);
}

NetworkListViewMd::~NetworkListViewMd() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListViewMd::Update() {
  CHECK(container());

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  NetworkStateHandler::NetworkStateList network_list;
  handler->GetVisibleNetworkList(&network_list);
  UpdateNetworks(network_list);

  NetworkStateHandler::NetworkStateList tether_network_list;
  handler->GetTetherNetworkList(0 /* no limit */, &tether_network_list);
  for (const auto* tether_network : tether_network_list) {
    network_list_.push_back(
        base::MakeUnique<NetworkInfo>(tether_network->guid()));
  }

  UpdateNetworkIcons();
  OrderNetworks();
  UpdateNetworkListInternal();
}

bool NetworkListViewMd::IsNetworkEntry(views::View* view,
                                       std::string* guid) const {
  std::map<views::View*, std::string>::const_iterator found =
      network_map_.find(view);
  if (found == network_map_.end())
    return false;
  *guid = found->second;
  return true;
}

void NetworkListViewMd::UpdateNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  SCOPED_NET_LOG_IF_SLOW();
  network_list_.clear();
  const NetworkTypePattern pattern = delegate_->GetNetworkTypePattern();
  for (const auto* network : networks) {
    if (pattern.MatchesType(network->type()))
      network_list_.push_back(base::MakeUnique<NetworkInfo>(network->guid()));
  }
}

void NetworkListViewMd::OrderNetworks() {
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
      if (network1->highlight != network2->highlight)
        return network1->highlight;
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

void NetworkListViewMd::UpdateNetworkIcons() {
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
    info->highlight = info->connected || info->connecting;
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

void NetworkListViewMd::UpdateNetworkListInternal() {
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
    if (delegate_->IsViewHovered(iter.second)) {
      selected_view = iter.second;
      break;
    }
  }
  container()->SizeToPreferredSize();
  delegate_->RelayoutScrollList();
  if (selected_view)
    container()->ScrollRectToVisible(selected_view->bounds());
}

std::unique_ptr<std::set<std::string>>
NetworkListViewMd::UpdateNetworkListEntries() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // First add high-priority networks (not Wi-Fi nor cellular).
  std::unique_ptr<std::set<std::string>> new_guids =
      UpdateNetworkChildren(NetworkInfo::Type::UNKNOWN, 0);

  // Keep an index where the next child should be inserted.
  int index = new_guids->size();

  const NetworkTypePattern pattern = delegate_->GetNetworkTypePattern();
  if (pattern.MatchesPattern(NetworkTypePattern::Cellular())) {
    if (handler->IsTechnologyAvailable(NetworkTypePattern::Cellular())) {
      index = UpdateSectionHeaderRow(
          NetworkTypePattern::Cellular(),
          handler->IsTechnologyEnabled(NetworkTypePattern::Cellular()), index,
          &cellular_header_view_, &cellular_separator_view_);
    }

    // Cellular initializing.
    int message_id = network_icon::GetCellularUninitializedMsg();
    if (!message_id &&
        handler->IsTechnologyEnabled(NetworkTypePattern::Mobile()) &&
        !handler->FirstNetworkByType(NetworkTypePattern::Mobile())) {
      message_id = IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS;
    }
    UpdateInfoLabel(message_id, index, &no_cellular_networks_view_);
    if (message_id)
      ++index;

    // Add cellular networks.
    std::unique_ptr<std::set<std::string>> new_cellular_guids =
        UpdateNetworkChildren(NetworkInfo::Type::CELLULAR, index);
    index += new_cellular_guids->size();
    new_guids->insert(new_cellular_guids->begin(), new_cellular_guids->end());
  }

  // TODO (hansberry): Audit existing usage of NonVirtual and consider changing
  // it to include Tether. See crbug.com/693647.
  if (handler->IsTechnologyAvailable(NetworkTypePattern::Tether())) {
    index = UpdateSectionHeaderRow(
        NetworkTypePattern::Tether(),
        handler->IsTechnologyEnabled(NetworkTypePattern::Tether()), index,
        &tether_header_view_, &tether_separator_view_);

    // TODO (hansberry): Should a message similar to
    // IDS_ASH_STATUS_TRAY_NO_CELLULAR_NETWORKS be shown if Tether technology
    // is enabled but no networks are around?

    // Add Tether networks.
    std::unique_ptr<std::set<std::string>> new_tether_guids =
        UpdateNetworkChildren(NetworkInfo::Type::TETHER, index);
    index += new_tether_guids->size();
    new_guids->insert(new_tether_guids->begin(), new_tether_guids->end());
  }

  if (pattern.MatchesPattern(NetworkTypePattern::WiFi())) {
    index = UpdateSectionHeaderRow(
        NetworkTypePattern::WiFi(),
        handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()), index,
        &wifi_header_view_, &wifi_separator_view_);

    // "Wifi Enabled / Disabled".
    int message_id = 0;
    if (network_list_.empty()) {
      message_id = handler->IsTechnologyEnabled(NetworkTypePattern::WiFi())
                       ? IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED
                       : IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
    }
    UpdateInfoLabel(message_id, index, &no_wifi_networks_view_);
    if (message_id)
      ++index;

    // Add Wi-Fi networks.
    std::unique_ptr<std::set<std::string>> new_wifi_guids =
        UpdateNetworkChildren(NetworkInfo::Type::WIFI, index);
    index += new_wifi_guids->size();
    new_guids->insert(new_wifi_guids->begin(), new_wifi_guids->end());
  }

  // No networks or other messages (fallback).
  if (index == 0) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NO_NETWORKS, index,
                    &no_wifi_networks_view_);
  }

  return new_guids;
}

std::unique_ptr<std::set<std::string>> NetworkListViewMd::UpdateNetworkChildren(
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

void NetworkListViewMd::UpdateNetworkChild(int index, const NetworkInfo* info) {
  views::View* network_view = nullptr;
  NetworkGuidMap::const_iterator found = network_guid_map_.find(info->guid);
  if (found == network_guid_map_.end()) {
    network_view = delegate_->CreateViewForNetwork(*info);
  } else {
    network_view = found->second;
    network_view->RemoveAllChildViews(true);
    delegate_->UpdateViewForNetwork(network_view, *info);
    network_view->Layout();
    network_view->SchedulePaint();
  }
  PlaceViewAtIndex(network_view, index);
  if (info->disable)
    network_view->SetEnabled(false);
  network_map_[network_view] = info->guid;
  network_guid_map_[info->guid] = network_view;
}

void NetworkListViewMd::PlaceViewAtIndex(views::View* view, int index) {
  if (view->parent() != container()) {
    container()->AddChildViewAt(view, index);
  } else {
    if (container()->child_at(index) == view)
      return;
    container()->ReorderChildView(view, index);
  }
  needs_relayout_ = true;
}

void NetworkListViewMd::UpdateInfoLabel(int message_id,
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
  base::string16 text =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
  if (!label)
    label = delegate_->CreateInfoLabel();
  label->SetText(text);
  PlaceViewAtIndex(label, insertion_index);
  *label_ptr = label;
}

int NetworkListViewMd::UpdateSectionHeaderRow(
    NetworkTypePattern pattern,
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
      *view = new WifiHeaderRowView(delegate_);
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

void NetworkListViewMd::NetworkIconChanged() {
  Update();
}

}  // namespace ash
