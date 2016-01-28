// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_state_list_detailed_view.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/networking_config_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/chromeos/network/tray_network_state_observer.h"
#include "ash/system/chromeos/network/vpn_list_view.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/fixed_sized_scroll_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/throbber_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_popup_header_button.h"
#include "ash/system/tray/tray_popup_label_button.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "grit/ui_chromeos_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/network/network_connect.h"
#include "ui/chromeos/network/network_icon.h"
#include "ui/chromeos/network/network_icon_animation.h"
#include "ui/chromeos/network/network_info.h"
#include "ui/chromeos/network/network_list.h"
#include "ui/chromeos/network/network_list_view_base.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_delegate.h"
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
using ui::NetworkInfo;

namespace ash {
namespace tray {
namespace {

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

}  // namespace

//------------------------------------------------------------------------------

// A bubble which displays network info.
class NetworkStateListDetailedView::InfoBubble
    : public views::BubbleDelegateView {
 public:
  InfoBubble(views::View* anchor,
             views::View* content,
             NetworkStateListDetailedView* detailed_view)
      : views::BubbleDelegateView(anchor, views::BubbleBorder::TOP_RIGHT),
        detailed_view_(detailed_view) {
    set_can_activate(false);
    set_parent_window(ash::Shell::GetContainer(
        anchor->GetWidget()->GetNativeWindow()->GetRootWindow(),
        ash::kShellWindowId_SettingBubbleContainer));
    SetLayoutManager(new views::FillLayout());
    AddChildView(content);
  }

  ~InfoBubble() override { detailed_view_->OnInfoBubbleDestroyed(); }

 private:
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

 private:
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
    SetImage(STATE_NORMAL, bundle.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_INFO)
                               .ToImageSkia());
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
    return gfx::Size(ash::kTrayPopupItemHeight, ash::kTrayPopupItemHeight);
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
    user::LoginStatus login)
    : NetworkDetailedView(owner),
      list_type_(list_type),
      login_(login),
      wifi_scanning_(false),
      info_icon_(nullptr),
      button_wifi_(nullptr),
      button_mobile_(nullptr),
      other_wifi_(nullptr),
      turn_on_wifi_(nullptr),
      other_mobile_(nullptr),
      settings_(nullptr),
      proxy_settings_(nullptr),
      info_bubble_(nullptr),
      scanning_throbber_(nullptr) {
  if (list_type == LIST_TYPE_VPN) {
    // Use a specialized class to list VPNs.
    network_list_view_.reset(new VPNListView(this));
  } else {
    // Use a common class to list any other network types.
    network_list_view_.reset(new ui::NetworkListView(this));
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
  scanning_throbber_ = nullptr;

  CreateScrollableList();
  CreateNetworkExtra();
  CreateHeaderEntry();

  network_list_view_->set_container(scroll_content());
  Update();

  CallRequestScan();
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

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
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
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        list_type_ == LIST_TYPE_VPN
            ? ash::UMA_STATUS_AREA_VPN_SETTINGS_CLICKED
            : ash::UMA_STATUS_AREA_NETWORK_SETTINGS_CLICKED);
    delegate->ShowNetworkSettingsForGuid("");
  } else if (sender == proxy_settings_) {
    delegate->ChangeProxySettings();
  } else if (sender == other_mobile_) {
    delegate->ShowOtherNetworkDialog(shill::kTypeCellular);
  } else if (sender == other_wifi_) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED);
    delegate->ShowOtherNetworkDialog(shill::kTypeWifi);
  } else {
    NOTREACHED();
  }
}

bool NetworkStateListDetailedView::ThrobberPressed(views::View* sender,
                                                   const ui::Event& event) {
  if (sender != scanning_throbber_)
    return false;
  ToggleInfoBubble();
  return true;
}

void NetworkStateListDetailedView::OnViewClicked(views::View* sender) {
  // If the info bubble was visible, close it when some other item is clicked.
  ResetInfoBubble();

  if (sender == footer()->content()) {
    TransitionToDefaultView();
    return;
  }

  if (login_ == user::LOGGED_IN_LOCKED)
    return;

  std::string service_path;
  if (!network_list_view_->IsNetworkEntry(sender, &service_path))
    return;

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network || network->IsConnectedState() || network->IsConnectingState()) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        list_type_ == LIST_TYPE_VPN
            ? ash::UMA_STATUS_AREA_SHOW_NETWORK_CONNECTION_DETAILS
            : ash::UMA_STATUS_AREA_SHOW_VPN_CONNECTION_DETAILS);
    Shell::GetInstance()->system_tray_delegate()->ShowNetworkSettingsForGuid(
        network ? network->guid() : "");
  } else {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        list_type_ == LIST_TYPE_VPN
            ? ash::UMA_STATUS_AREA_CONNECT_TO_VPN
            : ash::UMA_STATUS_AREA_CONNECT_TO_CONFIGURED_NETWORK);
    ui::NetworkConnect::Get()->ConnectToNetwork(service_path);
  }
}

