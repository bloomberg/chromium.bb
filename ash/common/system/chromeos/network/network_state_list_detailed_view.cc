// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/network_state_list_detailed_view.h"

#include <algorithm>
#include <vector>

#include "ash/common/ash_constants.h"
#include "ash/common/system/chromeos/network/network_icon.h"
#include "ash/common/system/chromeos/network/network_icon_animation.h"
#include "ash/common/system/chromeos/network/network_info.h"
#include "ash/common/system/chromeos/network/network_list_md.h"
#include "ash/common/system/chromeos/network/network_list_view_base.h"
#include "ash/common/system/chromeos/network/tray_network_state_observer.h"
#include "ash/common/system/chromeos/network/vpn_list_view.h"
#include "ash/common/system/networking_config_delegate.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/throbber_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

using chromeos::DeviceState;
using chromeos::LoginState;
using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {
namespace {

// Delay between scan requests.
const int kRequestScanDelaySeconds = 10;

// TODO(varkha): Consolidate with a similar method in tray_bluetooth.cc.
void SetupConnectedItemMd(HoverHighlightView* container,
                          const base::string16& text,
                          const gfx::ImageSkia& image) {
  container->AddIconAndLabels(
      image, text,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::CAPTION);
  style.set_color_style(TrayPopupItemStyle::ColorStyle::CONNECTED);
  style.SetupLabel(container->sub_text_label());
}

// TODO(varkha): Consolidate with a similar method in tray_bluetooth.cc.
void SetupConnectingItemMd(HoverHighlightView* container,
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

//------------------------------------------------------------------------------

// This margin value is used throughout the bubble:
// - margins inside the border
// - horizontal spacing between bubble border and parent bubble border
// - distance between top of this bubble's border and the bottom of the anchor
//   view (horizontal rule).
int kBubbleMargin = 8;

// A bubble which displays network info.
class NetworkStateListDetailedView::InfoBubble
    : public views::BubbleDialogDelegateView {
 public:
  InfoBubble(views::View* anchor,
             views::View* content,
             NetworkStateListDetailedView* detailed_view)
      : views::BubbleDialogDelegateView(anchor, views::BubbleBorder::TOP_RIGHT),
        detailed_view_(detailed_view) {
    set_margins(gfx::Insets(kBubbleMargin));
    set_arrow(views::BubbleBorder::NONE);
    set_shadow(views::BubbleBorder::NO_ASSETS);
    set_anchor_view_insets(gfx::Insets(0, 0, kBubbleMargin, 0));
    set_notify_enter_exit_on_child(true);
    set_close_on_deactivate(true);
    SetLayoutManager(new views::FillLayout());
    AddChildView(content);
  }

  ~InfoBubble() override { detailed_view_->OnInfoBubbleDestroyed(); }

 private:
  // View:
  gfx::Size GetPreferredSize() const override {
    // This bubble should be inset by kBubbleMargin on both left and right
    // relative to the parent bubble.
    const gfx::Size anchor_size = GetAnchorView()->size();
    int contents_width =
        anchor_size.width() - 2 * kBubbleMargin - margins().width();
    return gfx::Size(contents_width, GetHeightForWidth(contents_width));
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    // Like the user switching bubble/menu, hide the bubble when the mouse
    // exits.
    detailed_view_->ResetInfoBubble();
  }

  // BubbleDialogDelegateView:
  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_NONE; }

  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override {
    params->shadow_type = views::Widget::InitParams::SHADOW_TYPE_DROP;
    params->shadow_elevation = ::wm::ShadowElevation::TINY;
    params->name = "NetworkStateListDetailedView::InfoBubble";
  }

  // Not owned.
  NetworkStateListDetailedView* detailed_view_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubble);
};

//------------------------------------------------------------------------------

// Special layout to overlap the scanning throbber and the info button.
class InfoThrobberLayout : public views::LayoutManager {
 public:
  InfoThrobberLayout() {}
  ~InfoThrobberLayout() override {}

  // views::LayoutManager
  void Layout(views::View* host) override {
    gfx::Size max_size(GetMaxChildSize(host));
    // Center each child view within |max_size|.
    for (int i = 0; i < host->child_count(); ++i) {
      views::View* child = host->child_at(i);
      if (!child->visible())
        continue;
      gfx::Size child_size = child->GetPreferredSize();
      gfx::Point origin;
      origin.set_x((max_size.width() - child_size.width()) / 2);
      origin.set_y((max_size.height() - child_size.height()) / 2);
      gfx::Rect bounds(origin, child_size);
      bounds.Inset(-host->GetInsets());
      child->SetBoundsRect(bounds);
    }
  }

  gfx::Size GetPreferredSize(const views::View* host) const override {
    gfx::Point origin;
    gfx::Rect rect(origin, GetMaxChildSize(host));
    rect.Inset(-host->GetInsets());
    return rect.size();
  }

 private:
  gfx::Size GetMaxChildSize(const views::View* host) const {
    int width = 0, height = 0;
    for (int i = 0; i < host->child_count(); ++i) {
      const views::View* child = host->child_at(i);
      if (!child->visible())
        continue;
      gfx::Size child_size = child->GetPreferredSize();
      width = std::max(width, child_size.width());
      height = std::max(height, child_size.width());
    }
    return gfx::Size(width, height);
  }

  DISALLOW_COPY_AND_ASSIGN(InfoThrobberLayout);
};

//------------------------------------------------------------------------------
// NetworkStateListDetailedView

NetworkStateListDetailedView::NetworkStateListDetailedView(
    SystemTrayItem* owner,
    ListType list_type,
    LoginStatus login)
    : NetworkDetailedView(owner),
      list_type_(list_type),
      login_(login),
      info_button_(nullptr),
      settings_button_(nullptr),
      proxy_settings_button_(nullptr),
      info_bubble_(nullptr) {
  if (list_type == LIST_TYPE_VPN) {
    // Use a specialized class to list VPNs.
    network_list_view_.reset(new VPNListView(this));
  } else {
    // Use a common class to list any other network types.
    network_list_view_.reset(new NetworkListView(this));
  }
}

NetworkStateListDetailedView::~NetworkStateListDetailedView() {
  ResetInfoBubble();
}

void NetworkStateListDetailedView::Update() {
  UpdateNetworkList();
  UpdateHeaderButtons();
  Layout();
}

// Overridden from NetworkDetailedView:

void NetworkStateListDetailedView::Init() {
  Reset();
  info_button_ = nullptr;
  settings_button_ = nullptr;
  proxy_settings_button_ = nullptr;

  CreateScrollableList();
  CreateTitleRow(list_type_ == ListType::LIST_TYPE_NETWORK
                     ? IDS_ASH_STATUS_TRAY_NETWORK
                     : IDS_ASH_STATUS_TRAY_VPN);

  network_list_view_->set_container(scroll_content());
  Update();

  if (list_type_ != LIST_TYPE_VPN)
    CallRequestScan();
}

NetworkDetailedView::DetailedViewType
NetworkStateListDetailedView::GetViewType() const {
  return STATE_LIST_VIEW;
}

void NetworkStateListDetailedView::HandleButtonPressed(views::Button* sender,
                                                       const ui::Event& event) {
  if (sender == info_button_) {
    ToggleInfoBubble();
    return;
  }

  if (sender == settings_button_)
    ShowSettings();
  else if (sender == proxy_settings_button_)
    Shell::Get()->system_tray_controller()->ShowProxySettings();

  if (owner()->system_tray())
    owner()->system_tray()->CloseSystemBubble();
}

void NetworkStateListDetailedView::HandleViewClicked(views::View* view) {
  if (login_ == LoginStatus::LOCKED)
    return;

  std::string guid;
  if (!network_list_view_->IsNetworkEntry(view, &guid))
    return;

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network || network->IsConnectedState() || network->IsConnectingState()) {
    WmShell::Get()->RecordUserMetricsAction(
        list_type_ == LIST_TYPE_VPN
            ? UMA_STATUS_AREA_SHOW_VPN_CONNECTION_DETAILS
            : UMA_STATUS_AREA_SHOW_NETWORK_CONNECTION_DETAILS);
    Shell::Get()->system_tray_controller()->ShowNetworkSettings(
        network ? network->guid() : std::string());
  } else {
    WmShell::Get()->RecordUserMetricsAction(
        list_type_ == LIST_TYPE_VPN
            ? UMA_STATUS_AREA_CONNECT_TO_VPN
            : UMA_STATUS_AREA_CONNECT_TO_CONFIGURED_NETWORK);
    chromeos::NetworkConnect::Get()->ConnectToNetworkId(network->guid());
  }
}

