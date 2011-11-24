// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/status/status_area_view_chromeos.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

namespace {

// Time in milliseconds to delay showing of promo
// notification when Chrome window is not on screen.
const int kPromoShowDelayMs = 10000;

const int kNotificationCountPrefDefault = -1;

bool GetBooleanPref(const char* pref_name) {
  Browser* browser = BrowserList::GetLastActive();
  // Default to safe value which is false (not to show bubble).
  if (!browser || !browser->profile())
    return false;

  PrefService* prefs = browser->profile()->GetPrefs();
  return prefs->GetBoolean(pref_name);
}

int GetIntegerLocalPref(const char* pref_name) {
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetInteger(pref_name);
}

void SetBooleanPref(const char* pref_name, bool value) {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !browser->profile())
    return;

  PrefService* prefs = browser->profile()->GetPrefs();
  prefs->SetBoolean(pref_name, value);
}

void SetIntegerLocalPref(const char* pref_name, int value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(pref_name, value);
}

// Returns prefs::kShow3gPromoNotification or false
// if there's no active browser.
bool ShouldShow3gPromoNotification() {
  return GetBooleanPref(prefs::kShow3gPromoNotification);
}

void SetShow3gPromoNotification(bool value) {
  SetBooleanPref(prefs::kShow3gPromoNotification, value);
}

// Returns prefs::kCarrierDealPromoShown which is number of times
// carrier deal notification has been shown to users on this machine.
int GetCarrierDealPromoShown() {
  return GetIntegerLocalPref(prefs::kCarrierDealPromoShown);
}

void SetCarrierDealPromoShown(int value) {
  SetIntegerLocalPref(prefs::kCarrierDealPromoShown, value);
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

NetworkMenuButton::NetworkMenuButton(StatusAreaButton::Delegate* delegate)
    : StatusAreaButton(delegate, this),
      mobile_data_bubble_(NULL),
      check_for_promo_(true),
      was_sim_locked_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
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
    was_sim_locked_ = cellular->is_sim_locked();
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
  if (mobile_data_bubble_)
    mobile_data_bubble_->Close();
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
  const NetworkDevice* cellular = cros->FindCellularDevice();
  if (cellular) {
    // We make an assumption (which is valid for now) that the SIM
    // unlock dialog is put up only when the user is trying to enable
    // mobile data. So if the SIM is now unlocked, initiate the
    // enable operation that the user originally requested.
    if (was_sim_locked_ && !cellular->is_sim_locked() &&
        !cros->cellular_enabled()) {
      cros->EnableCellularNetworkDevice(true);
    }
    was_sim_locked_ = cellular->is_sim_locked();
  }
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
  ShowOptionalMobileDataPromoNotification(cros);
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
// NetworkMenuButton, views::ViewMenuDelegate implementation:
void NetworkMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
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
// MessageBubbleDelegate implementation:

void NetworkMenuButton::BubbleClosing(Bubble* bubble, bool closed_by_escape) {
  mobile_data_bubble_ = NULL;
  deal_info_url_.clear();
  deal_topup_url_.clear();
}

bool NetworkMenuButton::CloseOnEscape() {
  return true;
}

bool NetworkMenuButton::FadeInOnShow() {
  return false;
}

void NetworkMenuButton::OnLinkActivated(size_t index) {
  // If we have deal info URL defined that means that there're
  // 2 links in bubble. Let the user close it manually then thus giving ability
  // to navigate to second link.
  // mobile_data_bubble_ will be set to NULL in BubbleClosing callback.
  if (deal_info_url_.empty() && mobile_data_bubble_)
    mobile_data_bubble_->Close();

  std::string deal_url_to_open;
  if (index == 0) {
    if (!deal_topup_url_.empty()) {
      deal_url_to_open = deal_topup_url_;
    } else {
      const Network* cellular =
          CrosLibrary::Get()->GetNetworkLibrary()->cellular_network();
      if (!cellular)
        return;
      network_menu_->ShowTabbedNetworkSettings(cellular);
      return;
    }
  } else if (index == 1) {
    deal_url_to_open = deal_info_url_;
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

const MobileConfig::Carrier* NetworkMenuButton::GetCarrier(
    NetworkLibrary* cros) {
  std::string carrier_id = cros->GetCellularHomeCarrierId();
  if (carrier_id.empty()) {
    LOG(ERROR) << "Empty carrier ID with a cellular connected.";
    return NULL;
  }

  MobileConfig* config = MobileConfig::GetInstance();
  if (!config->IsReady())
    return NULL;

  return config->GetCarrier(carrier_id);
}

const MobileConfig::CarrierDeal* NetworkMenuButton::GetCarrierDeal(
    const MobileConfig::Carrier* carrier) {
  const MobileConfig::CarrierDeal* deal = carrier->GetDefaultDeal();
  if (deal) {
    // Check deal for validity.
    int carrier_deal_promo_pref = GetCarrierDealPromoShown();
    if (carrier_deal_promo_pref >= deal->notification_count())
      return NULL;
    const std::string locale = g_browser_process->GetApplicationLocale();
    std::string deal_text = deal->GetLocalizedString(locale,
                                                     "notification_text");
    if (deal_text.empty())
      return NULL;
  }
  return deal;
}

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
      was_sim_locked_ = cellular->is_sim_locked();
      cros->AddNetworkDeviceObserver(new_cellular_device_path, this);
    }
    cellular_device_path_ = new_cellular_device_path;
  }
}