// Create UI components.

void NetworkStateListDetailedView::CreateHeaderEntry() {
  CreateSpecialRow(IDS_ASH_STATUS_TRAY_NETWORK, this);

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
    footer()->AddButton(button_wifi_);
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
    footer()->AddButton(button_mobile_);
  }

  views::View* info_throbber_container = new views::View();
  InfoThrobberLayout* info_throbber_layout = new InfoThrobberLayout;
  info_throbber_container->SetLayoutManager(info_throbber_layout);
  footer()->AddView(info_throbber_container, true /* add_separator */);

  // Place the throbber behind the info icon so that the icon receives
  // click / touch events. The info icon is hidden when the throbber is active.
  scanning_throbber_ = new ScanningThrobber();
  info_throbber_container->AddChildView(scanning_throbber_);

  info_icon_ = new InfoIcon(this);
  info_icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_INFO));
  info_throbber_container->AddChildView(info_icon_);
}

void NetworkStateListDetailedView::CreateNetworkExtra() {
  if (login_ == user::LOGGED_IN_LOCKED)
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

// Update UI components.

void NetworkStateListDetailedView::UpdateHeaderButtons() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  if (button_wifi_)
    UpdateTechnologyButton(button_wifi_, NetworkTypePattern::WiFi());
  if (button_mobile_) {
    UpdateTechnologyButton(button_mobile_, NetworkTypePattern::Mobile());
  }
  if (proxy_settings_)
    proxy_settings_->SetEnabled(handler->DefaultNetwork() != nullptr);

  // Update Wifi Scanning throbber.
  bool scanning =
      NetworkHandler::Get()->network_state_handler()->GetScanningByType(
          NetworkTypePattern::WiFi());
  if (scanning != wifi_scanning_) {
    wifi_scanning_ = scanning;
    if (scanning) {
      info_icon_->SetVisible(false);
      scanning_throbber_->SetVisible(true);
      scanning_throbber_->Start();
      scanning_throbber_->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_WIFI_SCANNING_MESSAGE));
    } else {
      scanning_throbber_->Stop();
      scanning_throbber_->SetVisible(false);
      scanning_throbber_->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_INFO));
      info_icon_->SetVisible(true);
    }
  }

  static_cast<views::View*>(footer())->Layout();
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
  if (login_ == user::LOGGED_IN_LOCKED)
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
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  bool show_settings =
      ash::Shell::GetInstance()->system_tray_delegate()->ShouldShowSettings();
  if (login_ != user::LOGGED_IN_NONE) {
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

  info_bubble_ = new InfoBubble(info_icon_, CreateNetworkInfoView(), this);
  views::BubbleDelegateView::CreateBubble(info_bubble_)->Show();
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
  container->SetBorder(views::Border::CreateEmptyBorder(0, 5, 0, 5));

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
    const ui::NetworkInfo& info) {
  NetworkingConfigDelegate* networking_config_delegate =
      Shell::GetInstance()
          ->system_tray_delegate()
          ->GetNetworkingConfigDelegate();
  if (!networking_config_delegate)
    return nullptr;
  scoped_ptr<const ash::NetworkingConfigDelegate::ExtensionInfo>
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
  ui::NetworkConnect::Get()->SetTechnologyEnabled(NetworkTypePattern::Mobile(),
                                                  !enabled);
}

views::View* NetworkStateListDetailedView::CreateViewForNetwork(
    const ui::NetworkInfo& info) {
  HoverHighlightView* view = new HoverHighlightView(this);
  view->AddIconAndLabel(info.image, info.label, info.highlight);
  view->SetBorder(
      views::Border::CreateEmptyBorder(0, kTrayPopupPaddingHorizontal, 0, 0));
  views::View* controlled_icon = CreateControlledByExtensionView(info);
  view->set_tooltip(info.tooltip);
  if (controlled_icon)
    view->AddChildView(controlled_icon);
  return view;
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
  HoverHighlightView* highlight = static_cast<HoverHighlightView*>(view);
  highlight->AddIconAndLabel(info.image, info.label, info.highlight);
  views::View* controlled_icon = CreateControlledByExtensionView(info);
  highlight->set_tooltip(info.tooltip);
  if (controlled_icon)
    view->AddChildView(controlled_icon);
}

views::Label* NetworkStateListDetailedView::CreateInfoLabel() {
  views::Label* label = new views::Label();
  label->SetBorder(views::Border::CreateEmptyBorder(
      ash::kTrayPopupPaddingBetweenItems, ash::kTrayPopupPaddingHorizontal,
      ash::kTrayPopupPaddingBetweenItems, 0));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SkColorSetARGB(192, 0, 0, 0));
  return label;
}

void NetworkStateListDetailedView::RelayoutScrollList() {
  scroller()->Layout();
}

}  // namespace tray
}  // namespace ash