void NetworkStateListDetailedView::CreateExtraTitleRowButtons() {
  if (login_ == LoginStatus::LOCKED)
    return;

  DCHECK(!info_button_);
  tri_view()->SetContainerVisible(TriView::Container::END, true);

  info_button_ = new SystemMenuButton(
      this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuInfoIcon,
      IDS_ASH_STATUS_TRAY_NETWORK_INFO);
  tri_view()->AddView(TriView::Container::END, info_button_);

  if (login_ != LoginStatus::NOT_LOGGED_IN) {
    DCHECK(!settings_button_);
    settings_button_ = new SystemMenuButton(
        this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuSettingsIcon,
        IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS);

    // Allow the user to access settings only if user is logged in
    // and showing settings is allowed. There are situations (supervised user
    // creation flow) when session is started but UI flow continues within
    // login UI, i.e., no browser window is yet avaialable.
    if (!Shell::Get()->system_tray_delegate()->ShouldShowSettings())
      settings_button_->SetEnabled(false);

    tri_view()->AddView(TriView::Container::END, settings_button_);
  } else {
    proxy_settings_button_ = new SystemMenuButton(
        this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuSettingsIcon,
        IDS_ASH_STATUS_TRAY_NETWORK_PROXY_SETTINGS);
    tri_view()->AddView(TriView::Container::END, proxy_settings_button_);
  }
}