void NetworkMenuButton::ShowOptionalMobileDataPromoNotification(
    NetworkLibrary* cros) {
  // Display one-time notification for non-Guest users on first use
  // of Mobile Data connection or if there's a carrier deal defined
  // show that even if user has already seen generic promo.
  if (StatusAreaViewChromeos::IsBrowserMode() &&
      !UserManager::Get()->IsLoggedInAsGuest() &&
      check_for_promo_ && BrowserList::GetLastActive() &&
      cros->cellular_connected() && !cros->ethernet_connected() &&
      !cros->wifi_connected()) {
    std::string deal_text;
    int carrier_deal_promo_pref = -1;
    const MobileConfig::CarrierDeal* deal = NULL;
    const MobileConfig::Carrier* carrier = GetCarrier(cros);
    if (carrier)
      deal = GetCarrierDeal(carrier);
    if (deal) {
      carrier_deal_promo_pref = GetCarrierDealPromoShown();
      const std::string locale = g_browser_process->GetApplicationLocale();
      deal_text = deal->GetLocalizedString(locale, "notification_text");
      deal_info_url_ = deal->info_url();
      deal_topup_url_ = carrier->top_up_url();
    } else if (!ShouldShow3gPromoNotification()) {
      check_for_promo_ = false;
      return;
    }

    gfx::Rect button_bounds = GetScreenBounds();
    // StatusArea button Y position is usually -1, fix it so that
    // Contains() method for screen bounds works correctly.
    button_bounds.set_y(button_bounds.y() + 1);
    gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));

    // Chrome window is initialized in visible state off screen and then is
    // moved into visible screen area. Make sure that we're on screen
    // so that bubble is shown correctly.
    if (!screen_bounds.Contains(button_bounds)) {
      // If we're not on screen yet, delay notification display.
      // It may be shown earlier, on next NetworkLibrary callback processing.
      if (!weak_ptr_factory_.HasWeakPtrs()) {
        MessageLoop::current()->PostDelayedTask(FROM_HERE,
            base::Bind(
                &NetworkMenuButton::ShowOptionalMobileDataPromoNotification,
                weak_ptr_factory_.GetWeakPtr(),
                cros),
            kPromoShowDelayMs);
      }
      return;
    }

    // Add deal text if it's defined.
    std::wstring notification_text;
    std::wstring default_text =
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_3G_NOTIFICATION_MESSAGE));
    if (!deal_text.empty()) {
      notification_text = StringPrintf(L"%ls\n\n%ls",
                                       UTF8ToWide(deal_text).c_str(),
                                       default_text.c_str());
    } else {
      notification_text = default_text;
    }

    // Use deal URL if it's defined or general "Network Settings" URL.
    int link_message_id;
    if (deal_topup_url_.empty())
      link_message_id = IDS_OFFLINE_NETWORK_SETTINGS;
    else
      link_message_id = IDS_STATUSBAR_NETWORK_VIEW_ACCOUNT;

    std::vector<std::wstring> links;
    links.push_back(UTF16ToWide(l10n_util::GetStringUTF16(link_message_id)));
    if (!deal_info_url_.empty())
      links.push_back(UTF16ToWide(l10n_util::GetStringUTF16(IDS_LEARN_MORE)));
    mobile_data_bubble_ = MessageBubble::ShowWithLinks(
        GetWidget(),
        button_bounds,
        views::BubbleBorder::TOP_RIGHT ,
        ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_NOTIFICATION_3G),
        notification_text,
        links,
        this);

    check_for_promo_ = false;
    SetShow3gPromoNotification(false);
    if (carrier_deal_promo_pref != kNotificationCountPrefDefault)
      SetCarrierDealPromoShown(carrier_deal_promo_pref + 1);
  }
}

void NetworkMenuButton::SetTooltipAndAccessibleName(const string16& label) {
  SetTooltipText(label);
  SetAccessibleName(label);
}

}  // namespace chromeos
