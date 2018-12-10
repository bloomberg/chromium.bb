// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/data_promo_notification.h"

#include <memory>
#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/network/network_state_notifier.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
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
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;

namespace {

const char kMobileDataNotificationId[] =
    "chrome://settings/internet/mobile_data";
const char kNotifierMobileData[] = "ash.mobile-data";
const char kDataSaverNotificationId[] = "chrome://settings/internet/data_saver";
const char kNotifierDataSaver[] = "ash.data-saver";
const char kDataSaverExtensionUrl[] =
    "https://chrome.google.com/webstore/detail/"
    "pfmgfdlgomnbgkofeojodiodmgpgmkac?utm_source=chromeos-datasaver-prompt";

const int kTimesToShowDataSaverPrompt = 2;

Profile* GetProfileForNotifications() {
  return ProfileManager::GetActiveUserProfile();
}

// Returns prefs::kShowMobileDataNotification
bool ShouldShowMobileDataNotification() {
  return GetProfileForNotifications()->GetPrefs()->GetBoolean(
      prefs::kShowMobileDataNotification);
}

void SetShowMobileDataNotification(bool value) {
  GetProfileForNotifications()->GetPrefs()->SetBoolean(
      prefs::kShowMobileDataNotification, value);
}

// Returns number of times the Data Saver prompt has been displayed.
int GetDataSaverPromptsShown() {
  return GetProfileForNotifications()->GetPrefs()->GetInteger(
      prefs::kDataSaverPromptsShown);
}

// Updates number of times the Data Saver prompt has been displayed.
void SetDataSaverPromptsShown(int times_shown) {
  GetProfileForNotifications()->GetPrefs()->SetInteger(
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

void DataSaverNotificationClicked() {
  chrome::ScopedTabbedBrowserDisplayer displayer(GetProfileForNotifications());
  ShowSingletonTab(displayer.browser(), GURL(kDataSaverExtensionUrl));
  base::RecordAction(base::UserMetricsAction("DataSaverPrompt_Clicked"));
}

void MobileDataNotificationClicked(const std::string& network_id) {
  SystemTrayClient::Get()->ShowNetworkSettings(network_id);
}

void DisplayNotification(const std::string& notification_id,
                         const std::string& notifier_id,
                         const base::string16& message,
                         const base::string16& title,
                         const base::RepeatingClosure& callback) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
          message, base::string16() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, notifier_id),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              callback),
          ash::kNotificationMobileDataIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  NotificationDisplayService::GetForProfile(GetProfileForNotifications())
      ->Display(NotificationHandler::Type::TRANSIENT, *notification);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DataPromoNotification

DataPromoNotification::DataPromoNotification()
    : notifications_shown_(false), weak_ptr_factory_(this) {
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
}

DataPromoNotification::~DataPromoNotification() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

void DataPromoNotification::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (!network ||
      (network->type() != shill::kTypeCellular && !DataSaverSwitchDemoMode()))
    return;
  ShowOptionalMobileDataNotification();
}

void DataPromoNotification::DefaultNetworkChanged(const NetworkState* network) {
  // Call NetworkPropertiesUpdated in case the Cellular network became the
  // default network.
  NetworkPropertiesUpdated(network);
}

void DataPromoNotification::ShowOptionalMobileDataNotification() {
  if (notifications_shown_)
    return;

  // Do not show notifications to unauthenticated users, or when requesting a
  // network connection, or if there's no default_network.
  if (!chromeos::LoginState::Get()->IsUserAuthenticated())
    return;
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network)
    return;
  if (NetworkHandler::Get()
          ->network_connection_handler()
          ->HasPendingConnectRequest())
    return;

  if (!DataSaverSwitchDemoMode() &&
      default_network->type() != shill::kTypeCellular)
    return;

  notifications_shown_ = true;
  if (ShouldShowDataSaverNotification()) {
    DisplayNotification(kDataSaverNotificationId, kNotifierDataSaver,
                        l10n_util::GetStringUTF16(IDS_3G_DATASAVER_TITLE),
                        l10n_util::GetStringUTF16(IDS_3G_DATASAVER_MESSAGE),
                        base::BindRepeating(&DataSaverNotificationClicked));

    base::RecordAction(base::UserMetricsAction("DataSaverPrompt_Shown"));
    if (DataSaverSwitchDemoMode()) {
      SetShowMobileDataNotification(true);
      SetDataSaverPromptsShown(0);  // demo mode resets times shown counts.
    } else {
      SetDataSaverPromptsShown(GetDataSaverPromptsShown() + 1);
    }
  } else if (ShouldShowMobileDataNotification()) {
    // Display a one-time notification on first use of Mobile Data connection
    DisplayNotification(
        kMobileDataNotificationId, kNotifierMobileData,
        l10n_util::GetStringUTF16(IDS_MOBILE_DATA_NOTIFICATION_TITLE),
        l10n_util::GetStringUTF16(IDS_3G_NOTIFICATION_MESSAGE),
        base::BindRepeating(&MobileDataNotificationClicked,
                            default_network->guid()));

    // Update local state to disable mobile usage warning notification.
    SetShowMobileDataNotification(false);
  }
}

bool DataPromoNotification::ShouldShowDataSaverNotification() {
  if (!DataSaverSwitchEnabled())
    return false;

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
  return true;
}
