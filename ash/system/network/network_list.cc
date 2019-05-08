// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_info.h"
#include "ash/system/network/network_section_header_view.h"
#include "ash/system/network/network_state_list_detailed_view.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {
namespace {

const int kMobileNetworkBatteryIconSize = 14;
const int kPowerStatusPaddingRight = 10;

bool IsSecondaryUser() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsUserPrimary();
}

}  // namespace

// NetworkListView:

NetworkListView::NetworkListView(DetailedViewDelegate* delegate,
                                 LoginStatus login)
    : NetworkStateListDetailedView(delegate, LIST_TYPE_NETWORK, login) {}

NetworkListView::~NetworkListView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::UpdateNetworkList() {
  CHECK(scroll_content());

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  NetworkStateHandler::NetworkStateList network_list;
  handler->GetVisibleNetworkList(&network_list);
  UpdateNetworks(network_list);
  UpdateNetworkIcons();
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

  bool cellular_enabled =
      NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
          NetworkTypePattern::Cellular());
  network_list_.clear();
  for (const auto* network : networks) {
    if (!NetworkTypePattern::NonVirtual().MatchesType(network->type()))
      continue;
    // If cellular is disabled, skip the default cellular service.
    if (network->IsDefaultCellular() && !cellular_enabled)
      continue;
    network_list_.push_back(std::make_unique<NetworkInfo>(network->guid()));
  }
}

void NetworkListView::UpdateNetworkIcons() {
  SCOPED_NET_LOG_IF_SLOW();
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  bool animating = false;
  for (auto& info : network_list_) {
    const chromeos::NetworkState* network =
        handler->GetNetworkStateFromGuid(info->guid);
    if (!network)
      continue;
    bool prohibited_by_policy = network->blocked_by_policy();
    network_icon::NetworkIconState network_icon_state(network);
    info->label = network_icon::GetLabelForNetwork(
        network_icon_state, network_icon::ICON_TYPE_MENU_LIST);
    // |network_list_| only contains non virtual networks.
    info->image = network_icon::GetImageForNonVirtualNetwork(
        network_icon_state, network_icon::ICON_TYPE_LIST,
        false /* badge_vpn */);
    info->disable =
        (network->activation_state() == shill::kActivationStateActivating) ||
        prohibited_by_policy;
    info->connected = network->IsConnectedState();
    info->connecting = network->IsConnectingState();
    if (network->Matches(NetworkTypePattern::WiFi()))
      info->type = NetworkInfo::Type::WIFI;
    else if (network->Matches(NetworkTypePattern::Mobile()))
      info->type = NetworkInfo::Type::MOBILE;
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
  bool using_proxy = false;
  // TODO(jamescook): Create UIProxyConfigService under mash. This will require
  // the mojo pref service to work with prefs in Local State.
  // http://crbug.com/718072
  if (!::features::IsMultiProcessMash()) {
    using_proxy = NetworkHandler::Get()
                      ->ui_proxy_config_service()
                      ->HasDefaultNetworkProxyConfigured();
  }
  if (using_vpn || using_proxy) {
    if (!connection_warning_)
      connection_warning_ = CreateConnectionWarning();
    PlaceViewAtIndex(connection_warning_, index++);
  }

  // First add high-priority networks (neither Wi-Fi nor Mobile).
  std::unique_ptr<std::set<std::string>> new_guids =
      UpdateNetworkChildren(NetworkInfo::Type::UNKNOWN, index);
  index += new_guids->size();

  if (ShouldMobileDataSectionBeShown()) {
    if (!mobile_header_view_)
      mobile_header_view_ = new MobileSectionHeaderView();

    index = UpdateNetworkSectionHeader(
        NetworkTypePattern::Mobile(), false /* enabled */, index,
        mobile_header_view_, &mobile_separator_view_);

    std::unique_ptr<std::set<std::string>> new_cellular_guids =
        UpdateNetworkChildren(NetworkInfo::Type::MOBILE, index);
    int mobile_status_message =
        mobile_header_view_->UpdateToggleAndGetStatusMessage();
    // |mobile_status_message| may be zero. Passing zero to UpdateInfoLabel
    // clears the label.
    UpdateInfoLabel(mobile_status_message, index, &mobile_status_message_);
    if (mobile_status_message)
      ++index;
    index += new_cellular_guids->size();
    new_guids->insert(new_cellular_guids->begin(), new_cellular_guids->end());
  }

  if (!wifi_header_view_)
    wifi_header_view_ = new WifiSectionHeaderView();

  index = UpdateNetworkSectionHeader(
      NetworkTypePattern::WiFi(),
      handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()), index,
      wifi_header_view_, &wifi_separator_view_);

  // "Wifi Enabled / Disabled".
  if (!handler->IsTechnologyEnabled(NetworkTypePattern::WiFi())) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED, index,
                    &wifi_status_message_);
    return new_guids;
  }

  bool should_clear_info_label = true;
  if (!handler->FirstNetworkByType(NetworkTypePattern::WiFi())) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED, index,
                    &wifi_status_message_);
    ++index;
    should_clear_info_label = false;
  }

  // Add Wi-Fi networks.
  std::unique_ptr<std::set<std::string>> new_wifi_guids =
      UpdateNetworkChildren(NetworkInfo::Type::WIFI, index);
  index += new_wifi_guids->size();
  new_guids->insert(new_wifi_guids->begin(), new_wifi_guids->end());

  // No networks or other messages (fallback).
  if (index == 0) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NO_NETWORKS, index,
                    &wifi_status_message_);
  } else if (should_clear_info_label) {
    // Update the label to show nothing.
    UpdateInfoLabel(0, index, &wifi_status_message_);
  }

  return new_guids;
}

