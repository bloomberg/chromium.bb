// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/vpn_list_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/common/ash_view_ids.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/chromeos/network/network_icon.h"
#include "ash/common/system/chromeos/network/network_icon_animation.h"
#include "ash/common/system/chromeos/network/network_icon_animation_observer.h"
#include "ash/common/system/chromeos/network/network_list_delegate.h"
#include "ash/common/system/chromeos/network/vpn_list.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_label_button.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

bool UseMd() {
  return MaterialDesignController::IsSystemTrayMenuMaterial();
}

bool IsConnectedOrConnecting(const chromeos::NetworkState* network) {
  return network->IsConnectedState() || network->IsConnectingState();
}

void IgnoreDisconnectError(const std::string& error_name,
                           std::unique_ptr<base::DictionaryValue> error_data) {}

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

// The base class of all list entries, a |HoverHighlightView| with no border.
class VPNListEntryBase : public HoverHighlightView {
 public:
  // When the user clicks the entry, the |parent|'s OnViewClicked() will be
  // invoked.
  explicit VPNListEntryBase(VPNListView* parent);

 private:
  DISALLOW_COPY_AND_ASSIGN(VPNListEntryBase);
};

// A list entry that represents a VPN provider.
class VPNListProviderEntry : public VPNListEntryBase {
 public:
  VPNListProviderEntry(VPNListView* parent, const std::string& name)
      : VPNListEntryBase(parent) {
    views::Label* const label = AddLabel(
        base::UTF8ToUTF16(name), gfx::ALIGN_LEFT, false /* highlight */);
    label->SetBorder(views::CreateEmptyBorder(5, 0, 5, 0));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VPNListProviderEntry);
};

// A list entry that represents a VPN provider with Material Design.
class VPNListProviderEntryMd : public views::ButtonListener,
                               public views::View {
 public:
  VPNListProviderEntryMd(ViewClickListener* parent,
                         const std::string& name,
                         int button_accessible_name_id)
      : parent_(parent) {
    SetLayoutManager(new views::FillLayout);
    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    tri_view->SetContainerVisible(TriView::Container::START, false);
    AddChildView(tri_view);

    const ui::NativeTheme* theme = GetNativeTheme();
    const SkColor prominent_color =
        theme->GetSystemColor(ui::NativeTheme::kColorId_ProminentButtonColor);
    views::Label* label = TrayPopupUtils::CreateDefaultLabel();
    TrayPopupItemStyle style(
        theme, TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
    style.SetupLabel(label);
    label->SetText(base::ASCIIToUTF16(name));
    label->SetEnabledColor(prominent_color);
    tri_view->AddView(TriView::Container::CENTER, label);

    gfx::ImageSkia icon =
        gfx::CreateVectorIcon(kSystemMenuAddConnectionIcon, prominent_color);
    views::ImageButton* add_vpn_button =
        new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED, icon,
                             icon, button_accessible_name_id);
    add_vpn_button->set_ink_drop_base_color(prominent_color);
    add_vpn_button->SetEnabled(true);
    tri_view->AddView(TriView::Container::END, add_vpn_button);
  }

 protected:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    parent_->OnViewClicked(this);
  }

 private:
  // Our parent to handle events.
  ViewClickListener* parent_;

  DISALLOW_COPY_AND_ASSIGN(VPNListProviderEntryMd);
};

// A list entry that represents a network. If the network is currently
// connecting, the icon shown by this list entry will be animated. If the
// network is currently connected, a disconnect button will be shown next to its
// name.
class VPNListNetworkEntry : public VPNListEntryBase,
                            public network_icon::AnimationObserver {
 public:
  VPNListNetworkEntry(VPNListView* parent,
                      const chromeos::NetworkState* network);
  ~VPNListNetworkEntry() override;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // Overriden from ActionableView.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // A disconnect button that will be shown if the network is currently
  // connected. Updates the list entry's hover state as the mouse enters/exits
  // the button.
  class DisconnectButton : public TrayPopupLabelButton {
   public:
    explicit DisconnectButton(VPNListNetworkEntry* parent);

   private:
    // TrayPopupLabelButton:
    void OnMouseEntered(const ui::MouseEvent& event) override;
    void OnMouseExited(const ui::MouseEvent& event) override;
    void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

    VPNListNetworkEntry* parent_;

    DISALLOW_COPY_AND_ASSIGN(DisconnectButton);
  };

  void UpdateFromNetworkState(const chromeos::NetworkState* network);

  const std::string service_path_;

  views::LabelButton* disconnect_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VPNListNetworkEntry);
};

