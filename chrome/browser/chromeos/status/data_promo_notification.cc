// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/data_promo_notification.h"

#include "ash/system/system_notifier.h"
#include "base/command_line.h"
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
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_registry.h"
#include "grit/ui_chromeos_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/network/network_connect.h"
#include "ui/chromeos/network/network_state_notifier.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

const char kDataPromoNotificationId[] = "chrome://settings/internet/data_promo";
const char kDataSaverNotificationId[] = "chrome://settings/internet/data_saver";
const char kDataSaverExtensionUrl[] =
    "https://chrome.google.com/webstore/detail/"
    "pfmgfdlgomnbgkofeojodiodmgpgmkac?utm_source=chromeos-datasaver-prompt";

const int kNotificationCountPrefDefault = -1;
const int kTimesToShowDataSaverPrompt = 2;

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

// Returns number of times the Data Saver prompt has been displayed.
int GetDataSaverPromptsShown() {
  return ProfileManager::GetActiveUserProfile()->GetPrefs()->GetInteger(
      prefs::kDataSaverPromptsShown);
}

// Updates number of times the Data Saver prompt has been displayed.
void SetDataSaverPromptsShown(int times_shown) {
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetInteger(
      prefs::kDataSaverPromptsShown, times_shown);
}

// kDisableDataSaverPrompt takes precedence over any kEnableDataSaverPrompt
// value. If neither flag is set, the Data Saver prompt is enabled by default.
bool DataSaverSwitchEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableDataSaverPrompt);
}

// Is command line switch set for Data Saver demo mode, where we show the prompt
// after any change in network properties?
bool DataSaverSwitchDemoMode() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableDataSaverPrompt))
    return false;

  const std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kEnableDataSaverPrompt);
  return value == chromeos::switches::kDataSaverPromptDemoMode;
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
  if (!info_url.empty()) {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        ProfileManager::GetPrimaryUserProfile());
    chrome::ShowSingletonTab(displayer.browser(), GURL(info_url));
    if (info_url == kDataSaverExtensionUrl)
      content::RecordAction(base::UserMetricsAction("DataSaverPrompt_Clicked"));
  } else {
    ui::NetworkConnect::Get()->ShowNetworkSettingsForPath(service_path);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DataPromoNotification

DataPromoNotification::DataPromoNotification()
    : notifications_shown_(false),
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
  if (!network ||
      (network->type() != shill::kTypeCellular && !DataSaverSwitchDemoMode()))
    return;
  ShowOptionalMobileDataPromoNotification();
}

void DataPromoNotification::DefaultNetworkChanged(const NetworkState* network) {
  // Call NetworkPropertiesUpdated in case the Cellular network became the
  // default network.
  NetworkPropertiesUpdated(network);
}

void DataPromoNotification::ShowOptionalMobileDataPromoNotification() {
  // Do not show notifications to unauthenticated users, or when requesting a
  // network connection, or if there's no default_network.
  if (!LoginState::Get()->IsUserAuthenticated())
    return;
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network)
    return;
  if (NetworkHandler::Get()->network_connection_handler()->
      HasPendingConnectRequest())
    return;

  if (!DataSaverSwitchDemoMode() &&
      (notifications_shown_ || default_network->type() != shill::kTypeCellular))
    return;

  notifications_shown_ = true;
  bool data_saver_prompt_shown = ShowDataSaverNotification();

  // Display a one-time notification on first use of Mobile Data connection, or
  // if there is a carrier deal defined show that even if user has already seen
  // generic promo.  Show deal regardless of |data_saver_prompt_shown|.
  int carrier_deal_promo_pref = kNotificationCountPrefDefault;
  const MobileConfig::CarrierDeal* deal = NULL;
  const MobileConfig::Carrier* carrier = GetCarrier(default_network);
  if (carrier)
    deal = GetCarrierDeal(carrier);

  base::string16 message =
      l10n_util::GetStringUTF16(IDS_3G_NOTIFICATION_MESSAGE);
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
  } else if (data_saver_prompt_shown || !ShouldShow3gPromoNotification()) {
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
          kDataPromoNotificationId, base::string16() /* title */, message, icon,
          ui::NetworkStateNotifier::kNotifierNetwork,
          base::Bind(&NotificationClicked, default_network->path(), info_url)));

  SetShow3gPromoNotification(false);
  if (carrier_deal_promo_pref != kNotificationCountPrefDefault)
    SetCarrierDealPromoShown(carrier_deal_promo_pref + 1);
}

bool DataPromoNotification::ShowDataSaverNotification() {
  if (!DataSaverSwitchEnabled())
    return false;

  if (message_center::MessageCenter::Get()->FindVisibleNotificationById(
          kDataSaverNotificationId))  // already showing.
    return true;

  int times_shown = GetDataSaverPromptsShown();
  if (!DataSaverSwitchDemoMode() && times_shown >= kTimesToShowDataSaverPrompt)
    return false;

  if (extensions::ExtensionRegistry::Get(ProfileManager::GetActiveUserProfile())
          ->GetExtensionById(extension_misc::kDataSaverExtensionId,
                             extensions::ExtensionRegistry::EVERYTHING)) {
    // If extension is installed, disable future prompts.
    SetDataSaverPromptsShown(kTimesToShowDataSaverPrompt);
    return false;
  }

  base::string16 title = l10n_util::GetStringUTF16(IDS_3G_DATASAVER_TITLE);
  base::string16 message = l10n_util::GetStringUTF16(IDS_3G_DATASAVER_MESSAGE);
  const gfx::Image& icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_AURA_UBER_TRAY_NOTIFICATION_DATASAVER);

  message_center::MessageCenter::Get()->AddNotification(
      message_center::Notification::CreateSystemNotification(
          kDataSaverNotificationId, title, message, icon,
          ui::NetworkStateNotifier::kNotifierNetwork,
          base::Bind(&NotificationClicked, "", kDataSaverExtensionUrl)));
  content::RecordAction(base::UserMetricsAction("DataSaverPrompt_Shown"));

  if (DataSaverSwitchDemoMode()) {
    SetDataSaverPromptsShown(0);  // demo mode resets times shown counts.
    SetShow3gPromoNotification(true);
  } else {
    SetDataSaverPromptsShown(times_shown + 1);
  }

  return true;
}

}  // namespace chromeos
