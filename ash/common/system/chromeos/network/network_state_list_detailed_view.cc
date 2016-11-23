// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/network_state_list_detailed_view.h"

#include <algorithm>
#include <vector>

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/chromeos/network/network_icon.h"
#include "ash/common/system/chromeos/network/network_icon_animation.h"
#include "ash/common/system/chromeos/network/network_info.h"
#include "ash/common/system/chromeos/network/network_list.h"
#include "ash/common/system/chromeos/network/network_list_md.h"
#include "ash/common/system/chromeos/network/network_list_view_base.h"
#include "ash/common/system/chromeos/network/tray_network_state_observer.h"
#include "ash/common/system/chromeos/network/vpn_list_view.h"
#include "ash/common/system/networking_config_delegate.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/fixed_sized_scroll_view.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/throbber_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/system/tray/tray_popup_label_button.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
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
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

using chromeos::DeviceState;
using chromeos::LoginState;
using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {
namespace {

bool UseMd() {
  return MaterialDesignController::IsSystemTrayMenuMaterial();
}

// Delay between scan requests.
const int kRequestScanDelaySeconds = 10;

// Create a label with the font size and color used in the network info bubble.
views::Label* CreateInfoBubbleLabel(const base::string16& text) {
  views::Label* label = new views::Label(text);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  label->SetFontList(rb.GetFontList(ui::ResourceBundle::SmallFont));
  label->SetEnabledColor(SkColorSetARGB(127, 0, 0, 0));
  return label;
}

// Create a row of labels for the network info bubble.
views::View* CreateInfoBubbleLine(const base::string16& text_label,
                                  const std::string& text_string) {
  views::View* view = new views::View;
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 1));
  view->AddChildView(CreateInfoBubbleLabel(text_label));
  view->AddChildView(CreateInfoBubbleLabel(base::UTF8ToUTF16(": ")));
  view->AddChildView(CreateInfoBubbleLabel(base::UTF8ToUTF16(text_string)));
  return view;
}

bool PolicyProhibitsUnmanaged() {
  if (!LoginState::IsInitialized() || !LoginState::Get()->IsUserLoggedIn())
    return false;
  bool policy_prohibites_unmanaged = false;
  const base::DictionaryValue* global_network_config =
      NetworkHandler::Get()
          ->managed_network_configuration_handler()
          ->GetGlobalConfigFromPolicy(
              std::string() /* no username hash, device policy */);
  if (global_network_config) {
    global_network_config->GetBooleanWithoutPathExpansion(
        ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
        &policy_prohibites_unmanaged);
  }
  return policy_prohibites_unmanaged;
}

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

// A bubble which displays network info.
class NetworkStateListDetailedView::InfoBubble
    : public views::BubbleDialogDelegateView {
 public:
  InfoBubble(views::View* anchor,
             views::View* content,
             NetworkStateListDetailedView* detailed_view)
      : views::BubbleDialogDelegateView(anchor, views::BubbleBorder::TOP_RIGHT),
        detailed_view_(detailed_view) {
    set_can_activate(false);
    SetLayoutManager(new views::FillLayout());
    AddChildView(content);
  }

  ~InfoBubble() override { detailed_view_->OnInfoBubbleDestroyed(); }

 private:
  // BubbleDialogDelegateView:
  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_NONE; }

  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override {
    DCHECK(anchor_widget());
    // Place the bubble in the anchor widget's root window.
    WmLookup::Get()
        ->GetWindowForWidget(anchor_widget())
        ->GetRootWindowController()
        ->ConfigureWidgetInitParamsForContainer(
            widget, kShellWindowId_SettingBubbleContainer, params);
    params->name = "NetworkStateListDetailedView::InfoBubble";
  }

  // Not owned.
  NetworkStateListDetailedView* detailed_view_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubble);
};

//------------------------------------------------------------------------------

const int kFadeIconMs = 500;