void NetworkStateListDetailedView::ShowSettings() {
  WmShell::Get()->RecordUserMetricsAction(
      list_type_ == LIST_TYPE_VPN ? UMA_STATUS_AREA_VPN_SETTINGS_OPENED
                                  : UMA_STATUS_AREA_NETWORK_SETTINGS_OPENED);
  Shell::Get()->system_tray_controller()->ShowNetworkSettings(std::string());
}

void NetworkStateListDetailedView::UpdateNetworkList() {
  network_list_view_->Update();
}

void NetworkStateListDetailedView::UpdateHeaderButtons() {
  if (proxy_settings_button_) {
    proxy_settings_button_->SetEnabled(
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork() !=
        nullptr);
  }
  if (list_type_ != LIST_TYPE_VPN) {
    bool scanning =
        NetworkHandler::Get()->network_state_handler()->GetScanningByType(
            NetworkTypePattern::WiFi());
    ShowProgress(-1, scanning);
    info_button_->SetTooltipText(l10n_util::GetStringUTF16(
        scanning ? IDS_ASH_STATUS_TRAY_WIFI_SCANNING_MESSAGE
                 : IDS_ASH_STATUS_TRAY_NETWORK_INFO));
  }
}

void NetworkStateListDetailedView::ToggleInfoBubble() {
  if (ResetInfoBubble())
    return;

  info_bubble_ = new InfoBubble(tri_view(), CreateNetworkInfoView(), this);
  views::BubbleDialogDelegateView::CreateBubble(info_bubble_)->Show();
  info_bubble_->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, false);
}

bool NetworkStateListDetailedView::ResetInfoBubble() {
  if (!info_bubble_)
    return false;
  // After losing activation, the InfoBubble will be closed.
  owner()
      ->system_tray()
      ->GetSystemBubble()
      ->bubble_view()
      ->GetWidget()
      ->Activate();
  return true;
}

void NetworkStateListDetailedView::OnInfoBubbleDestroyed() {
  info_bubble_ = nullptr;
}

views::View* NetworkStateListDetailedView::CreateNetworkInfoView() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  std::string ip_address, ipv6_address;
  const NetworkState* network = handler->DefaultNetwork();
  if (network) {
    const DeviceState* device = handler->GetDeviceState(network->device_path());
    if (device) {
      ip_address = device->GetIpAddressByType(shill::kTypeIPv4);
      ipv6_address = device->GetIpAddressByType(shill::kTypeIPv6);
    }
  }

  std::string ethernet_address, wifi_address, vpn_address;
  if (list_type_ != LIST_TYPE_VPN) {
    ethernet_address = handler->FormattedHardwareAddressForType(
        NetworkTypePattern::Ethernet());
    wifi_address =
        handler->FormattedHardwareAddressForType(NetworkTypePattern::WiFi());
  } else {
    vpn_address =
        handler->FormattedHardwareAddressForType(NetworkTypePattern::VPN());
  }

  base::string16 bubble_text;
  auto add_line = [&bubble_text](const std::string& address, int ids) {
    if (!address.empty()) {
      if (!bubble_text.empty())
        bubble_text += base::ASCIIToUTF16("\n");

      bubble_text +=
          l10n_util::GetStringFUTF16(ids, base::UTF8ToUTF16(address));
    }
  };

  add_line(ip_address, IDS_ASH_STATUS_TRAY_IP);
  add_line(ipv6_address, IDS_ASH_STATUS_TRAY_IPV6);
  add_line(ethernet_address, IDS_ASH_STATUS_TRAY_ETHERNET);
  add_line(wifi_address, IDS_ASH_STATUS_TRAY_WIFI);
  add_line(vpn_address, IDS_ASH_STATUS_TRAY_VPN);

  // Avoid an empty bubble in the unlikely event that there is no network
  // information at all.
  if (bubble_text.empty())
    bubble_text = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_NETWORKS);

  auto* label = new views::Label(bubble_text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  label->SetSelectable(true);
  return label;
}

