// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_list_view.h"

#include <memory>
#include <vector>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/throbber_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace tray {
namespace {

// Indicates whether |network| belongs to this VPN provider.
bool VpnProviderMatchesNetwork(const VPNProvider& provider,
                               const chromeos::NetworkState& network) {
  if (network.type() != shill::kTypeVPN)
    return false;
  const bool network_uses_third_party_provider =
      network.vpn_provider_type() == shill::kProviderThirdPartyVpn;
  if (!provider.third_party)
    return !network_uses_third_party_provider;
  return network_uses_third_party_provider &&
         network.third_party_vpn_provider_extension_id() ==
             provider.extension_id;
}

// A list entry that represents a VPN provider.
class VPNListProviderEntry : public views::ButtonListener, public views::View {
 public:
  VPNListProviderEntry(const VPNProvider& vpn_provider,
                       bool top_item,
                       const std::string& name,
                       int button_accessible_name_id)
      : vpn_provider_(vpn_provider) {
    TrayPopupUtils::ConfigureAsStickyHeader(this);
    SetLayoutManager(new views::FillLayout);
    TriView* tri_view = TrayPopupUtils::CreateSubHeaderRowView(false);
    AddChildView(tri_view);

    views::Label* label = TrayPopupUtils::CreateDefaultLabel();
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SUB_HEADER);
    style.SetupLabel(label);
    label->SetText(base::ASCIIToUTF16(name));
    tri_view->AddView(TriView::Container::CENTER, label);

    const SkColor image_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
    gfx::ImageSkia icon =
        gfx::CreateVectorIcon(kSystemMenuAddConnectionIcon, image_color);
    SystemMenuButton* add_vpn_button =
        new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED, icon,
                             icon, button_accessible_name_id);
    add_vpn_button->SetInkDropColor(image_color);
    add_vpn_button->SetEnabled(true);
    tri_view->AddView(TriView::Container::END, add_vpn_button);
  }

 protected:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    // If the user clicks on a provider entry, request that the "add network"
    // dialog for this provider be shown.
    if (vpn_provider_.third_party) {
      ShellPort::Get()->RecordUserMetricsAction(
          UMA_STATUS_AREA_VPN_ADD_THIRD_PARTY_CLICKED);
      Shell::Get()->system_tray_controller()->ShowThirdPartyVpnCreate(
          vpn_provider_.extension_id);
    } else {
      ShellPort::Get()->RecordUserMetricsAction(
          UMA_STATUS_AREA_VPN_ADD_BUILT_IN_CLICKED);
      Shell::Get()->system_tray_controller()->ShowNetworkCreate(
          shill::kTypeVPN);
    }
  }

 private:
  const VPNProvider vpn_provider_;

  DISALLOW_COPY_AND_ASSIGN(VPNListProviderEntry);
};

// A list entry that represents a network. If the network is currently
// connecting, the icon shown by this list entry will be animated. If the
// network is currently connected, a disconnect button will be shown next to its
// name.
class VPNListNetworkEntry : public HoverHighlightView,
                            public network_icon::AnimationObserver {
 public:
  VPNListNetworkEntry(ViewClickListener* listener,
                      const chromeos::NetworkState* network);
  ~VPNListNetworkEntry() override;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

 private:
  void UpdateFromNetworkState(const chromeos::NetworkState* network);
  void SetupConnectedItem(const base::string16& text,
                          const gfx::ImageSkia& image);
  void SetupConnectingItem(const base::string16& text,
                           const gfx::ImageSkia& image);

  const std::string guid_;

  views::LabelButton* disconnect_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VPNListNetworkEntry);
};

VPNListNetworkEntry::VPNListNetworkEntry(ViewClickListener* listener,
                                         const chromeos::NetworkState* network)
    : HoverHighlightView(listener), guid_(network->guid()) {
  UpdateFromNetworkState(network);
}