// A throbber view that fades in/out when shown/hidden.
class ScanningThrobber : public ThrobberView {
 public:
  ScanningThrobber() {
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(1.0);
    accessible_name_ =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_WIFI_SCANNING_MESSAGE);
  }
  ~ScanningThrobber() override {}

  // views::View
  void SetVisible(bool visible) override {
    layer()->GetAnimator()->StopAnimating();  // Stop any previous animation.
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kFadeIconMs));
    layer()->SetOpacity(visible ? 1.0 : 0.0);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(accessible_name_);
    node_data->role = ui::AX_ROLE_BUSY_INDICATOR;
  }

 private:
  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(ScanningThrobber);
};

//------------------------------------------------------------------------------

// An image button showing the info icon similar to TrayPopupHeaderButton,
// but without the toggle properties, that fades in/out when shown/hidden.
class InfoIcon : public views::ImageButton {
 public:
  explicit InfoIcon(views::ButtonListener* listener)
      : views::ImageButton(listener) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    SetImage(
        STATE_NORMAL,
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_INFO).ToImageSkia());
    SetImage(STATE_HOVERED,
             bundle.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_INFO_HOVER)
                 .ToImageSkia());
    SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
    SetAccessibleName(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_INFO));
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(1.0);
  }

  ~InfoIcon() override {}

  // views::View
  gfx::Size GetPreferredSize() const override {
    int size = GetTrayConstant(TRAY_POPUP_ITEM_MIN_HEIGHT);
    return gfx::Size(size, size);
  }

  void SetVisible(bool visible) override {
    layer()->GetAnimator()->StopAnimating();  // Stop any previous animation.
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kFadeIconMs));
    layer()->SetOpacity(visible ? 1.0 : 0.0);
  }

  // views::CustomButton
  void StateChanged() override {
    if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
      set_background(views::Background::CreateSolidBackground(
          kTrayPopupHoverBackgroundColor));
    } else {
      set_background(nullptr);
    }
    SchedulePaint();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InfoIcon);
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
      prev_wifi_scanning_state_(false),
      info_icon_(nullptr),
      button_wifi_(nullptr),
      button_mobile_(nullptr),
      other_wifi_(nullptr),
      turn_on_wifi_(nullptr),
      other_mobile_(nullptr),
      settings_(nullptr),
      proxy_settings_(nullptr),
      info_button_md_(nullptr),
      settings_button_md_(nullptr),
      proxy_settings_button_md_(nullptr),
      info_bubble_(nullptr),
      scanning_throbber_(nullptr) {
  if (list_type == LIST_TYPE_VPN) {
    // Use a specialized class to list VPNs.
    network_list_view_.reset(new VPNListView(this));
  } else {
    // Use a common class to list any other network types.
    // TODO(varkha): NetworkListViewMd is a temporary fork of NetworkListView.
    // NetworkListView will go away when Material Design becomes default.
    // See crbug.com/614453.
    if (UseMd())
      network_list_view_.reset(new NetworkListViewMd(this));
    else
      network_list_view_.reset(new NetworkListView(this));
  }
}

NetworkStateListDetailedView::~NetworkStateListDetailedView() {
  if (info_bubble_)
    info_bubble_->GetWidget()->CloseNow();
}

void NetworkStateListDetailedView::Update() {
  UpdateNetworkList();
  UpdateHeaderButtons();
  UpdateNetworkExtra();
  Layout();
}

// Overridden from NetworkDetailedView:

void NetworkStateListDetailedView::Init() {
  Reset();
  info_icon_ = nullptr;
  button_wifi_ = nullptr;
  button_mobile_ = nullptr;
  other_wifi_ = nullptr;
  turn_on_wifi_ = nullptr;
  other_mobile_ = nullptr;
  settings_ = nullptr;
  proxy_settings_ = nullptr;
  info_button_md_ = nullptr;
  settings_button_md_ = nullptr;
  proxy_settings_button_md_ = nullptr;
  scanning_throbber_ = nullptr;

  CreateScrollableList();
  if (!UseMd())
    CreateNetworkExtra();
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
  if (UseMd()) {
    if (sender == info_button_md_) {
      ToggleInfoBubble();
      return;
    } else if (sender == settings_button_md_) {
      ShowSettings();
    } else if (sender == proxy_settings_button_md_) {
      WmShell::Get()->system_tray_controller()->ShowProxySettings();
    }

    if (owner()->system_tray())
      owner()->system_tray()->CloseSystemBubble();
    return;
  }

  if (sender == info_icon_) {
    ToggleInfoBubble();
    return;
  }

  // If the info bubble was visible, close it when some other item is clicked.
  ResetInfoBubble();
  bool close_bubble = false;
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  if (sender == button_wifi_) {
    bool enabled = handler->IsTechnologyEnabled(NetworkTypePattern::WiFi());
    handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(), !enabled,
                                  chromeos::network_handler::ErrorCallback());
  } else if (sender == turn_on_wifi_) {
    handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(), true,
                                  chromeos::network_handler::ErrorCallback());
  } else if (sender == button_mobile_) {
    ToggleMobile();
  } else if (sender == settings_) {
    ShowSettings();
    close_bubble = true;
  } else if (sender == proxy_settings_) {
    WmShell::Get()->system_tray_controller()->ShowProxySettings();
    close_bubble = true;
  } else if (sender == other_mobile_) {
    WmShell::Get()->system_tray_controller()->ShowNetworkCreate(
        shill::kTypeCellular);
    close_bubble = true;
  } else if (sender == other_wifi_) {
    OnOtherWifiClicked();
    close_bubble = true;
  } else {
    NOTREACHED();
  }
  if (close_bubble && owner()->system_tray())
    owner()->system_tray()->CloseSystemBubble();
}

void NetworkStateListDetailedView::HandleViewClicked(views::View* view) {
  // If the info bubble was visible, close it when some other item is clicked.
  ResetInfoBubble();

  if (login_ == LoginStatus::LOCKED)
    return;

  std::string service_path;
  if (!network_list_view_->IsNetworkEntry(view, &service_path))
    return;

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network || network->IsConnectedState() || network->IsConnectingState()) {
    WmShell::Get()->RecordUserMetricsAction(
        list_type_ == LIST_TYPE_VPN
            ? UMA_STATUS_AREA_SHOW_NETWORK_CONNECTION_DETAILS
            : UMA_STATUS_AREA_SHOW_VPN_CONNECTION_DETAILS);
    WmShell::Get()->system_tray_controller()->ShowNetworkSettings(
        network ? network->guid() : std::string());
  } else {
    WmShell::Get()->RecordUserMetricsAction(
        list_type_ == LIST_TYPE_VPN
            ? UMA_STATUS_AREA_CONNECT_TO_VPN
            : UMA_STATUS_AREA_CONNECT_TO_CONFIGURED_NETWORK);
    chromeos::NetworkConnect::Get()->ConnectToNetworkId(network->guid());
  }
  owner()->system_tray()->CloseSystemBubble();
}