views::View* NetworkStateListDetailedView::CreateControlledByExtensionView(
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

  // Get the tooltip text.
  base::string16 tooltip_text = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_EXTENSION_CONTROLLED_WIFI,
      base::UTF8ToUTF16(extension_info->extension_name));

  views::ImageView* controlled_icon =
      new FixedSizedImageView(kTrayPopupDetailsIconWidth, 0);

  controlled_icon->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AURA_UBER_TRAY_NETWORK_CONTROLLED));
  controlled_icon->SetTooltipText(tooltip_text);
  return controlled_icon;
}

void NetworkStateListDetailedView::CallRequestScan() {
  VLOG(1) << "Requesting Network Scan.";
  NetworkHandler::Get()->network_state_handler()->RequestScan();
  // Periodically request a scan while this UI is open.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NetworkStateListDetailedView::CallRequestScan, AsWeakPtr()),
      base::TimeDelta::FromSeconds(kRequestScanDelaySeconds));
}

views::View* NetworkStateListDetailedView::CreateViewForNetwork(
    const NetworkInfo& info) {
  HoverHighlightView* container = new HoverHighlightView(this);
  if (info.connected)
    SetupConnectedItemMd(container, info.label, info.image);
  else if (info.connecting)
    SetupConnectingItemMd(container, info.label, info.image);
  else
    container->AddIconAndLabel(info.image, info.label, info.highlight);
  container->set_tooltip(info.tooltip);
  views::View* controlled_icon = CreateControlledByExtensionView(info);
  if (controlled_icon)
    container->AddChildView(controlled_icon);
  return container;
}

bool NetworkStateListDetailedView::IsViewHovered(views::View* view) {
  return static_cast<HoverHighlightView*>(view)->hover();
}

NetworkTypePattern NetworkStateListDetailedView::GetNetworkTypePattern() const {
  return list_type_ == LIST_TYPE_VPN ? NetworkTypePattern::VPN()
                                     : NetworkTypePattern::NonVirtual();
}

void NetworkStateListDetailedView::UpdateViewForNetwork(
    views::View* view,
    const NetworkInfo& info) {
  HoverHighlightView* container = static_cast<HoverHighlightView*>(view);
  DCHECK(!container->has_children());
  if (info.connected)
    SetupConnectedItemMd(container, info.label, info.image);
  else if (info.connecting)
    SetupConnectingItemMd(container, info.label, info.image);
  else
    container->AddIconAndLabel(info.image, info.label, info.highlight);
  views::View* controlled_icon = CreateControlledByExtensionView(info);
  container->set_tooltip(info.tooltip);
  if (controlled_icon)
    view->AddChildView(controlled_icon);
}

views::Label* NetworkStateListDetailedView::CreateInfoLabel() {
  views::Label* label = new views::Label();
  label->SetBorder(views::CreateEmptyBorder(kTrayPopupPaddingBetweenItems,
                                            kTrayPopupPaddingHorizontal,
                                            kTrayPopupPaddingBetweenItems, 0));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SkColorSetARGB(192, 0, 0, 0));
  return label;
}

void NetworkStateListDetailedView::OnNetworkEntryClicked(views::View* sender) {
  HandleViewClicked(sender);
}

void NetworkStateListDetailedView::OnOtherWifiClicked() {
  WmShell::Get()->RecordUserMetricsAction(
      UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED);
  Shell::Get()->system_tray_controller()->ShowNetworkCreate(shill::kTypeWifi);
}

void NetworkStateListDetailedView::RelayoutScrollList() {
  scroller()->Layout();
}

}  // namespace tray
}  // namespace ash
