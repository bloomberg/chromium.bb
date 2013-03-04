// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_list_detailed_view.h"

#include "ash/shell.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace internal {
namespace tray {

NetworkListDetailedView::NetworkListDetailedView(SystemTrayItem* owner,
                                                 user::LoginStatus login,
                                                 int header_string_id)
    : NetworkListDetailedViewBase(owner, login, header_string_id),
      airplane_(NULL),
      button_wifi_(NULL),
      button_mobile_(NULL),
      view_mobile_account_(NULL),
      setup_mobile_account_(NULL),
      other_wifi_(NULL),
      turn_on_wifi_(NULL),
      other_mobile_(NULL),
      scanning_view_(NULL),
      no_wifi_networks_view_(NULL),
      no_cellular_networks_view_(NULL) {
}

NetworkListDetailedView::~NetworkListDetailedView() {
}

// NetworkListDetailedViewBase
void NetworkListDetailedView::AppendHeaderButtons() {
  button_wifi_ = new TrayPopupHeaderButton(
      this,
      IDR_AURA_UBER_TRAY_WIFI_ENABLED,
      IDR_AURA_UBER_TRAY_WIFI_DISABLED,
      IDR_AURA_UBER_TRAY_WIFI_ENABLED_HOVER,
      IDR_AURA_UBER_TRAY_WIFI_DISABLED_HOVER,
      IDS_ASH_STATUS_TRAY_WIFI);
  button_wifi_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_WIFI));
  button_wifi_->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_WIFI));
  footer()->AddButton(button_wifi_);

  button_mobile_ = new TrayPopupHeaderButton(
      this,
      IDR_AURA_UBER_TRAY_CELLULAR_ENABLED,
      IDR_AURA_UBER_TRAY_CELLULAR_DISABLED,
      IDR_AURA_UBER_TRAY_CELLULAR_ENABLED_HOVER,
      IDR_AURA_UBER_TRAY_CELLULAR_DISABLED_HOVER,
      IDS_ASH_STATUS_TRAY_CELLULAR);
  button_mobile_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_MOBILE));
  button_mobile_->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_MOBILE));
  footer()->AddButton(button_mobile_);

  AppendInfoButtonToHeader();
}

void NetworkListDetailedView::UpdateHeaderButtons() {
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  button_wifi_->SetToggled(!delegate->GetWifiEnabled());
  button_mobile_->SetToggled(!delegate->GetMobileEnabled());
  button_mobile_->SetVisible(delegate->GetMobileAvailable());
  UpdateSettingButton();
  static_cast<views::View*>(footer())->Layout();
}

void NetworkListDetailedView::AppendNetworkEntries() {
  CreateScrollableList();

  HoverHighlightView* container = new HoverHighlightView(this);
  container->AddLabel(ui::ResourceBundle::GetSharedInstance().
                      GetLocalizedString(
                          IDS_ASH_STATUS_TRAY_MOBILE_VIEW_ACCOUNT),
                      gfx::Font::NORMAL);
  AddChildView(container);
  view_mobile_account_ = container;

  container = new HoverHighlightView(this);
  container->AddLabel(ui::ResourceBundle::GetSharedInstance().
                      GetLocalizedString(IDS_ASH_STATUS_TRAY_SETUP_MOBILE),
                      gfx::Font::NORMAL);
  AddChildView(container);
  setup_mobile_account_ = container;
}

void NetworkListDetailedView::GetAvailableNetworkList(
    std::vector<NetworkIconInfo>* list) {
  Shell::GetInstance()->system_tray_delegate()->GetAvailableNetworks(list);
}

void NetworkListDetailedView::UpdateNetworkEntries() {
  RefreshNetworkScrollWithUpdatedNetworkData();

  view_mobile_account_->SetVisible(false);
  setup_mobile_account_->SetVisible(false);

  if (login() == user::LOGGED_IN_NONE)
    return;

  std::string carrier_id, topup_url, setup_url;
  if (Shell::GetInstance()->system_tray_delegate()->
      GetCellularCarrierInfo(&carrier_id, &topup_url, &setup_url)) {
    if (carrier_id != carrier_id_) {
      carrier_id_ = carrier_id;
      if (!topup_url.empty())
        topup_url_ = topup_url;
    }

    if (!setup_url.empty())
      setup_url_ = setup_url;

    if (!topup_url_.empty())
      view_mobile_account_->SetVisible(true);
    if (!setup_url_.empty())
      setup_mobile_account_->SetVisible(true);
  }
}

void NetworkListDetailedView::AppendCustomButtonsToBottomRow(
    views::View* bottom_row) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  other_wifi_ = new TrayPopupLabelButton(
      this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_WIFI));
  bottom_row->AddChildView(other_wifi_);

  turn_on_wifi_ = new TrayPopupLabelButton(
      this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_TURN_ON_WIFI));
  bottom_row->AddChildView(turn_on_wifi_);

  other_mobile_ = new TrayPopupLabelButton(
      this, rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_MOBILE));
  bottom_row->AddChildView(other_mobile_);
}