void NetworkStateListDetailedView::CreateExtraTitleRowButtons() {
  if (UseMd()) {
    if (login_ == LoginStatus::LOCKED)
      return;

    DCHECK(!info_button_md_);
    tri_view()->SetContainerVisible(TriView::Container::END, true);

    info_button_md_ = new SystemMenuButton(
        this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuInfoIcon,
        IDS_ASH_STATUS_TRAY_NETWORK_INFO);
    tri_view()->AddView(TriView::Container::END, info_button_md_);

    if (login_ != LoginStatus::NOT_LOGGED_IN) {
      DCHECK(!settings_button_md_);
      settings_button_md_ = new SystemMenuButton(
          this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuSettingsIcon,
          IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS);

      // Allow the user to access settings only if user is logged in
      // and showing settings is allowed. There are situations (supervised user
      // creation flow) when session is started but UI flow continues within
      // login UI, i.e., no browser window is yet avaialable.
      if (!WmShell::Get()->system_tray_delegate()->ShouldShowSettings())
        settings_button_md_->SetState(views::Button::STATE_DISABLED);

      tri_view()->AddView(TriView::Container::END, settings_button_md_);
    } else {
      proxy_settings_button_md_ = new SystemMenuButton(
          this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuSettingsIcon,
          IDS_ASH_STATUS_TRAY_NETWORK_PROXY_SETTINGS);
      tri_view()->AddView(TriView::Container::END, proxy_settings_button_md_);
    }

    return;
  }

  if (list_type_ != LIST_TYPE_VPN) {
    NetworkStateHandler* network_state_handler =
        NetworkHandler::Get()->network_state_handler();
    button_wifi_ = new TrayPopupHeaderButton(
        this, IDR_AURA_UBER_TRAY_WIFI_ENABLED, IDR_AURA_UBER_TRAY_WIFI_DISABLED,
        IDR_AURA_UBER_TRAY_WIFI_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_WIFI_DISABLED_HOVER, IDS_ASH_STATUS_TRAY_WIFI);
    button_wifi_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_WIFI));
    button_wifi_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_WIFI));
    title_row()->AddViewToRowNonMd(button_wifi_, true);
    if (network_state_handler->IsTechnologyProhibited(
            NetworkTypePattern::WiFi())) {
      button_wifi_->SetState(views::Button::STATE_DISABLED);
      button_wifi_->SetToggledTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_TECHNOLOGY_ENFORCED_BY_POLICY));
    }

    button_mobile_ =
        new TrayPopupHeaderButton(this, IDR_AURA_UBER_TRAY_CELLULAR_ENABLED,
                                  IDR_AURA_UBER_TRAY_CELLULAR_DISABLED,
                                  IDR_AURA_UBER_TRAY_CELLULAR_ENABLED_HOVER,
                                  IDR_AURA_UBER_TRAY_CELLULAR_DISABLED_HOVER,
                                  IDS_ASH_STATUS_TRAY_CELLULAR);
    button_mobile_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_MOBILE));
    button_mobile_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_MOBILE));
    if (network_state_handler->IsTechnologyProhibited(
            NetworkTypePattern::Cellular())) {
      button_mobile_->SetState(views::Button::STATE_DISABLED);
      button_mobile_->SetToggledTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_TECHNOLOGY_ENFORCED_BY_POLICY));
    }
    title_row()->AddViewToRowNonMd(button_mobile_, true);
  }

  views::View* info_throbber_container = new views::View();
  InfoThrobberLayout* info_throbber_layout = new InfoThrobberLayout;
  info_throbber_container->SetLayoutManager(info_throbber_layout);
  title_row()->AddViewToRowNonMd(info_throbber_container, true);

  if (list_type_ != LIST_TYPE_VPN) {
    // Place the throbber behind the info icon so that the icon receives
    // click / touch events. The info icon is hidden when the throbber is
    // active.
    scanning_throbber_ = new ScanningThrobber();
    info_throbber_container->AddChildView(scanning_throbber_);
  }

  info_icon_ = new InfoIcon(this);
  info_icon_->SetFocusBehavior(FocusBehavior::ALWAYS);
  info_icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_INFO));
  info_throbber_container->AddChildView(info_icon_);
}

void NetworkStateListDetailedView::ShowSettings() {
  WmShell::Get()->RecordUserMetricsAction(
      list_type_ == LIST_TYPE_VPN ? UMA_STATUS_AREA_VPN_SETTINGS_OPENED
                                  : UMA_STATUS_AREA_NETWORK_SETTINGS_OPENED);
  WmShell::Get()->system_tray_controller()->ShowNetworkSettings(std::string());
}