VPNListNetworkEntry::~VPNListNetworkEntry() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void VPNListNetworkEntry::NetworkIconChanged() {
  UpdateFromNetworkState(chromeos::NetworkHandler::Get()
                             ->network_state_handler()
                             ->GetNetworkStateFromGuid(guid_));
}

void VPNListNetworkEntry::ButtonPressed(Button* sender,
                                        const ui::Event& event) {
  if (sender != disconnect_button_) {
    HoverHighlightView::ButtonPressed(sender, event);
    return;
  }

  chromeos::NetworkConnect::Get()->DisconnectFromNetworkId(guid_);
}

void VPNListNetworkEntry::UpdateFromNetworkState(
    const chromeos::NetworkState* network) {
  if (network && network->IsConnectingState())
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);

  if (!network) {
    // This is a transient state where the network has been removed already but
    // the network list in the UI has not been updated yet.
    return;
  }
  Reset();
  disconnect_button_ = nullptr;

  gfx::ImageSkia image =
      network_icon::GetImageForNetwork(network, network_icon::ICON_TYPE_LIST);
  base::string16 label = network_icon::GetLabelForNetwork(
      network, network_icon::ICON_TYPE_MENU_LIST);
  if (network->IsConnectedState())
    SetupConnectedItem(label, image);
  else if (network->IsConnectingState())
    SetupConnectingItem(label, image);
  else
    AddIconAndLabel(image, label);

  if (network->IsConnectedState()) {
    disconnect_button_ = TrayPopupUtils::CreateTrayPopupButton(
        this, l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_DISCONNECT));
    tri_view()->AddView(TriView::Container::END, disconnect_button_);
    tri_view()->SetContainerVisible(TriView::Container::END, true);
    tri_view()->SetContainerBorder(
        TriView::Container::END,
        views::CreateEmptyBorder(0, 0, 0, kTrayPopupButtonEndMargin));
  }
  Layout();
}

// TODO(varkha|mohsen): Consolidate with a similar method in
// BluetoothDetailedView. See https://crbug.com/686924.
void VPNListNetworkEntry::SetupConnectedItem(const base::string16& text,
                                             const gfx::ImageSkia& image) {
  AddIconAndLabels(
      image, text,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::CAPTION);
  style.set_color_style(TrayPopupItemStyle::ColorStyle::CONNECTED);
  style.SetupLabel(sub_text_label());
}

// TODO(varkha|mohsen): Consolidate with a similar method in
// BluetoothDetailedView. See https://crbug.com/686924.
void VPNListNetworkEntry::SetupConnectingItem(const base::string16& text,
                                              const gfx::ImageSkia& image) {
  AddIconAndLabels(
      image, text,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING));
  ThrobberView* throbber = new ThrobberView;
  throbber->Start();
  AddRightView(throbber);
}

}  // namespace

VPNListView::VPNListView(SystemTrayItem* owner, LoginStatus login)
    : NetworkStateListDetailedView(owner, LIST_TYPE_VPN, login) {
  Shell::Get()->vpn_list()->AddObserver(this);
}

VPNListView::~VPNListView() {
  Shell::Get()->vpn_list()->RemoveObserver(this);
}

