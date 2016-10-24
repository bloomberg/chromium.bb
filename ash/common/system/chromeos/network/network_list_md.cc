// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/network_list_md.h"

#include <stddef.h>

#include "ash/common/system/chromeos/network/network_icon.h"
#include "ash/common/system/chromeos/network/network_icon_animation.h"
#include "ash/common/system/chromeos/network/network_info.h"
#include "ash/common/system/chromeos/network/network_list_delegate.h"
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
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
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

const int kWiFiButtonSize = 48;
const int kWifiRowVerticalInset = 4;
const int kWifiRowLeftInset = 18;
const int kWifiRowRightInset = 14;
const int kWifiRowSeparatorThickness = 1;
const int kWifiRowChildSpacing = 14;
const int kFocusBorderInset = 1;

const SkColor kWifiRowSeparatorColor = SkColorSetA(SK_ColorBLACK, 0x1F);

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

class NetworkListViewMd::WifiHeaderRowView : public views::View {
 public:
  WifiHeaderRowView(views::ButtonListener* listener, bool enabled)
      : views::View(),
        listener_(listener),
        label_(nullptr),
        toggle_(nullptr),
        join_(nullptr) {
    Init();
    SetWifiEnabled(enabled);
  }

  ~WifiHeaderRowView() override {}

  void SetWifiEnabled(bool enabled) {
    join_->SetVisible(enabled);
    toggle_->SetIsOn(enabled, true);
  }

  const views::Button* toggle() const { return toggle_; }
  const views::Button* join() const { return join_; }
  bool is_toggled() const { return toggle_->is_on(); }

 protected:
  // views::View:
  gfx::Size GetPreferredSize() const override {
    gfx::Size size = views::View::GetPreferredSize();
    size.set_height(kWiFiButtonSize + kWifiRowVerticalInset * 2);
    return size;
  }

  int GetHeightForWidth(int w) const override {
    // Make row height fixed avoiding layout manager adjustments.
    return GetPreferredSize().height();
  }

 private:
  void Init() {
    // TODO(tdanderson): Need to unify this with the generic menu row class.
    SetBorder(views::Border::CreateSolidSidedBorder(
        kWifiRowSeparatorThickness, 0, 0, 0, kWifiRowSeparatorColor));
    views::View* container = new views::View;
    container->SetBorder(views::Border::CreateEmptyBorder(
        0, kWifiRowLeftInset, 0, kWifiRowRightInset));
    views::FillLayout* layout = new views::FillLayout;
    SetLayoutManager(layout);
    AddChildView(container);

    views::BoxLayout* container_layout = new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, kWifiRowChildSpacing);
    container_layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
    container->SetLayoutManager(container_layout);
    ui::NativeTheme* theme = GetNativeTheme();
    const SkColor prominent_color =
        theme->GetSystemColor(ui::NativeTheme::kColorId_ProminentButtonColor);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    label_ = new views::Label(
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_WIFI),
        rb.GetFontList(ui::ResourceBundle::MediumFont));
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_->SetEnabledColor(prominent_color);
    container->AddChildView(label_);
    container_layout->SetFlexForView(label_, 1);

    // TODO(varkha): Make this a SystemMenuButton.
    join_ = new views::ImageButton(listener_);
    join_image_ = network_icon::GetImageForNewWifiNetwork(
        SkColorSetA(prominent_color, 0xFF / 2), prominent_color);
    join_->SetImage(views::CustomButton::STATE_NORMAL, &join_image_);

    const SkColor focus_color =
        theme->GetSystemColor(ui::NativeTheme::kColorId_FocusedBorderColor);
    const int horizontal_padding = (kWiFiButtonSize - join_image_.width()) / 2;
    const int vertical_padding = (kWiFiButtonSize - join_image_.height()) / 2;
    join_->SetBorder(views::Border::CreateEmptyBorder(
        gfx::Insets(vertical_padding, horizontal_padding)));
    join_->SetFocusForPlatform();
    join_->SetFocusPainter(views::Painter::CreateSolidFocusPainter(
        focus_color, gfx::Insets(kFocusBorderInset)));
    container->AddChildView(join_);

    toggle_ = new views::ToggleButton(listener_);
    // TODO(varkha): Implement focus painter.
    toggle_->SetFocusForPlatform();
    container->AddChildView(toggle_);
  }

  // ButtonListener to notify when |toggle_| or |join_| are clicked.
  views::ButtonListener* listener_;

  // Text label for the row.
  views::Label* label_;

  // ToggleButton to toggle Wi-Fi on or off.
  views::ToggleButton* toggle_;

  // A button to invoke "Join Wi-Fi network" dialog.
  views::ImageButton* join_;

  // Image used for the |join_| button.
  gfx::ImageSkia join_image_;

  DISALLOW_COPY_AND_ASSIGN(WifiHeaderRowView);
};

// NetworkListViewMd:

NetworkListViewMd::NetworkListViewMd(NetworkListDelegate* delegate)
    : needs_relayout_(false),
      delegate_(delegate),
      no_wifi_networks_view_(nullptr),
      no_cellular_networks_view_(nullptr),
      wifi_header_view_(nullptr) {
  CHECK(delegate_);
}

NetworkListViewMd::~NetworkListViewMd() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListViewMd::Update() {
  CHECK(container());
  NetworkStateHandler::NetworkStateList network_list;
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  handler->GetVisibleNetworkList(&network_list);
  UpdateNetworks(network_list);
  OrderNetworks();
  UpdateNetworkIcons();
  UpdateNetworkListInternal();
}

