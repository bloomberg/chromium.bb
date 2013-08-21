// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/data_promo_notification.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/chromeos/network/network_connect.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

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
    const NetworkState* cellular) {
  const DeviceState* device = NetworkHandler::Get()->network_state_handler()->
      GetDeviceState(cellular->device_path());
  std::string carrier_id = device->home_provider_id();
  if (carrier_id.empty()) {
    NET_LOG_ERROR("Empty carrier ID with a cellular device", device->path());
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

ash::NetworkObserver::NetworkType NetworkTypeForNetwork(
    const NetworkState* network) {
  DCHECK(network);
  const std::string& technology = network->network_technology();
  return (technology == flimflam::kNetworkTechnologyLte ||
          technology == flimflam::kNetworkTechnologyLteAdvanced)
      ? ash::NetworkObserver::NETWORK_CELLULAR_LTE
      : ash::NetworkObserver::NETWORK_CELLULAR;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DataPromoNotification

DataPromoNotification::DataPromoNotification()
    : check_for_promo_(true),
      cellular_activating_(false),
      weak_ptr_factory_(this) {
  UpdateCellularActivating();
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
}

DataPromoNotification::~DataPromoNotification() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
  CloseNotification();
}

void DataPromoNotification::RegisterPrefs(PrefRegistrySimple* registry) {
  // Carrier deal notification shown count defaults to 0.
  registry->RegisterIntegerPref(prefs::kCarrierDealPromoShown, 0);
}

void DataPromoNotification::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (!network || network->type() != flimflam::kTypeCellular)
    return;
  ShowOptionalMobileDataPromoNotification();
  UpdateCellularActivating();
}

void DataPromoNotification::DefaultNetworkChanged(const NetworkState* network) {
  // Call NetworkPropertiesUpdated in case the Cellular network became the
  // default network.
  NetworkPropertiesUpdated(network);
}

void DataPromoNotification::NotificationLinkClicked(
    ash::NetworkObserver::MessageType message_type,
    size_t link_index) {
  const NetworkState* cellular =
      NetworkHandler::Get()->network_state_handler()->
      FirstNetworkByType(flimflam::kTypeCellular);
  std::string service_path = cellular ? cellular->path() : "";
  if (message_type == ash::NetworkObserver::ERROR_OUT_OF_CREDITS) {
    ash::network_connect::ShowNetworkSettings(service_path);
    ash::Shell::GetInstance()->system_tray_notifier()->
        NotifyClearNetworkMessage(message_type);
  }
  if (message_type != ash::NetworkObserver::MESSAGE_DATA_PROMO)
    return;

  // If we have a deal info URL defined that means that there are
  // two links in the bubble. Let the user close it manually then giving the
  // ability to navigate to the second link.
  if (deal_info_url_.empty())
    CloseNotification();

  std::string deal_url_to_open;
  if (link_index == 0) {
    if (!deal_topup_url_.empty()) {
      deal_url_to_open = deal_topup_url_;
    } else {
      ash::network_connect::ShowNetworkSettings(service_path);
      return;
    }
  } else if (link_index == 1) {
    deal_url_to_open = deal_info_url_;
  }

  if (!deal_url_to_open.empty()) {
    Browser* browser = chrome::FindOrCreateTabbedBrowser(
        ProfileManager::GetDefaultProfileOrOffTheRecord(),
        chrome::HOST_DESKTOP_TYPE_ASH);
    if (!browser)
      return;
    chrome::ShowSingletonTab(browser, GURL(deal_url_to_open));
  }
}

void DataPromoNotification::UpdateCellularActivating() {
  // We only care about the first (default) cellular network.
  const NetworkState* cellular =
      NetworkHandler::Get()->network_state_handler()->
      FirstNetworkByType(flimflam::kTypeCellular);
  if (!cellular)
    return;

  std::string activation_state = cellular->activation_state();
  if (activation_state == flimflam::kActivationStateActivating) {
    cellular_activating_ = true;
  } else if (cellular_activating_ &&
             activation_state == flimflam::kActivationStateActivated) {
    cellular_activating_ = false;
    ash::Shell::GetInstance()->system_tray_notifier()->
        NotifySetNetworkMessage(
            NULL,
            ash::NetworkObserver::MESSAGE_DATA_PROMO,
            NetworkTypeForNetwork(cellular),
            l10n_util::GetStringUTF16(IDS_NETWORK_CELLULAR_ACTIVATED_TITLE),
            l10n_util::GetStringFUTF16(IDS_NETWORK_CELLULAR_ACTIVATED,
                                       UTF8ToUTF16((cellular->name()))),
            std::vector<string16>());
  }
}

void DataPromoNotification::ShowOptionalMobileDataPromoNotification() {
  // Display a one-time notification for authenticated users on first use
  // of Mobile Data connection or if there is a carrier deal defined
  // show that even if user has already seen generic promo.
  if (!check_for_promo_ || !LoginState::Get()->IsUserAuthenticated())
    return;
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network || default_network->type() != flimflam::kTypeCellular)
    return;
  // When requesting a network connection, do not show the notification.
  if (NetworkHandler::Get()->network_connection_handler()->
      HasPendingConnectRequest())
    return;

  std::string deal_text;
  int carrier_deal_promo_pref = kNotificationCountPrefDefault;
  const MobileConfig::CarrierDeal* deal = NULL;
  const MobileConfig::Carrier* carrier = GetCarrier(default_network);
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
      NetworkTypeForNetwork(default_network);

  std::vector<string16> links;
  links.push_back(l10n_util::GetStringUTF16(link_message_id));
  if (!deal_info_url_.empty())
    links.push_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  ash::Shell::GetInstance()->system_tray_notifier()->NotifySetNetworkMessage(
      this, ash::NetworkObserver::MESSAGE_DATA_PROMO,
      type, string16(), message, links);
  check_for_promo_ = false;
  SetShow3gPromoNotification(false);
  if (carrier_deal_promo_pref != kNotificationCountPrefDefault)
    SetCarrierDealPromoShown(carrier_deal_promo_pref + 1);
}

void DataPromoNotification::CloseNotification() {
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyClearNetworkMessage(
      ash::NetworkObserver::MESSAGE_DATA_PROMO);
}

}  // namespace chromeos
