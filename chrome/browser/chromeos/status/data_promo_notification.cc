// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/data_promo_notification.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Time in milliseconds to delay showing of promo
// notification when Chrome window is not on screen.
const int kPromoShowDelayMs = 10000;

const int kNotificationCountPrefDefault = -1;

bool GetBooleanPref(const char* pref_name) {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  PrefService* prefs = profile->GetPrefs();
  return prefs->GetBoolean(pref_name);
}

int GetIntegerLocalPref(const char* pref_name) {
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetInteger(pref_name);
}

void SetBooleanPref(const char* pref_name, bool value) {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  PrefService* prefs = profile->GetPrefs();
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

const chromeos::MobileConfig::Carrier* GetCarrier(
    chromeos::NetworkLibrary* cros) {
  std::string carrier_id = cros->GetCellularHomeCarrierId();
  if (carrier_id.empty()) {
    LOG(ERROR) << "Empty carrier ID with a cellular connected.";
    return NULL;
  }

  chromeos::MobileConfig* config = chromeos::MobileConfig::GetInstance();
  if (!config->IsReady())
    return NULL;

  return config->GetCarrier(carrier_id);
}

const chromeos::MobileConfig::CarrierDeal* GetCarrierDeal(
    const chromeos::MobileConfig::Carrier* carrier) {
  const chromeos::MobileConfig::CarrierDeal* deal = carrier->GetDefaultDeal();
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

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// DataPromoNotification

DataPromoNotification::DataPromoNotification()
    : check_for_promo_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

DataPromoNotification::~DataPromoNotification() {
  CloseNotification();
}

void DataPromoNotification::RegisterPrefs(PrefServiceSimple* local_state) {
  // Carrier deal notification shown count defaults to 0.
  local_state->RegisterIntegerPref(prefs::kCarrierDealPromoShown, 0);
}

void DataPromoNotification::ShowOptionalMobileDataPromoNotification(
    NetworkLibrary* cros,
    views::View* host,
    ash::NetworkTrayDelegate* listener) {
  // Display one-time notification for non-Guest users on first use
  // of Mobile Data connection or if there's a carrier deal defined
  // show that even if user has already seen generic promo.
  if (UserManager::Get()->IsUserLoggedIn() &&
      !UserManager::Get()->IsLoggedInAsGuest() &&
      check_for_promo_ &&
      cros->cellular_connected() && !cros->ethernet_connected() &&
      !cros->wifi_connected() && !cros->wimax_connected()) {
    std::string deal_text;
    int carrier_deal_promo_pref = kNotificationCountPrefDefault;
    const MobileConfig::CarrierDeal* deal = NULL;
    const MobileConfig::Carrier* carrier = GetCarrier(cros);
    if (carrier)
      deal = GetCarrierDeal(carrier);
    deal_info_url_.clear();
    deal_topup_url_.clear();
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

    gfx::Rect button_bounds = host->GetBoundsInScreen();
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
                &DataPromoNotification::ShowOptionalMobileDataPromoNotification,
                weak_ptr_factory_.GetWeakPtr(),
                cros,
                host,
                listener),
            base::TimeDelta::FromMilliseconds(kPromoShowDelayMs));
      }
      return;
    }

    string16 message = l10n_util::GetStringUTF16(IDS_3G_NOTIFICATION_MESSAGE);
    if (!deal_text.empty())
      message = UTF8ToUTF16(deal_text + "\n\n") + message;

    // Use deal URL if it's defined or general "Network Settings" URL.
    int link_message_id;
    if (deal_topup_url_.empty())
      link_message_id = IDS_OFFLINE_NETWORK_SETTINGS;
    else
      link_message_id = IDS_STATUSBAR_NETWORK_VIEW_ACCOUNT;

    ash::NetworkObserver::NetworkType type =
        ash::NetworkObserver::NETWORK_CELLULAR;
    const chromeos::CellularNetwork* net = cros->cellular_network();
    if (net && (net->network_technology() == NETWORK_TECHNOLOGY_LTE ||
            net->network_technology() == NETWORK_TECHNOLOGY_LTE_ADVANCED)) {
      type = ash::NetworkObserver::NETWORK_CELLULAR_LTE;
    }

    std::vector<string16> links;
    links.push_back(l10n_util::GetStringUTF16(link_message_id));
    if (!deal_info_url_.empty())
      links.push_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    ash::Shell::GetInstance()->system_tray_notifier()->NotifySetNetworkMessage(
        listener, ash::NetworkObserver::MESSAGE_DATA_PROMO,
        type, string16(), message, links);
    check_for_promo_ = false;
    SetShow3gPromoNotification(false);
    if (carrier_deal_promo_pref != kNotificationCountPrefDefault)
      SetCarrierDealPromoShown(carrier_deal_promo_pref + 1);
  }
}

void DataPromoNotification::CloseNotification() {
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyClearNetworkMessage(
      ash::NetworkObserver::MESSAGE_DATA_PROMO);
}

}  // namespace chromeos