bool NetworkListViewMd::IsNetworkEntry(views::View* view,
                                       std::string* service_path) const {
  std::map<views::View*, std::string>::const_iterator found =
      network_map_.find(view);
  if (found == network_map_.end())
    return false;
  *service_path = found->second;
  return true;
}

void NetworkListViewMd::UpdateNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  SCOPED_NET_LOG_IF_SLOW();
  network_list_.clear();
  const NetworkTypePattern pattern = delegate_->GetNetworkTypePattern();
  for (const auto& network : networks) {
    if (pattern.MatchesType(network->type()))
      network_list_.push_back(base::MakeUnique<NetworkInfo>(network->path()));
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
          GetOrder(handler_->GetNetworkState(network1->service_path));
      const int order2 =
          GetOrder(handler_->GetNetworkState(network2->service_path));
      if (order1 != order2)
        return order1 < order2;
      if (network1->highlight != network2->highlight)
        return network1->highlight;
      return network1->service_path.compare(network2->service_path) < 0;
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
        handler->GetNetworkState(info->service_path);
    if (!network)
      continue;
    bool prohibited_by_policy = IsProhibitedByPolicy(network);
    info->label =
        network_icon::GetLabelForNetwork(network, network_icon::ICON_TYPE_LIST);
    info->image =
        network_icon::GetImageForNetwork(network, network_icon::ICON_TYPE_LIST);
    info->disable =
        (network->activation_state() == shill::kActivationStateActivating) ||
        prohibited_by_policy;
    info->highlight =
        network->IsConnectedState() || network->IsConnectingState();
    info->is_wifi = network->Matches(NetworkTypePattern::WiFi());
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
  std::unique_ptr<std::set<std::string>> new_service_paths =
      UpdateNetworkListEntries();

  // Remove old children.
  std::set<std::string> remove_service_paths;
  for (const auto& iter : service_path_map_) {
    if (new_service_paths->find(iter.first) == new_service_paths->end()) {
      remove_service_paths.insert(iter.first);
      network_map_.erase(iter.second);
      delete iter.second;
      needs_relayout_ = true;
    }
  }

  for (const auto& remove_iter : remove_service_paths)
    service_path_map_.erase(remove_iter);

  if (!needs_relayout_)
    return;

  views::View* selected_view = nullptr;
  for (const auto& iter : service_path_map_) {
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

  // First add high-priority networks (not Wi-Fi).
  std::unique_ptr<std::set<std::string>> new_service_paths =
      UpdateNetworkChildren(false /* not Wi-Fi */, 0);

  // Keep an index of the last inserted child.
  int index = new_service_paths->size();

  const NetworkTypePattern pattern = delegate_->GetNetworkTypePattern();
  if (pattern.MatchesPattern(NetworkTypePattern::Cellular())) {
    // Cellular initializing.
    int message_id = network_icon::GetCellularUninitializedMsg();
    if (!message_id &&
        handler->IsTechnologyEnabled(NetworkTypePattern::Mobile()) &&
        !handler->FirstNetworkByType(NetworkTypePattern::Mobile())) {
      message_id = IDS_ASH_STATUS_TRAY_NO_CELLULAR_NETWORKS;
    }
    UpdateInfoLabel(message_id, index, &no_cellular_networks_view_);
    if (message_id)
      ++index;
  }

  if (pattern.MatchesPattern(NetworkTypePattern::WiFi())) {
    UpdateWifiHeaderRow(
        handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()), index,
        &wifi_header_view_);
    ++index;

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
    std::unique_ptr<std::set<std::string>> new_wifi_service_paths =
        UpdateNetworkChildren(true /* Wi-Fi */, index);
    index += new_wifi_service_paths->size();
    new_service_paths->insert(new_wifi_service_paths->begin(),
                              new_wifi_service_paths->end());
  }

  // No networks or other messages (fallback).
  if (index == 0) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NO_NETWORKS, index,
                    &no_wifi_networks_view_);
  }

  return new_service_paths;
}

std::unique_ptr<std::set<std::string>> NetworkListViewMd::UpdateNetworkChildren(
    bool is_wifi,
    int index) {
  std::unique_ptr<std::set<std::string>> new_service_paths(
      new std::set<std::string>);
  for (const auto& info : network_list_) {
    if (info->is_wifi != is_wifi)
      continue;
    UpdateNetworkChild(index++, info.get());
    new_service_paths->insert(info->service_path);
  }
  return new_service_paths;
}

void NetworkListViewMd::UpdateNetworkChild(int index, const NetworkInfo* info) {
  views::View* network_view = nullptr;
  ServicePathMap::const_iterator found =
      service_path_map_.find(info->service_path);
  if (found == service_path_map_.end()) {
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
  network_map_[network_view] = info->service_path;
  service_path_map_[info->service_path] = network_view;
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

void NetworkListViewMd::UpdateWifiHeaderRow(bool enabled,
                                            int child_index,
                                            WifiHeaderRowView** view) {
  if (!*view)
    *view = new WifiHeaderRowView(this, enabled);
  (*view)->SetWifiEnabled(enabled);
  PlaceViewAtIndex(*view, child_index);
}

void NetworkListViewMd::NetworkIconChanged() {
  Update();
}

void NetworkListViewMd::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  if (sender == wifi_header_view_->toggle()) {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(),
                                  wifi_header_view_->is_toggled(),
                                  chromeos::network_handler::ErrorCallback());
  } else if (sender == wifi_header_view_->join()) {
    delegate_->OnOtherWifiClicked();
  } else {
    NOTREACHED();
  }
}

}  // namespace ash