bool NetworkListView::ShouldMobileDataSectionBeShown() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // The section should always be shown if Cellular networks are available.
  if (handler->IsTechnologyAvailable(NetworkTypePattern::Cellular()))
    return true;

  // Hide the section if both Cellular and Tether are UNAVAILABLE.
  if (!handler->IsTechnologyAvailable(NetworkTypePattern::Tether()))
    return false;

  // Hide the section if Tether is PROHIBITED.
  if (handler->IsTechnologyProhibited(NetworkTypePattern::Tether()))
    return false;

  // Secondary users cannot enable Bluetooth, and Tether is only UNINITIALIZED
  // if Bluetooth is disabled. Hide the section in this case.
  if (handler->IsTechnologyUninitialized(NetworkTypePattern::Tether()) &&
      IsSecondaryUser()) {
    return false;
  }

  return true;
}

void NetworkListView::UpdateViewForNetwork(HoverHighlightView* view,
                                           const NetworkInfo& info) {
  view->Reset();
  gfx::ImageSkia network_image;
  if (info.type == NetworkInfo::Type::MOBILE &&
      (!info.connected && !info.connecting)) {
    // Mobile icons which are not connecting or connected should display a small
    // "X" icon superimposed so that it is clear that they are disconnected.
    network_image = gfx::ImageSkiaOperations::CreateSuperimposedImage(
        info.image, gfx::CreateVectorIcon(kNetworkMobileNotConnectedXIcon,
                                          info.image.height(),
                                          kMobileNotConnectedXIconColor));
  } else {
    network_image = info.image;
  }
  view->AddIconAndLabel(network_image, info.label);
  if (info.connected)
    SetupConnectedScrollListItem(view);
  else if (info.connecting)
    SetupConnectingScrollListItem(view);
  view->SetTooltipText(info.tooltip);

  // Add an additional icon to the right of the label for networks
  // that require it (e.g. Tether, controlled by extension).
  views::View* icon = CreatePowerStatusView(info);
  if (icon) {
    view->AddRightView(icon, views::CreateEmptyBorder(gfx::Insets(
                                 0 /* top */, 0 /* left */, 0 /* bottom */,
                                 kPowerStatusPaddingRight)));
  } else {
    icon = CreatePolicyView(info);
    if (!icon)
      icon = CreateControlledByExtensionView(info);
    if (icon)
      view->AddRightView(icon);
  }

  needs_relayout_ = true;
}

views::View* NetworkListView::CreatePowerStatusView(const NetworkInfo& info) {
  // Mobile can be Cellular or Tether.
  if (info.type != NetworkInfo::Type::MOBILE)
    return nullptr;

  const chromeos::NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          info.guid);

  // Only return a battery icon for Tether network type.
  if (!NetworkTypePattern::Tether().MatchesType(network->type()))
    return nullptr;

  views::ImageView* icon = TrayPopupUtils::CreateMoreImageView();
  PowerStatus::BatteryImageInfo icon_info;
  icon_info.charge_percent = network->battery_percentage();
  icon->SetImage(
      PowerStatus::GetBatteryImage(icon_info, kMobileNetworkBatteryIconSize,
                                   kMenuIconColorDisabled, kMenuIconColor));

  // Show the numeric battery percentage on hover.
  icon->set_tooltip_text(base::FormatPercent(network->battery_percentage()));

  return icon;
}