VPNListEntryBase::VPNListEntryBase(VPNListView* parent)
    : HoverHighlightView(parent) {
  SetBorder(views::CreateEmptyBorder(0, kTrayPopupPaddingHorizontal, 0, 0));
}

VPNListNetworkEntry::VPNListNetworkEntry(VPNListView* parent,
                                         const chromeos::NetworkState* network)
    : VPNListEntryBase(parent), service_path_(network->path()) {
  UpdateFromNetworkState(network);
}

VPNListNetworkEntry::~VPNListNetworkEntry() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void VPNListNetworkEntry::NetworkIconChanged() {
  UpdateFromNetworkState(
      chromeos::NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path_));
}

void VPNListNetworkEntry::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender != disconnect_button_) {
    ActionableView::ButtonPressed(sender, event);
    return;
  }

  WmShell::Get()->RecordUserMetricsAction(
      UMA_STATUS_AREA_VPN_DISCONNECT_CLICKED);
  chromeos::NetworkHandler::Get()
      ->network_connection_handler()
      ->DisconnectNetwork(service_path_, base::Bind(&base::DoNothing),
                          base::Bind(&IgnoreDisconnectError));
}

VPNListNetworkEntry::DisconnectButton::DisconnectButton(
    VPNListNetworkEntry* parent)
    : TrayPopupLabelButton(
          parent,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_DISCONNECT)),
      parent_(parent) {
  DCHECK(!UseMd());
  DCHECK(parent_);
}

void VPNListNetworkEntry::DisconnectButton::OnMouseEntered(
    const ui::MouseEvent& event) {
  TrayPopupLabelButton::OnMouseEntered(event);
  parent_->SetHoverHighlight(false);
}

void VPNListNetworkEntry::DisconnectButton::OnMouseExited(
    const ui::MouseEvent& event) {
  TrayPopupLabelButton::OnMouseExited(event);
  if (parent_->IsMouseHovered())
    parent_->SetHoverHighlight(true);
}

void VPNListNetworkEntry::DisconnectButton::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  TrayPopupLabelButton::OnBoundsChanged(previous_bounds);
  if (IsMouseHovered()) {
    SetState(STATE_HOVERED);
    parent_->SetHoverHighlight(false);
  }
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

  RemoveAllChildViews(true);
  disconnect_button_ = nullptr;

  AddIconAndLabel(
      network_icon::GetImageForNetwork(network, network_icon::ICON_TYPE_LIST),
      network_icon::GetLabelForNetwork(network, network_icon::ICON_TYPE_LIST),
      IsConnectedOrConnecting(network));
  if (IsConnectedOrConnecting(network)) {
    if (UseMd()) {
      disconnect_button_ = TrayPopupUtils::CreateTrayPopupButton(
          this, l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_DISCONNECT));
    } else {
      disconnect_button_ = new DisconnectButton(this);
    }
    if (UseMd()) {
      DCHECK(tri_view());
      tri_view()->AddView(TriView::Container::END, disconnect_button_);
      tri_view()->SetContainerVisible(TriView::Container::END, true);
    } else {
      AddChildView(disconnect_button_);
    }

    SetBorder(
        views::CreateEmptyBorder(0, kTrayPopupPaddingHorizontal, 0,
                                 UseMd() ? kTrayPopupButtonEndMargin : 3));
  } else {
    SetBorder(views::CreateEmptyBorder(0, kTrayPopupPaddingHorizontal, 0, 0));
  }

  if (!UseMd()) {
    // The icon and the disconnect button are always set to their preferred
    // size. All remaining space is used for the network name.
    views::BoxLayout* layout = new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 3, kTrayPopupPaddingBetweenItems);
    SetLayoutManager(layout);
    layout->SetDefaultFlex(0);
    layout->SetFlexForView(text_label(), 1);
  }
  Layout();
}

}  // namespace

VPNListView::VPNListView(NetworkListDelegate* delegate) : delegate_(delegate) {
  WmShell::Get()->vpn_list()->AddObserver(this);
}

VPNListView::~VPNListView() {
  WmShell::Get()->vpn_list()->RemoveObserver(this);
}