void NetworkStateListDetailedView::CreateNetworkExtra() {
  DCHECK(!UseMd());
  if (login_ == LoginStatus::LOCKED)
    return;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::View* bottom_row = new views::View();
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, kTrayMenuBottomRowPadding,
      kTrayMenuBottomRowPadding, kTrayMenuBottomRowPaddingBetweenItems);
  layout->SetDefaultFlex(1);
  bottom_row->SetLayoutManager(layout);

  if (list_type_ != LIST_TYPE_VPN) {
    other_wifi_ = new TrayPopupLabelButton(
        this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_WIFI));
    bottom_row->AddChildView(other_wifi_);
    if (PolicyProhibitsUnmanaged()) {
      other_wifi_->SetEnabled(false);
      other_wifi_->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_PROHIBITED_OTHER));
    }

    turn_on_wifi_ = new TrayPopupLabelButton(
        this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_TURN_ON_WIFI));
    if (NetworkHandler::Get()->network_state_handler()->IsTechnologyProhibited(
            NetworkTypePattern::WiFi())) {
      turn_on_wifi_->SetState(views::Button::STATE_DISABLED);
      turn_on_wifi_->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_TECHNOLOGY_ENFORCED_BY_POLICY));
    }
    bottom_row->AddChildView(turn_on_wifi_);

    other_mobile_ = new TrayPopupLabelButton(
        this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_MOBILE));
    bottom_row->AddChildView(other_mobile_);
  }

  CreateSettingsEntry();

  // Both settings_ and proxy_settings_ can be null. This happens when
  // we're logged in but showing settings page is not enabled.
  // Example: supervised user creation flow where user session is active
  // but all action happens on the login window.
  // Allowing opening proxy settigns dialog will break assumption in
  //  SystemTrayDelegateChromeOS::ChangeProxySettings(), see CHECK.
  if (settings_ || proxy_settings_)
    bottom_row->AddChildView(settings_ ? settings_ : proxy_settings_);

  AddChildView(bottom_row);
}

void NetworkStateListDetailedView::UpdateHeaderButtons() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  if (button_wifi_)
    UpdateTechnologyButton(button_wifi_, NetworkTypePattern::WiFi());
  if (button_mobile_)
    UpdateTechnologyButton(button_mobile_, NetworkTypePattern::Mobile());
  if (proxy_settings_)
    proxy_settings_->SetEnabled(handler->DefaultNetwork() != nullptr);

  if (list_type_ != LIST_TYPE_VPN && !UseMd()) {
    // Update Wifi Scanning throbber.
    bool scanning =
        NetworkHandler::Get()->network_state_handler()->GetScanningByType(
            NetworkTypePattern::WiFi());
    if (scanning != prev_wifi_scanning_state_) {
      prev_wifi_scanning_state_ = scanning;
      if (scanning) {
        SetScanningStateForThrobberView(true);

        // Start animation on the |scanning_throbber_| indicator.
        scanning_throbber_->Start();

        // Since the |scanning_throbber_| view is behind the |info_icon_|
        // view, the tooltip text for |info_icon_| will be used for both.
        info_icon_->SetTooltipText(l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_WIFI_SCANNING_MESSAGE));
      } else {
        SetScanningStateForThrobberView(false);

        // Stop animation on the |scanning_throbber_| indicator.
        scanning_throbber_->Stop();
        info_icon_->SetTooltipText(
            l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_INFO));
      }
    }
  }

  if (!UseMd())
    static_cast<views::View*>(title_row())->Layout();
}

void NetworkStateListDetailedView::SetScanningStateForThrobberView(
    bool is_scanning) {
  if (UseMd())
    return;

  // Hide the network info button if the device is scanning for Wi-Fi networks
  // and display the WiFi scanning indicator.
  info_icon_->SetVisible(!is_scanning);
  scanning_throbber_->SetVisible(is_scanning);
  // Set the element, network info button or the wifi scanning indicator, as
  // focusable based on which one is active/visible.
  // NOTE: As we do not want to lose focus from the network info throbber view,
  // the order of below operation is important.
  if (is_scanning) {
    scanning_throbber_->SetFocusBehavior(FocusBehavior::ALWAYS);
    info_icon_->SetFocusBehavior(FocusBehavior::NEVER);
  } else {
    info_icon_->SetFocusBehavior(FocusBehavior::ALWAYS);
    scanning_throbber_->SetFocusBehavior(FocusBehavior::NEVER);
  }
  // If the Network Info view was in focus while this toggle operation was
  // being performed then the focus should remain on this view.
  if (info_icon_->HasFocus() && is_scanning)
    scanning_throbber_->RequestFocus();
  else if (scanning_throbber_->HasFocus() && !is_scanning)
    info_icon_->RequestFocus();
}

