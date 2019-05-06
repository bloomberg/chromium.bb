// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_section_header_view.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "base/bind.h"
#include "chromeos/network/device_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/controls/image_view.h"

using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {

namespace {

// Full opacity for badge.
constexpr int kJoinBadgeAlpha = 0xFF;

// .30 opacity for icon.
constexpr int kJoinIconAlpha = 0x4D;

// .38 opacity for disabled badge.
constexpr int kDisabledJoinBadgeAlpha = 0x61;

// .30 * .38 opacity for disabled icon.
constexpr int kDisabledJoinIconAlpha = 0x1D;

const int64_t kBluetoothTimeoutDelaySeconds = 2;

bool IsCellularSimLocked() {
  const chromeos::DeviceState* cellular_device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
          NetworkTypePattern::Cellular());
  return cellular_device && cellular_device->IsSimLocked();
}

void ShowCellularSettings() {
  const chromeos::NetworkState* cellular_network =
      NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
          NetworkTypePattern::Cellular());
  Shell::Get()->system_tray_model()->client_ptr()->ShowNetworkSettings(
      cellular_network ? cellular_network->guid() : std::string());
}

bool IsSecondaryUser() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsUserPrimary();
}

}  // namespace

NetworkSectionHeaderView::NetworkSectionHeaderView(int title_id)
    : title_id_(title_id) {}

void NetworkSectionHeaderView::Init(bool enabled) {
  InitializeLayout();
  AddExtraButtons(enabled);
  AddToggleButton(enabled);
}

void NetworkSectionHeaderView::AddExtraButtons(bool enabled) {}

void NetworkSectionHeaderView::SetToggleVisibility(bool visible) {
  toggle_->SetVisible(visible);
}

void NetworkSectionHeaderView::SetToggleState(bool toggle_enabled, bool is_on) {
  toggle_->SetEnabled(toggle_enabled);
  toggle_->set_accepts_events(toggle_enabled);
  toggle_->SetIsOn(is_on, true /* animate */);
}

int NetworkSectionHeaderView::GetHeightForWidth(int width) const {
  // Make row height fixed avoiding layout manager adjustments.
  return GetPreferredSize().height();
}
void NetworkSectionHeaderView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  DCHECK_EQ(toggle_, sender);
  // In the event of frequent clicks, helps to prevent a toggle button state
  // from becoming inconsistent with the async operation of enabling /
  // disabling of mobile radio. The toggle will get unlocked in the next
  // call to NetworkListView::Update(). Note that we don't disable/enable
  // because that would clear focus.
  toggle_->set_accepts_events(false);
  OnToggleToggled(toggle_->is_on());
}
void NetworkSectionHeaderView::InitializeLayout() {
  TrayPopupUtils::ConfigureAsStickyHeader(this);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  container_ = TrayPopupUtils::CreateSubHeaderRowView(true);
  container_->AddView(TriView::Container::START,
                      TrayPopupUtils::CreateMainImageView());
  AddChildView(container_);

  network_row_title_view_ = new NetworkRowTitleView(title_id_);
  container_->AddView(TriView::Container::CENTER, network_row_title_view_);
}

void NetworkSectionHeaderView::AddToggleButton(bool enabled) {
  toggle_ = TrayPopupUtils::CreateToggleButton(this, title_id_);
  toggle_->SetIsOn(enabled, false);
  container_->AddView(TriView::Container::END, toggle_);
}

MobileSectionHeaderView::MobileSectionHeaderView()
    : NetworkSectionHeaderView(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE),
      weak_ptr_factory_(this) {
  NetworkSectionHeaderView::Init(false /* enabled */);
}

MobileSectionHeaderView::~MobileSectionHeaderView() {}

const char* MobileSectionHeaderView::GetClassName() const {
  return "MobileSectionHeaderView";
}