void VPNListView::Update() {
  // Before updating the list, determine whether the user was hovering over one
  // of the VPN provider or network entries.
  std::unique_ptr<VPNProvider> hovered_provider;
  std::string hovered_network_service_path;
  for (const std::pair<const views::View* const, VPNProvider>& provider :
       provider_view_map_) {
    if (static_cast<const HoverHighlightView*>(provider.first)->hover()) {
      hovered_provider.reset(new VPNProvider(provider.second));
      break;
    }
  }
  if (!hovered_provider) {
    for (const std::pair<const views::View*, std::string>& entry :
         network_view_service_path_map_) {
      if (static_cast<const HoverHighlightView*>(entry.first)->hover()) {
        hovered_network_service_path = entry.second;
        break;
      }
    }
  }

  // Clear the list.
  container()->RemoveAllChildViews(true);
  provider_view_map_.clear();
  network_view_service_path_map_.clear();
  list_empty_ = true;
  if (!UseMd()) {
    container()->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  }

  // Get the list of available VPN networks, in shill's priority order.
  chromeos::NetworkStateHandler::NetworkStateList networks;
  chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetVisibleNetworkListByType(chromeos::NetworkTypePattern::VPN(),
                                    &networks);

  if (!networks.empty() && IsConnectedOrConnecting(networks.front())) {
    // If there is a connected or connecting network, show that network first.
    AddNetwork(networks.front());
    networks.erase(networks.begin());
  }

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
  } else if (!hovered_network_service_path.empty()) {
    for (const std::pair<const views::View*, std::string>& entry :
         network_view_service_path_map_) {
      if (entry.second == hovered_network_service_path) {
        scroll_to_show_view = entry.first;
        break;
      }
    }
  }

  // Layout the updated list.
  container()->SizeToPreferredSize();
  delegate_->RelayoutScrollList();

  if (scroll_to_show_view) {
    // Scroll the list so that |scroll_to_show_view| is in view.
    container()->ScrollRectToVisible(scroll_to_show_view->bounds());
  }
}

bool VPNListView::IsNetworkEntry(views::View* view,
                                 std::string* service_path) const {
  const auto& entry = network_view_service_path_map_.find(view);
  if (entry == network_view_service_path_map_.end())
    return false;
  *service_path = entry->second;
  return true;
}

void VPNListView::OnVPNProvidersChanged() {
  Update();
}

void VPNListView::OnViewClicked(views::View* sender) {
  const auto& provider_iter = provider_view_map_.find(sender);
  if (provider_iter != provider_view_map_.end()) {
    // If the user clicks on a provider entry, request that the "add network"
    // dialog for this provider be shown.
    const VPNProvider& provider = provider_iter->second;
    WmShell* shell = WmShell::Get();
    if (provider.third_party) {
      shell->RecordUserMetricsAction(
          UMA_STATUS_AREA_VPN_ADD_THIRD_PARTY_CLICKED);
      shell->system_tray_controller()->ShowThirdPartyVpnCreate(
          provider.extension_id);
    } else {
      shell->RecordUserMetricsAction(UMA_STATUS_AREA_VPN_ADD_BUILT_IN_CLICKED);
      shell->system_tray_controller()->ShowNetworkCreate(shill::kTypeVPN);
    }
    return;
  }

  // If the user clicked on a network entry, let the |delegate_| trigger a
  // connection attempt (if the network is currently disconnected) or show a
  // configuration dialog (if the network is currently connected or connecting).
  delegate_->OnNetworkEntryClicked(sender);
}

void VPNListView::AddNetwork(const chromeos::NetworkState* network) {
  views::View* entry(new VPNListNetworkEntry(this, network));
  container()->AddChildView(entry);
  network_view_service_path_map_[entry] = network->path();
  list_empty_ = false;
}

void VPNListView::AddProviderAndNetworks(
    const VPNProvider& vpn_provider,
    const chromeos::NetworkStateHandler::NetworkStateList& networks) {
  // Add a visual separator, unless this is the topmost entry in the list.
  if (!list_empty_) {
    views::Separator* const separator =
        new views::Separator(views::Separator::HORIZONTAL);
    separator->SetColor(kBorderLightColor);
    container()->AddChildView(separator);
  } else {
    list_empty_ = false;
  }
  std::string vpn_name =
      vpn_provider.third_party
          ? vpn_provider.third_party_provider_name
          : l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_VPN_BUILT_IN_PROVIDER);

  // Add a list entry for the VPN provider.
  views::View* provider_view = nullptr;
  if (UseMd()) {
    provider_view = new VPNListProviderEntryMd(
        this, vpn_name, IDS_ASH_STATUS_TRAY_ADD_CONNECTION);
  } else {
    provider_view = new VPNListProviderEntry(this, vpn_name);
  }
  container()->AddChildView(provider_view);
  provider_view_map_[provider_view] = vpn_provider;
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
      WmShell::Get()->vpn_list()->vpn_providers();

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

}  // namespace ash