void NetworkListDetailedView::UpdateNetworkExtra() {
  if (login() == user::LOGGED_IN_LOCKED)
    return;

  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  if (IsNetworkListEmpty() && !delegate->GetWifiEnabled()) {
    turn_on_wifi_->SetVisible(true);
    other_wifi_->SetVisible(false);
  } else {
    turn_on_wifi_->SetVisible(false);
    other_wifi_->SetVisible(true);
    other_wifi_->SetEnabled(delegate->GetWifiEnabled());
  }
  other_mobile_->SetVisible(delegate->GetMobileAvailable() &&
                            delegate->GetMobileScanSupported());
  if (other_mobile_->visible())
    other_mobile_->SetEnabled(delegate->GetMobileEnabled());

  turn_on_wifi_->parent()->Layout();
}

 void NetworkListDetailedView::CustomButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
   ash::SystemTrayDelegate* delegate =
       ash::Shell::GetInstance()->system_tray_delegate();
   if (sender == button_wifi_)
     delegate->ToggleWifi();
   else if (sender == button_mobile_)
     delegate->ToggleMobile();
   else if (sender == other_mobile_)
     delegate->ShowOtherCellular();
   else if (sender == other_wifi_)
     delegate->ShowOtherWifi();
   else if (sender == turn_on_wifi_)
     delegate->ToggleWifi();
   else
     NOTREACHED();
 }

bool NetworkListDetailedView::CustomLinkClickedOn(views::View* sender) {
  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
  if (sender == view_mobile_account_) {
    delegate->ShowCellularURL(topup_url_);
    return true;
  } else if (sender == setup_mobile_account_) {
    delegate->ShowCellularURL(setup_url_);
    return true;
  } else if (sender == airplane_) {
    delegate->ToggleAirplaneMode();
    return true;
  } else {
    return false;
  }
}

bool NetworkListDetailedView::UpdateNetworkListEntries(
    std::set<std::string>* new_service_paths) {
  bool needs_relayout = false;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();

  // Insert child views

  int index = 0;
  bool have_cellular_network = false;

  // Highlighted networks
  for (size_t i = 0; i < network_list().size(); ++i) {
    const NetworkIconInfo* info = &network_list()[i];
    if (info->highlight()) {
      if (UpdateNetworkChild(index++, info))
        needs_relayout = true;
      new_service_paths->insert(info->service_path);
    }
    if (info->is_cellular)
      have_cellular_network = true;
  }

  // "Cellular Initializing" or "No celular networks" or "No network"
  int status_message_id = 0;
  if (delegate->GetCellularInitializing())
    status_message_id = IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
  else if (!have_cellular_network && delegate->GetMobileEnabled())
    status_message_id = IDS_ASH_STATUS_TRAY_NO_CELLULAR_NETWORKS;
  if (status_message_id) {
    string16 text = rb.GetLocalizedString(status_message_id);
    if (CreateOrUpdateInfoLabel(index++, text, &no_cellular_networks_view_))
      needs_relayout = true;
  } else if (no_cellular_networks_view_) {
    scroll_content()->RemoveChildView(no_cellular_networks_view_);
    no_cellular_networks_view_ = NULL;
    needs_relayout = true;
  }

  // "Wifi Enabled / Disabled"
  if (network_list().empty()) {
    int message_id = delegate->GetWifiEnabled() ?
        IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED :
        IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
    string16 text = rb.GetLocalizedString(message_id);
    if (CreateOrUpdateInfoLabel(index++, text, &no_wifi_networks_view_))
      needs_relayout = true;
  } else if (no_wifi_networks_view_) {
    scroll_content()->RemoveChildView(no_wifi_networks_view_);
    no_wifi_networks_view_ = NULL;
    needs_relayout = true;
  }

  // "Wifi Scanning"
  if (delegate->GetWifiScanning()) {
    string16 text =
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_WIFI_SCANNING_MESSAGE);
    if (CreateOrUpdateInfoLabel(index++, text, &scanning_view_))
      needs_relayout = true;
  } else if (scanning_view_ != NULL) {
    scroll_content()->RemoveChildView(scanning_view_);
    scanning_view_ = NULL;
    needs_relayout = true;
  }

  // Un-highlighted networks
  for (size_t i = 0; i < network_list().size(); ++i) {
    const NetworkIconInfo* info = &network_list()[i];
    if (!info->highlight()) {
      if (UpdateNetworkChild(index++, info))
        needs_relayout = true;
      new_service_paths->insert(info->service_path);
    }
  }

  return needs_relayout;
}

}  // namespace tray
}  // namespace internal
}  // namespace ash