int MobileSectionHeaderView::UpdateToggleAndGetStatusMessage() {
  NetworkStateHandler* network_state_handler =
      NetworkHandler::Get()->network_state_handler();
  NetworkStateHandler::TechnologyState cellular_state =
      network_state_handler->GetTechnologyState(NetworkTypePattern::Cellular());
  NetworkStateHandler::TechnologyState tether_state =
      network_state_handler->GetTechnologyState(NetworkTypePattern::Tether());

  bool default_toggle_enabled = !IsSecondaryUser();

  // If Cellular is available, toggle state and status message reflect Cellular.
  if (cellular_state != NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
    const chromeos::DeviceState* cellular_device =
        network_state_handler->GetDeviceStateByType(
            NetworkTypePattern::Cellular());
    bool cellular_enabled =
        cellular_state == NetworkStateHandler::TECHNOLOGY_ENABLED;

    if (!cellular_device->IsSimAbsent()) {
      SetToggleVisibility(true);
      SetToggleState(default_toggle_enabled, cellular_enabled);
    } else {
      SetToggleVisibility(false);
    }

    if (!cellular_device ||
        cellular_state == NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) {
      return IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    }
    if (cellular_device->scanning())
      return IDS_ASH_STATUS_TRAY_MOBILE_SCANNING;

    if (cellular_device->IsSimAbsent())
      return IDS_ASH_STATUS_TRAY_SIM_CARD_MISSING;

    if (cellular_device->IsSimLocked())
      return IDS_ASH_STATUS_TRAY_SIM_CARD_LOCKED;

    const chromeos::NetworkState* mobile_network =
        network_state_handler->FirstNetworkByType(NetworkTypePattern::Mobile());
    if (cellular_enabled &&
        (!mobile_network || mobile_network->IsDefaultCellular())) {
      // If no connectable Cellular network is available (see
      // network_state_handler.h re: IsDefaultCellular), show 'turn on
      // Bluetooth' if Tether is available but not initialized, otherwise
      // show 'no networks'.
      if (tether_state == NetworkStateHandler::TECHNOLOGY_UNINITIALIZED)
        return IDS_ENABLE_BLUETOOTH;
      return IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS;
    }
    return 0;
  }

  // Tether is also unavailable (edge case).
  if (tether_state == NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
    SetToggleState(false /* toggle_enabled */, false /* is_on */);
    return IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLED;
  }

  // Otherwise, toggle state and status message reflect Tether.
  if (tether_state == NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) {
    if (waiting_for_tether_initialize_) {
      SetToggleState(false /* toggle_enabled */, true /* is_on */);
      // "Initializing...". TODO(stevenjb): Rename the string to _MOBILE.
      return IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    } else {
      SetToggleState(default_toggle_enabled, false /* is_on */);
      return IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH;
    }
  }

  bool tether_enabled = tether_state == NetworkStateHandler::TECHNOLOGY_ENABLED;

  if (waiting_for_tether_initialize_) {
    waiting_for_tether_initialize_ = false;
    enable_bluetooth_timer_.Stop();
    if (!tether_enabled) {
      // We enabled Bluetooth so Tether is now initialized, but it was not
      // enabled so enable it.
      network_state_handler->SetTechnologyEnabled(
          NetworkTypePattern::Tether(), true /* enabled */,
          chromeos::network_handler::ErrorCallback());
      SetToggleState(default_toggle_enabled, true /* is_on */);
      // "Initializing...". TODO(stevenjb): Rename the string to _MOBILE.
      return IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    }
  }

  // Ensure that the toggle state and status message match the tether state.
  SetToggleState(default_toggle_enabled, tether_enabled /* is_on */);
  if (tether_enabled && !network_state_handler->FirstNetworkByType(
                            NetworkTypePattern::Tether())) {
    return IDS_ASH_STATUS_TRAY_NO_MOBILE_DEVICES_FOUND;
  }
  return 0;
}