void VPNListView::UpdateNetworkList() {
  // Before updating the list, determine whether the user was hovering over one
  // of the VPN provider or network entries.
  std::unique_ptr<VPNProvider> hovered_provider;
  std::string hovered_network_guid;
  for (const std::pair<const views::View* const, VPNProvider>& provider :
       provider_view_map_) {
    if (provider.first->IsMouseHovered()) {
      hovered_provider.reset(new VPNProvider(provider.second));
      break;
    }
  }
  if (!hovered_provider) {
    for (const std::pair<const views::View*, std::string>& entry :
         network_view_guid_map_) {
      if (entry.first->IsMouseHovered()) {
        hovered_network_guid = entry.second;
        break;
      }
    }
  }

  // Clear the list.
  scroll_content()->RemoveAllChildViews(true);
  provider_view_map_.clear();
  network_view_guid_map_.clear();
  list_empty_ = true;

  // Get the list of available VPN networks, in shill's priority order.
  chromeos::NetworkStateHandler::NetworkStateList networks;
  chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetVisibleNetworkListByType(chromeos::NetworkTypePattern::VPN(),
                                    &networks);

  // Show all VPN providers and all networks that are currently disconnected.
  AddProvidersAndNetworks(networks);

  // Determine whether one of the new list entries corresponds to the entry that
  // the user was previously hovering over. If such an entry is found, the list
  // will be scrolled to ensure the entry is visible.
  const views::View* scroll_to_show_view = nullptr;
  if (hovered_provider) {
    for (const std::pair<const views::View* const, VPNProvider>& provider :
         provider_view_map_) {
      if (provider.second == *hovered_provider) {
        scroll_to_show_view = provider.first;
        break;
      }
    }
  } else if (!hovered_network_guid.empty()) {
    for (const std::pair<const views::View*, std::string>& entry :
         network_view_guid_map_) {
      if (entry.second == hovered_network_guid) {
        scroll_to_show_view = entry.first;
        break;
      }
    }
  }

  // Layout the updated list.
  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();

  if (scroll_to_show_view) {
    // Scroll the list so that |scroll_to_show_view| is in view.
    scroll_content()->ScrollRectToVisible(scroll_to_show_view->bounds());
  }
}

bool VPNListView::IsNetworkEntry(views::View* view, std::string* guid) const {
  const auto& entry = network_view_guid_map_.find(view);
  if (entry == network_view_guid_map_.end())
    return false;
  *guid = entry->second;
  return true;
}

void VPNListView::OnVPNProvidersChanged() {
  UpdateNetworkList();
}

void VPNListView::AddNetwork(const chromeos::NetworkState* network) {
  views::View* entry(new VPNListNetworkEntry(this, network));
  scroll_content()->AddChildView(entry);
  network_view_guid_map_[entry] = network->guid();
  list_empty_ = false;
}

void VPNListView::AddProviderAndNetworks(
    const VPNProvider& vpn_provider,
    const chromeos::NetworkStateHandler::NetworkStateList& networks) {
  // Add a visual separator, unless this is the topmost entry in the list.
  if (!list_empty_) {
    scroll_content()->AddChildView(
        TrayPopupUtils::CreateListSubHeaderSeparator());
  }
  std::string vpn_name =
      vpn_provider.third_party
          ? vpn_provider.third_party_provider_name
          : l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_VPN_BUILT_IN_PROVIDER);

  // Add a list entry for the VPN provider.
  views::View* provider_view = nullptr;
  provider_view = new VPNListProviderEntry(vpn_provider, list_empty_, vpn_name,
                                           IDS_ASH_STATUS_TRAY_ADD_CONNECTION);
  scroll_content()->AddChildView(provider_view);
  provider_view_map_[provider_view] = vpn_provider;
  list_empty_ = false;
  // Add the networks belonging to this provider, in the priority order returned
  // by shill.
  for (const chromeos::NetworkState* const& network : networks) {
    if (VpnProviderMatchesNetwork(vpn_provider, *network))
      AddNetwork(network);
  }
}

void VPNListView::AddProvidersAndNetworks(
    const chromeos::NetworkStateHandler::NetworkStateList& networks) {
  // Get the list of VPN providers enabled in the primary user's profile.
  std::vector<VPNProvider> providers =
      Shell::Get()->vpn_list()->vpn_providers();

  // Add providers with at least one configured network along with their
  // networks. Providers are added in the order of their highest priority
  // network.
  for (const chromeos::NetworkState* const& network : networks) {
    for (auto provider = providers.begin(); provider != providers.end();
         ++provider) {
      if (!VpnProviderMatchesNetwork(*provider, *network))
        continue;
      AddProviderAndNetworks(*provider, networks);
      providers.erase(provider);
      break;
    }
  }

  // Add providers without any configured networks, in the order that the
  // providers were returned by the extensions system.
  for (const VPNProvider& provider : providers)
    AddProviderAndNetworks(provider, networks);
}

}  // namespace tray
}  // namespace ash