void NetworkStateListDetailedView::UpdateTechnologyButton(
    TrayPopupHeaderButton* button,
    const NetworkTypePattern& technology) {
  NetworkStateHandler::TechnologyState state =
      NetworkHandler::Get()->network_state_handler()->GetTechnologyState(
          technology);
  if (state == NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
    button->SetVisible(false);
    return;
  }
  button->SetVisible(true);
  if (state == NetworkStateHandler::TECHNOLOGY_AVAILABLE) {
    button->SetEnabled(true);
    button->SetToggled(true);
  } else if (state == NetworkStateHandler::TECHNOLOGY_ENABLED) {
    button->SetEnabled(true);
    button->SetToggled(false);
  } else if (state == NetworkStateHandler::TECHNOLOGY_ENABLING) {
    button->SetEnabled(false);
    button->SetToggled(false);
  } else {  // Initializing
    button->SetEnabled(false);
    button->SetToggled(true);
  }
}

void NetworkStateListDetailedView::UpdateNetworkList() {
  network_list_view_->Update();
}

bool NetworkStateListDetailedView::OrderChild(views::View* view, int index) {
  if (scroll_content()->child_at(index) != view) {
    scroll_content()->ReorderChildView(view, index);
    return true;
  }
  return false;
}

void NetworkStateListDetailedView::UpdateNetworkExtra() {
  if (login_ == LoginStatus::LOCKED)
    return;

  View* layout_parent = nullptr;  // All these buttons have the same parent.
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  if (other_wifi_) {
    DCHECK(turn_on_wifi_);
    NetworkStateHandler::TechnologyState state =
        handler->GetTechnologyState(NetworkTypePattern::WiFi());
    if (state == NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
      turn_on_wifi_->SetVisible(false);
      other_wifi_->SetVisible(false);
    } else {
      if (state == NetworkStateHandler::TECHNOLOGY_AVAILABLE) {
        turn_on_wifi_->SetVisible(true);
        turn_on_wifi_->SetEnabled(true);
        other_wifi_->SetVisible(false);
      } else if (state == NetworkStateHandler::TECHNOLOGY_ENABLED) {
        turn_on_wifi_->SetVisible(false);
        other_wifi_->SetVisible(true);
      } else {
        // Initializing or Enabling
        turn_on_wifi_->SetVisible(true);
        turn_on_wifi_->SetEnabled(false);
        other_wifi_->SetVisible(false);
      }
    }
    layout_parent = other_wifi_->parent();
  }

  if (other_mobile_) {
    bool show_other_mobile = false;
    NetworkStateHandler::TechnologyState state =
        handler->GetTechnologyState(NetworkTypePattern::Mobile());
    if (state != NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
      const chromeos::DeviceState* device =
          handler->GetDeviceStateByType(NetworkTypePattern::Mobile());
      show_other_mobile = (device && device->support_network_scan());
    }
    if (show_other_mobile) {
      other_mobile_->SetVisible(true);
      other_mobile_->SetEnabled(state ==
                                NetworkStateHandler::TECHNOLOGY_ENABLED);
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
  DCHECK(!UseMd());
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  bool show_settings =
      WmShell::Get()->system_tray_delegate()->ShouldShowSettings();
  if (login_ != LoginStatus::NOT_LOGGED_IN) {
    // Allow user access settings only if user is logged in
    // and showing settings is allowed. There're situations (supervised user
    // creation flow) when session is started but UI flow continues within
    // login UI i.e. no browser window is yet avaialable.
    if (show_settings) {
      settings_ = new TrayPopupLabelButton(
          this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS));
      if (list_type_ == LIST_TYPE_VPN)
        settings_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
  } else {
    // Allow users to change proxy settings only when not logged in.
    proxy_settings_ = new TrayPopupLabelButton(
        this,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_PROXY_SETTINGS));
  }
}

void NetworkStateListDetailedView::ToggleInfoBubble() {
  if (ResetInfoBubble())
    return;

  info_bubble_ = new InfoBubble(UseMd() ? info_button_md_ : info_icon_,
                                CreateNetworkInfoView(), this);
  views::BubbleDialogDelegateView::CreateBubble(info_bubble_)->Show();
  info_bubble_->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, false);
}