void MobileSectionHeaderView::OnToggleToggled(bool is_on) {
  NetworkStateHandler* network_state_handler =
      NetworkHandler::Get()->network_state_handler();
  NetworkStateHandler::TechnologyState cellular_state =
      network_state_handler->GetTechnologyState(NetworkTypePattern::Cellular());

  // When Cellular is available, the toggle controls Cellular enabled state.
  // (Tether may be enabled by turning on Bluetooth and turning on
  // 'Get data connection' in the Settings > Mobile data subpage).
  if (cellular_state != NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
    if (is_on && IsCellularSimLocked()) {
      ShowCellularSettings();
      return;
    }
    network_state_handler->SetTechnologyEnabled(
        NetworkTypePattern::Cellular(), is_on,
        chromeos::network_handler::ErrorCallback());
    return;
  }

  NetworkStateHandler::TechnologyState tether_state =
      network_state_handler->GetTechnologyState(NetworkTypePattern::Tether());

  // Tether is also unavailable (edge case).
  if (tether_state == NetworkStateHandler::TECHNOLOGY_UNAVAILABLE)
    return;

  // If Tether is available but uninitialized, we expect Bluetooth to be off.
  // Enable Bluetooth so that Tether will be initialized. Ignore edge cases
  // (e.g. Bluetooth was disabled from a different UI).
  if (tether_state == NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) {
    if (is_on && !waiting_for_tether_initialize_)
      EnableBluetooth();
    return;
  }

  // Otherwise the toggle controls the Tether enabled state.
  network_state_handler->SetTechnologyEnabled(
      NetworkTypePattern::Tether(), is_on,
      chromeos::network_handler::ErrorCallback());
}

void MobileSectionHeaderView::EnableBluetooth() {
  DCHECK(!waiting_for_tether_initialize_);

  Shell::Get()
      ->bluetooth_power_controller()
      ->SetPrimaryUserBluetoothPowerSetting(true /* enabled */);
  waiting_for_tether_initialize_ = true;
  enable_bluetooth_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kBluetoothTimeoutDelaySeconds),
      base::BindOnce(&MobileSectionHeaderView::OnEnableBluetoothTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MobileSectionHeaderView::OnEnableBluetoothTimeout() {
  DCHECK(waiting_for_tether_initialize_);
  waiting_for_tether_initialize_ = false;
  SetToggleState(true /* toggle_enabled */, false /* is_on */);

  LOG(ERROR) << "Error enabling Bluetooth. Cannot enable Mobile data.";
}

WifiSectionHeaderView::WifiSectionHeaderView()
    : NetworkSectionHeaderView(IDS_ASH_STATUS_TRAY_NETWORK_WIFI) {
  bool enabled =
      NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
          NetworkTypePattern::WiFi());
  NetworkSectionHeaderView::Init(enabled);
}

void WifiSectionHeaderView::SetToggleState(bool toggle_enabled, bool is_on) {
  join_button_->SetEnabled(toggle_enabled && is_on);
  NetworkSectionHeaderView::SetToggleState(toggle_enabled, is_on);
}
const char* WifiSectionHeaderView::GetClassName() const {
  return "WifiSectionHeaderView";
}
void WifiSectionHeaderView::OnToggleToggled(bool is_on) {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(), is_on,
                                chromeos::network_handler::ErrorCallback());
}
void WifiSectionHeaderView::AddExtraButtons(bool enabled) {
  const SkColor prominent_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor);
  gfx::ImageSkia normal_image = network_icon::GetImageForNewWifiNetwork(
      SkColorSetA(prominent_color, kJoinIconAlpha),
      SkColorSetA(prominent_color, kJoinBadgeAlpha));
  gfx::ImageSkia disabled_image = network_icon::GetImageForNewWifiNetwork(
      SkColorSetA(prominent_color, kDisabledJoinIconAlpha),
      SkColorSetA(prominent_color, kDisabledJoinBadgeAlpha));
  auto* join_button = new TopShortcutButton(this, vector_icons::kWifiAddIcon,
                                            IDS_ASH_STATUS_TRAY_OTHER_WIFI);
  join_button->SetEnabled(enabled);
  container()->AddView(TriView::Container::END, join_button);
  join_button_ = join_button;
}

void WifiSectionHeaderView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == join_button_) {
    Shell::Get()->metrics()->RecordUserMetricsAction(
        UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED);
    Shell::Get()->system_tray_model()->client_ptr()->ShowNetworkCreate(
        ::onc::network_type::kWiFi);
    return;
  }
  NetworkSectionHeaderView::ButtonPressed(sender, event);
}

}  // namespace tray
}  // namespace ash
