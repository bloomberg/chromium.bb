// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/data_promo_notification.h"

#include "ash/system/chromeos/network/network_connect.h"
#include "ash/system/system_notifier.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

const int kNotificationCountPrefDefault = -1;

bool GetBooleanPref(const char* pref_name) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  PrefService* prefs = profile->GetPrefs();
  return prefs->GetBoolean(pref_name);
}

int GetIntegerLocalPref(const char* pref_name) {
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetInteger(pref_name);
}

void SetBooleanPref(const char* pref_name, bool value) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(pref_name, value);
}

void SetIntegerLocalPref(const char* pref_name, int value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(pref_name, value);
}

// Returns prefs::kShow3gPromoNotification or false if no active browser.
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
    const NetworkState* cellular) {
  const DeviceState* device = NetworkHandler::Get()->network_state_handler()->
      GetDeviceState(cellular->device_path());
  std::string carrier_id = device ? device->home_provider_id() : "";
  if (carrier_id.empty()) {
    NET_LOG_ERROR("Empty carrier ID for cellular network",
                  device ? device->path(): "No device");
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
    NET_LOG_DEBUG("Carrier Deal Found", deal_text);
    if (deal_text.empty())
      return NULL;
  }
  return deal;
}

void NotificationClicked(const std::string& service_path,
                         const std::string& info_url) {
  if (info_url.empty())
    ash::network_connect::ShowNetworkSettings(service_path);

  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetPrimaryUserProfile(),
      chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::ShowSingletonTab(displayer.browser(), GURL(info_url));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DataPromoNotification

DataPromoNotification::DataPromoNotification()
    : check_for_promo_(true),
      weak_ptr_factory_(this) {
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
}

DataPromoNotification::~DataPromoNotification() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
}

void DataPromoNotification::RegisterPrefs(PrefRegistrySimple* registry) {
  // Carrier deal notification shown count defaults to 0.
  registry->RegisterIntegerPref(prefs::kCarrierDealPromoShown, 0);
}

void DataPromoNotification::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (!network || network->type() != shill::kTypeCellular)
    return;
  ShowOptionalMobileDataPromoNotification();
}

void DataPromoNotification::DefaultNetworkChanged(const NetworkState* network) {
  // Call NetworkPropertiesUpdated in case the Cellular network became the
  // default network.
  NetworkPropertiesUpdated(network);
}

void DataPromoNotification::ShowOptionalMobileDataPromoNotification() {
  // Display a one-time notification for authenticated users on first use
  // of Mobile Data connection or if there is a carrier deal defined
  // show that even if user has already seen generic promo.
  if (!check_for_promo_ || !LoginState::Get()->IsUserAuthenticated())
    return;
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network || default_network->type() != shill::kTypeCellular)
    return;
  // When requesting a network connection, do not show the notification.
  if (NetworkHandler::Get()->network_connection_handler()->
      HasPendingConnectRequest())
    return;

  int carrier_deal_promo_pref = kNotificationCountPrefDefault;
  const MobileConfig::CarrierDeal* deal = NULL;
  const MobileConfig::Carrier* carrier = GetCarrier(default_network);
  if (carrier)
    deal = GetCarrierDeal(carrier);

  base::string16 message = l10n_util::GetStringUTF16(IDS_3G_NOTIFICATION_MESSAGE);
  std::string info_url;
  if (deal) {
    carrier_deal_promo_pref = GetCarrierDealPromoShown();
    const std::string locale = g_browser_process->GetApplicationLocale();
    std::string deal_text =
        deal->GetLocalizedString(locale, "notification_text");
    message = base::UTF8ToUTF16(deal_text + "\n\n") + message;
    info_url = deal->info_url();
    if (info_url.empty() && carrier)
      info_url = carrier->top_up_url();
  } else if (!ShouldShow3gPromoNotification()) {
    check_for_promo_ = false;
    return;
  }

  int icon_id;
  if (default_network->network_technology() == shill::kNetworkTechnologyLte)
    icon_id = IDR_AURA_UBER_TRAY_NOTIFICATION_LTE;
  else
    icon_id = IDR_AURA_UBER_TRAY_NOTIFICATION_3G;
  const gfx::Image& icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(icon_id);

  message_center::MessageCenter::Get()->AddNotification(
      message_center::Notification::CreateSystemNotification(
          ash::network_connect::kNetworkActivateNotificationId,
          base::string16() /* title */,
          message,
          icon,
          ash::system_notifier::kNotifierNetwork,
          base::Bind(&NotificationClicked,
                     default_network->path(), info_url)));

  check_for_promo_ = false;
  SetShow3gPromoNotification(false);
  if (carrier_deal_promo_pref != kNotificationCountPrefDefault)
    SetCarrierDealPromoShown(carrier_deal_promo_pref + 1);
}

}  // namespace chromeos
