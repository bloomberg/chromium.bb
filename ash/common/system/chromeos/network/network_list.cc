// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/network_list.h"

#include <stddef.h>

#include "ash/common/system/chromeos/network/network_icon.h"
#include "ash/common/system/chromeos/network/network_icon_animation.h"
#include "ash/common/system/chromeos/network/network_info.h"
#include "ash/common/system/chromeos/network/network_list_delegate.h"
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
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"
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

// NetworkListView:

NetworkListView::NetworkListView(NetworkListDelegate* delegate)
    : delegate_(delegate),
      no_wifi_networks_view_(nullptr),
      no_cellular_networks_view_(nullptr) {
  CHECK(delegate_);
}

NetworkListView::~NetworkListView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::Update() {
  CHECK(container());
  NetworkStateHandler::NetworkStateList network_list;
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
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
  network_list_.clear();
  const NetworkTypePattern pattern = delegate_->GetNetworkTypePattern();
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin();
       iter != networks.end(); ++iter) {
    const chromeos::NetworkState* network = *iter;
    if (!pattern.MatchesType(network->type()))
      continue;
    network_list_.push_back(base::MakeUnique<NetworkInfo>(network->guid()));
  }
}

void NetworkListView::UpdateNetworkIcons() {
  SCOPED_NET_LOG_IF_SLOW();
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // First, update state for all networks
  bool animating = false;

  for (auto& info : network_list_) {
    const chromeos::NetworkState* network =
        handler->GetNetworkStateFromGuid(info->guid);
    if (!network)
      continue;
    bool prohibited_by_policy = IsProhibitedByPolicy(network);
    info->image =
        network_icon::GetImageForNetwork(network, network_icon::ICON_TYPE_LIST);
    info->label =
        network_icon::GetLabelForNetwork(network, network_icon::ICON_TYPE_LIST);
    info->highlight =
        network->IsConnectedState() || network->IsConnectingState();
    info->disable =
        (network->activation_state() == shill::kActivationStateActivating) ||
        prohibited_by_policy;
    if (network->Matches(NetworkTypePattern::WiFi()))
      info->type = NetworkInfo::Type::WIFI;
    else if (network->Matches(NetworkTypePattern::Cellular()))
      info->type = NetworkInfo::Type::CELLULAR;
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
  // Get the updated list entries
  network_map_.clear();
  std::set<std::string> new_guids;
  bool needs_relayout = UpdateNetworkListEntries(&new_guids);

  // Remove old children
  std::set<std::string> remove_guids;
  for (NetworkGuidMap::const_iterator it = network_guid_map_.begin();
       it != network_guid_map_.end(); ++it) {
    if (new_guids.find(it->first) == new_guids.end()) {
      remove_guids.insert(it->first);
      network_map_.erase(it->second);
      delete it->second;
      needs_relayout = true;
    }
  }

  for (std::set<std::string>::const_iterator remove_it = remove_guids.begin();
       remove_it != remove_guids.end(); ++remove_it) {
    network_guid_map_.erase(*remove_it);
  }

  if (needs_relayout)
    HandleRelayout();
}

void NetworkListView::HandleRelayout() {
  views::View* selected_view = nullptr;
  for (auto& iter : network_guid_map_) {
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

bool NetworkListView::UpdateNetworkListEntries(
    std::set<std::string>* new_guids) {
  bool needs_relayout = false;
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // Insert child views
  int index = 0;

  // Highlighted networks
  needs_relayout |=
      UpdateNetworkChildren(new_guids, &index, true /* highlighted */);

  const NetworkTypePattern pattern = delegate_->GetNetworkTypePattern();
  if (pattern.MatchesPattern(NetworkTypePattern::Cellular())) {
    // Cellular initializing
    int message_id = network_icon::GetCellularUninitializedMsg();
    if (!message_id &&
        handler->IsTechnologyEnabled(NetworkTypePattern::Mobile()) &&
        !handler->FirstNetworkByType(NetworkTypePattern::Mobile())) {
      message_id = IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS;
    }
    needs_relayout |=
        UpdateInfoLabel(message_id, index, &no_cellular_networks_view_);

    if (message_id)
      ++index;
  }

  if (pattern.MatchesPattern(NetworkTypePattern::WiFi())) {
    // "Wifi Enabled / Disabled"
    int message_id = 0;
    if (network_list_.empty()) {
      message_id = handler->IsTechnologyEnabled(NetworkTypePattern::WiFi())
                       ? IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED
                       : IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
    }
    needs_relayout |=
        UpdateInfoLabel(message_id, index, &no_wifi_networks_view_);
    if (message_id)
      ++index;
  }

  // Un-highlighted networks
  needs_relayout |=
      UpdateNetworkChildren(new_guids, &index, false /* not highlighted */);

  // No networks or other messages (fallback)
  if (index == 0) {
    needs_relayout |= UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NO_NETWORKS, index,
                                      &no_wifi_networks_view_);
  }

  return needs_relayout;
}

bool NetworkListView::UpdateNetworkChildren(std::set<std::string>* new_guids,
                                            int* child_index,
                                            bool highlighted) {
  bool needs_relayout = false;
  int index = *child_index;
  for (auto& info : network_list_) {
    if (info->highlight != highlighted)
      continue;
    needs_relayout |= UpdateNetworkChild(index++, info.get());
    new_guids->insert(info->guid);
  }
  *child_index = index;
  return needs_relayout;
}

bool NetworkListView::UpdateNetworkChild(int index, const NetworkInfo* info) {
  bool needs_relayout = false;
  views::View* network_view = nullptr;
  NetworkGuidMap::const_iterator found = network_guid_map_.find(info->guid);
  if (found == network_guid_map_.end()) {
    network_view = delegate_->CreateViewForNetwork(*info);
    container()->AddChildViewAt(network_view, index);
    needs_relayout = true;
  } else {
    network_view = found->second;
    network_view->RemoveAllChildViews(true);
    delegate_->UpdateViewForNetwork(network_view, *info);
    network_view->Layout();
    network_view->SchedulePaint();
    needs_relayout = PlaceViewAtIndex(network_view, index);
  }
  if (info->disable)
    network_view->SetEnabled(false);
  network_map_[network_view] = info->guid;
  network_guid_map_[info->guid] = network_view;
  return needs_relayout;
}

bool NetworkListView::PlaceViewAtIndex(views::View* view, int index) {
  if (container()->child_at(index) == view)
    return false;
  container()->ReorderChildView(view, index);
  return true;
}

bool NetworkListView::UpdateInfoLabel(int message_id,
                                      int index,
                                      views::Label** label) {
  CHECK(label);
  bool needs_relayout = false;
  if (message_id) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    base::string16 text = rb.GetLocalizedString(message_id);
    if (!*label) {
      *label = delegate_->CreateInfoLabel();
      (*label)->SetText(text);
      container()->AddChildViewAt(*label, index);
      needs_relayout = true;
    } else {
      (*label)->SetText(text);
      needs_relayout = PlaceViewAtIndex(*label, index);
    }
  } else if (*label) {
    delete *label;
    *label = nullptr;
    needs_relayout = true;
  }
  return needs_relayout;
}

void NetworkListView::NetworkIconChanged() {
  Update();
}

}  // namespace ash