views::View* NetworkListView::CreatePolicyView(const NetworkInfo& info) {
  // Check if the network is managed by policy.
  const chromeos::NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          info.guid);
  if (!network || !network->IsManagedByPolicy())
    return nullptr;

  views::ImageView* controlled_icon = TrayPopupUtils::CreateMainImageView();
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kSystemMenuBusinessIcon, kMenuIconColor));
  return controlled_icon;
}

views::View* NetworkListView::CreateControlledByExtensionView(
    const NetworkInfo& info) {
  const chromeos::NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          info.guid);
  if (!network || !network->captive_portal_provider())
    return nullptr;

  views::ImageView* controlled_icon = TrayPopupUtils::CreateMainImageView();
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kCaptivePortalIcon, kMenuIconColor));
  controlled_icon->set_tooltip_text(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_EXTENSION_CONTROLLED_WIFI,
      base::UTF8ToUTF16(network->captive_portal_provider()->name)));
  controlled_icon->SetID(VIEW_ID_EXTENSION_CONTROLLED_WIFI);
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
    network_view = new HoverHighlightView(this);
    UpdateViewForNetwork(network_view, *info);
  } else {
    network_view = found->second;
    if (NeedUpdateViewForNetwork(*info))
      UpdateViewForNetwork(network_view, *info);
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
  } else if (index > 0 && size_t{index} < scroll_content()->children().size() &&
             scroll_content()->children()[size_t{index}] == view) {
    // ReorderChildView() would no-op in this case, but we still want to avoid
    // setting |needs_relayout_|.
    return;
  } else {
    scroll_content()->ReorderChildView(view, index);
  }
  needs_relayout_ = true;
}

void NetworkListView::UpdateInfoLabel(int message_id,
                                      int insertion_index,
                                      TrayInfoLabel** info_label_ptr) {
  TrayInfoLabel* info_label = *info_label_ptr;
  if (!message_id) {
    if (info_label) {
      needs_relayout_ = true;
      delete info_label;
      *info_label_ptr = nullptr;
    }
    return;
  }
  if (!info_label)
    info_label = new TrayInfoLabel(nullptr /* delegate */, message_id);
  else
    info_label->Update(message_id);

  PlaceViewAtIndex(info_label, insertion_index);
  *info_label_ptr = info_label;
}

int NetworkListView::UpdateNetworkSectionHeader(
    NetworkTypePattern pattern,
    bool enabled,
    int child_index,
    NetworkSectionHeaderView* view,
    views::Separator** separator_view) {
  // Show or hide a separator above the header. The separator should only be
  // visible when the header row is not at the top of the list.
  if (child_index > 0) {
    if (!*separator_view)
      *separator_view = CreateListSubHeaderSeparator();
    PlaceViewAtIndex(*separator_view, child_index++);
  } else {
    if (*separator_view)
      delete *separator_view;
    *separator_view = nullptr;
  }

  bool default_toggle_enabled = !IsSecondaryUser();
  // Mobile updates its toggle state independently.
  if (!pattern.MatchesPattern(NetworkTypePattern::Mobile()))
    view->SetToggleState(default_toggle_enabled, enabled /* is_on */);
  PlaceViewAtIndex(view, child_index++);
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

  // Set 'info' icon on left side.
  views::ImageView* image_view = TrayPopupUtils::CreateMainImageView();
  image_view->SetImage(
      gfx::CreateVectorIcon(kSystemMenuInfoIcon, kMenuIconColor));
  image_view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  connection_warning->AddView(TriView::Container::START, image_view);

  // Set message label in middle of row.
  views::Label* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING));
  label->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  style.SetupLabel(label);
  connection_warning->AddView(TriView::Container::CENTER, label);
  connection_warning->SetContainerBorder(
      TriView::Container::CENTER, views::CreateEmptyBorder(gfx::Insets(
                                      0, 0, 0, kTrayPopupLabelRightPadding)));

  // Nothing to the right of the text.
  connection_warning->SetContainerVisible(TriView::Container::END, false);
  return connection_warning;
}

}  // namespace tray
}  // namespace ash
