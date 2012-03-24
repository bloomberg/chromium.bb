// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_button.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/status/data_promo_notification.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

NetworkMenuButton::NetworkMenuButton(StatusAreaButton::Delegate* delegate)
    : StatusAreaButton(delegate, this),
      data_promo_notification_(new DataPromoNotification()) {
  set_id(VIEW_ID_STATUS_BUTTON_NETWORK_MENU);
  network_menu_.reset(new NetworkMenu(this));
  network_icon_.reset(
      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE));

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  OnNetworkManagerChanged(network_library);
  network_library->AddNetworkManagerObserver(this);
  network_library->AddCellularDataPlanObserver(this);
  const NetworkDevice* cellular = network_library->FindCellularDevice();
  if (cellular) {
    cellular_device_path_ = cellular->device_path();
    network_library->AddNetworkDeviceObserver(cellular_device_path_, this);
  }
}

NetworkMenuButton::~NetworkMenuButton() {
  NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
  netlib->RemoveNetworkManagerObserver(this);
  netlib->RemoveObserverForAllNetworks(this);
  netlib->RemoveCellularDataPlanObserver(this);
  if (!cellular_device_path_.empty())
    netlib->RemoveNetworkDeviceObserver(cellular_device_path_, this);
}

// static
void NetworkMenuButton::RegisterPrefs(PrefService* local_state) {
  // Carrier deal notification shown count defaults to 0.
  local_state->RegisterIntegerPref(prefs::kCarrierDealPromoShown, 0);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary::NetworkDeviceObserver implementation:

void NetworkMenuButton::OnNetworkDeviceChanged(NetworkLibrary* cros,
                                               const NetworkDevice* device) {
  // Device status, such as SIMLock may have changed.
  SetNetworkIcon();
  network_menu_->UpdateMenu();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::NetworkManagerObserver implementation:

void NetworkMenuButton::OnNetworkManagerChanged(NetworkLibrary* cros) {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().
  SetNetworkIcon();
  network_menu_->UpdateMenu();
  RefreshNetworkObserver(cros);
  RefreshNetworkDeviceObserver(cros);
  data_promo_notification_->ShowOptionalMobileDataPromoNotification(cros, this,
      this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::NetworkObserver implementation:
void NetworkMenuButton::OnNetworkChanged(NetworkLibrary* cros,
                                         const Network* network) {
  SetNetworkIcon();
  network_menu_->UpdateMenu();
}

void NetworkMenuButton::OnCellularDataPlanChanged(NetworkLibrary* cros) {
  // Call OnNetworkManagerChanged which will update the icon.
  SetNetworkIcon();
  network_menu_->UpdateMenu();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkMenu implementation:

views::MenuButton* NetworkMenuButton::GetMenuButton() {
  return this;
}

gfx::NativeWindow NetworkMenuButton::GetNativeWindow() const {
  if (BaseLoginDisplayHost::default_host()) {
    // When not in browser mode i.e. login screen, status area is hosted in
    // a separate widget.
    return BaseLoginDisplayHost::default_host()->GetNativeWindow();
  } else {
    // This must always have a parent, which must have a widget ancestor.
    return parent()->GetWidget()->GetNativeWindow();
  }
}

void NetworkMenuButton::OpenButtonOptions() {
  delegate()->ExecuteStatusAreaCommand(
      this, StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS);
}

bool NetworkMenuButton::ShouldOpenButtonOptions() const {
  return delegate()->ShouldExecuteStatusAreaCommand(
      this, StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::View implementation:

void NetworkMenuButton::OnLocaleChanged() {
  SetNetworkIcon();
  network_menu_->UpdateMenu();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::MenuButtonListener implementation:
void NetworkMenuButton::OnMenuButtonClicked(views::View* source,
                                            const gfx::Point& point) {
  network_menu_->RunMenu(source);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkMenuIcon::Delegate implementation:
void NetworkMenuButton::NetworkMenuIconChanged() {
  const SkBitmap bitmap = network_icon_->GetIconAndText(NULL);
  SetIcon(bitmap);
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// MessageBubbleLinkListener implementation:

void NetworkMenuButton::OnLinkActivated(size_t index) {
  // If we have deal info URL defined that means that there're
  // 2 links in bubble. Let the user close it manually then thus giving ability
  // to navigate to second link.
  // mobile_data_bubble_ will be set to NULL in BubbleClosing callback.
  std::string deal_info_url = data_promo_notification_->deal_info_url();
  std::string deal_topup_url = data_promo_notification_->deal_topup_url();
  if (deal_info_url.empty())
    data_promo_notification_->CloseNotification();

  std::string deal_url_to_open;
  if (index == 0) {
    if (!deal_topup_url.empty()) {
      deal_url_to_open = deal_topup_url;
    } else {
      const Network* cellular =
          CrosLibrary::Get()->GetNetworkLibrary()->cellular_network();
      if (!cellular)
        return;
      network_menu_->ShowTabbedNetworkSettings(cellular);
      return;
    }
  } else if (index == 1) {
    deal_url_to_open = deal_info_url;
  }

  if (!deal_url_to_open.empty()) {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser)
      return;
    browser->ShowSingletonTab(GURL(deal_url_to_open));
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, private methods:

void NetworkMenuButton::SetNetworkIcon() {
  string16 tooltip;
  const SkBitmap bitmap = network_icon_->GetIconAndText(&tooltip);
  SetIcon(bitmap);
  SetTooltipAndAccessibleName(tooltip);
  SchedulePaint();
}

void NetworkMenuButton::RefreshNetworkObserver(NetworkLibrary* cros) {
  const Network* network = cros->active_network();
  std::string new_network = network ? network->service_path() : std::string();
  if (active_network_ != new_network) {
    if (!active_network_.empty()) {
      cros->RemoveNetworkObserver(active_network_, this);
    }
    if (!new_network.empty()) {
      cros->AddNetworkObserver(new_network, this);
    }
    active_network_ = new_network;
  }
}

void NetworkMenuButton::RefreshNetworkDeviceObserver(NetworkLibrary* cros) {
  const NetworkDevice* cellular = cros->FindCellularDevice();
  std::string new_cellular_device_path = cellular ?
      cellular->device_path() : std::string();
  if (cellular_device_path_ != new_cellular_device_path) {
    if (!cellular_device_path_.empty()) {
      cros->RemoveNetworkDeviceObserver(cellular_device_path_, this);
    }
    if (!new_cellular_device_path.empty()) {
      cros->AddNetworkDeviceObserver(new_cellular_device_path, this);
    }
    cellular_device_path_ = new_cellular_device_path;
  }
}

void NetworkMenuButton::SetTooltipAndAccessibleName(const string16& label) {
  SetTooltipText(label);
  SetAccessibleName(label);
}

}  // namespace chromeos