bool NetworkStateListDetailedView::ResetInfoBubble() {
  if (!info_bubble_)
    return false;
  info_bubble_->GetWidget()->Close();
  info_bubble_ = nullptr;
  return true;
}

void NetworkStateListDetailedView::OnInfoBubbleDestroyed() {
  info_bubble_ = nullptr;
}

views::View* NetworkStateListDetailedView::CreateNetworkInfoView() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
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

  views::View* container = new views::View;
  container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
  container->SetBorder(views::CreateEmptyBorder(0, 5, 0, 5));

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

  if (!ip_address.empty()) {
    container->AddChildView(CreateInfoBubbleLine(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_IP), ip_address));
  }
  if (!ipv6_address.empty()) {
    container->AddChildView(CreateInfoBubbleLine(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_IPV6), ipv6_address));
  }
  if (!ethernet_address.empty()) {
    container->AddChildView(CreateInfoBubbleLine(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_ETHERNET),
        ethernet_address));
  }
  if (!wifi_address.empty()) {
    container->AddChildView(CreateInfoBubbleLine(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_WIFI), wifi_address));
  }
  if (!vpn_address.empty()) {
    container->AddChildView(CreateInfoBubbleLine(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_VPN), vpn_address));
  }

  // Avoid an empty bubble in the unlikely event that there is no network
  // information at all.
  if (!container->has_children()) {
    container->AddChildView(CreateInfoBubbleLabel(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_NO_NETWORKS)));
  }

  return container;
}

const gfx::ImageSkia*
NetworkStateListDetailedView::GetControlledByExtensionIcon() {
  // Lazily load the icon from the resource bundle.
  if (controlled_by_extension_icon_.IsEmpty()) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    controlled_by_extension_icon_ =
        rb.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_CONTROLLED);
  }
  DCHECK(!controlled_by_extension_icon_.IsEmpty());
  return controlled_by_extension_icon_.ToImageSkia();
}

views::View* NetworkStateListDetailedView::CreateControlledByExtensionView(
    const NetworkInfo& info) {
  NetworkingConfigDelegate* networking_config_delegate =
      WmShell::Get()->system_tray_delegate()->GetNetworkingConfigDelegate();
  if (!networking_config_delegate)
    return nullptr;
  std::unique_ptr<const NetworkingConfigDelegate::ExtensionInfo>
      extension_info = networking_config_delegate->LookUpExtensionForNetwork(
          info.service_path);
  if (!extension_info)
    return nullptr;

  // Get the tooltip text.
  base::string16 tooltip_text = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_EXTENSION_CONTROLLED_WIFI,
      base::UTF8ToUTF16(extension_info->extension_name));

  views::ImageView* controlled_icon =
      new FixedSizedImageView(kTrayPopupDetailsIconWidth, 0);

  controlled_icon->SetImage(GetControlledByExtensionIcon());
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

void NetworkStateListDetailedView::ToggleMobile() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  bool enabled = handler->IsTechnologyEnabled(NetworkTypePattern::Mobile());
  chromeos::NetworkConnect::Get()->SetTechnologyEnabled(
      NetworkTypePattern::Mobile(), !enabled);
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
  if (!UseMd()) {
    container->SetBorder(
        views::CreateEmptyBorder(0, kTrayPopupPaddingHorizontal, 0, 0));
  }
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
  WmShell::Get()->system_tray_controller()->ShowNetworkCreate(shill::kTypeWifi);
}

void NetworkStateListDetailedView::RelayoutScrollList() {
  scroller()->Layout();
}

}  // namespace tray
}  // namespace ash
