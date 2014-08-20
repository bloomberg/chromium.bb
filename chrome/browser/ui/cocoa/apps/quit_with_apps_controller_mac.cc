// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "grit/chrome_unscaled_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

const char kQuitWithAppsOriginUrl[] = "chrome://quit-with-apps";
const int kQuitAllAppsButtonIndex = 0;
const int kDontShowAgainButtonIndex = 1;

const char QuitWithAppsController::kQuitWithAppsNotificationID[] =
    "quit-with-apps";

QuitWithAppsController::QuitWithAppsController()
    : suppress_for_session_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // There is only ever one notification to replace, so use the same replace_id
  // each time.
  base::string16 replace_id = base::UTF8ToUTF16(id());

  message_center::ButtonInfo quit_apps_button_info(
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_QUIT_LABEL));
  message_center::ButtonInfo suppression_button_info(
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_SUPPRESSION_LABEL));
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(quit_apps_button_info);
  rich_notification_data.buttons.push_back(suppression_button_info);

  notification_.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GURL(kQuitWithAppsOriginUrl),
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_TITLE),
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_EXPLANATION),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PRODUCT_LOGO_128),
      blink::WebTextDirectionDefault,
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kQuitWithAppsNotificationID),
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_NOTIFICATION_DISPLAY_SOURCE),
      replace_id,
      rich_notification_data,
      this));
}

QuitWithAppsController::~QuitWithAppsController() {}

void QuitWithAppsController::Display() {}

void QuitWithAppsController::Error() {}

void QuitWithAppsController::Close(bool by_user) {
  if (by_user)
    suppress_for_session_ = true;
}

void QuitWithAppsController::Click() {
  g_browser_process->notification_ui_manager()->CancelById(id());
}

void QuitWithAppsController::ButtonClick(int button_index) {
  g_browser_process->notification_ui_manager()->CancelById(id());
  if (button_index == kQuitAllAppsButtonIndex) {
    apps::AppWindowRegistry::CloseAllAppWindows();
  } else if (button_index == kDontShowAgainButtonIndex) {
    g_browser_process->local_state()->SetBoolean(
        prefs::kNotifyWhenAppsKeepChromeAlive, false);
  }
}

content::WebContents* QuitWithAppsController::GetWebContents() const {
  return NULL;
}

std::string QuitWithAppsController::id() const {
  return kQuitWithAppsNotificationID;
}

bool QuitWithAppsController::ShouldQuit() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Quit immediately if this is a test.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAppsKeepChromeAliveInTests)) {
    return true;
  }

  // Quit immediately if there are no windows or the confirmation has been
  // suppressed.
  if (!apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0))
    return true;

  // If there are browser windows, and this notification has been suppressed for
  // this session or permanently, then just return false to prevent Chrome from
  // quitting. If there are no browser windows, always show the notification.
  bool suppress_always = !g_browser_process->local_state()->GetBoolean(
      prefs::kNotifyWhenAppsKeepChromeAlive);
  if (!chrome::BrowserIterator().done() &&
      (suppress_for_session_ || suppress_always)) {
    return false;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  DCHECK(profiles.size());

  // Delete any existing notification to ensure this one is shown.
  g_browser_process->notification_ui_manager()->CancelById(id());
  g_browser_process->notification_ui_manager()->Add(*notification_,
                                                    profiles[0]);

  // Always return false, the notification UI can be used to quit all apps which
  // will cause Chrome to quit.
  return false;
}

// static
void QuitWithAppsController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNotifyWhenAppsKeepChromeAlive, true);
}
