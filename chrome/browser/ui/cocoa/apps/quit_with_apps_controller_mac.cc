// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"

#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_window_registry_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "grit/chrome_unscaled_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

using extensions::ExtensionRegistry;

const char kQuitWithAppsOriginUrl[] = "chrome://quit-with-apps";
const int kQuitAllAppsButtonIndex = 0;
const int kDontShowAgainButtonIndex = 1;

const char QuitWithAppsController::kQuitWithAppsNotificationID[] =
    "quit-with-apps";

QuitWithAppsController::QuitWithAppsController()
    : notification_profile_(NULL), suppress_for_session_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  hosted_app_quit_notification_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kHostedAppQuitNotification);

  // There is only ever one notification to replace, so use the same tag
  // each time.
  std::string tag = id();

  message_center::ButtonInfo quit_apps_button_info(
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_QUIT_LABEL));
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(quit_apps_button_info);
  if (!hosted_app_quit_notification_) {
    message_center::ButtonInfo suppression_button_info(
        l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_SUPPRESSION_LABEL));
    rich_notification_data.buttons.push_back(suppression_button_info);
  }

  notification_.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_TITLE),
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_EXPLANATION),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PRODUCT_LOGO_128),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kQuitWithAppsNotificationID),
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_NOTIFICATION_DISPLAY_SOURCE),
      GURL(kQuitWithAppsOriginUrl), tag, rich_notification_data, this));
}

QuitWithAppsController::~QuitWithAppsController() {}

void QuitWithAppsController::Display() {}

void QuitWithAppsController::Close(bool by_user) {
  if (by_user) {
    suppress_for_session_ = hosted_app_quit_notification_ ? false : true;
  }
}

void QuitWithAppsController::Click() {
  g_browser_process->notification_ui_manager()->CancelById(
      id(), NotificationUIManager::GetProfileID(notification_profile_));
}

void QuitWithAppsController::ButtonClick(int button_index) {
  g_browser_process->notification_ui_manager()->CancelById(
      id(), NotificationUIManager::GetProfileID(notification_profile_));

  if (button_index == kQuitAllAppsButtonIndex) {
    if (hosted_app_quit_notification_) {
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
          content::NotificationService::AllSources(),
          content::NotificationService::NoDetails());
      chrome::CloseAllBrowsers();
    }
    AppWindowRegistryUtil::CloseAllAppWindows();
  } else if (button_index == kDontShowAgainButtonIndex &&
             !hosted_app_quit_notification_) {
    g_browser_process->local_state()->SetBoolean(
        prefs::kNotifyWhenAppsKeepChromeAlive, false);
  }
}

std::string QuitWithAppsController::id() const {
  return kQuitWithAppsNotificationID;
}

bool QuitWithAppsController::ShouldQuit() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Quit immediately if this is a test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAppsKeepChromeAliveInTests)) {
    return true;
  }

  // Quit immediately if Chrome is restarting.
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kRestartLastSessionOnShutdown)) {
    return true;
  }

  if (hosted_app_quit_notification_) {
    bool hosted_apps_open = false;
    for (Browser* browser : *BrowserList::GetInstance()) {
      if (!browser->is_app())
        continue;

      ExtensionRegistry* registry = ExtensionRegistry::Get(browser->profile());
      const extensions::Extension* extension = registry->GetExtensionById(
          web_app::GetExtensionIdFromApplicationName(browser->app_name()),
          ExtensionRegistry::ENABLED);
      if (extension->is_hosted_app()) {
        hosted_apps_open = true;
        break;
      }
    }

    // Quit immediately if there are no packaged app windows or hosted apps open
    // or the confirmation has been suppressed. Ignore panels.
    if (!AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(
            extensions::AppWindow::WINDOW_TYPE_DEFAULT) &&
        !hosted_apps_open) {
      return true;
    }
  } else {
    // Quit immediately if there are no windows or the confirmation has been
    // suppressed.
    if (!AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(
            extensions::AppWindow::WINDOW_TYPE_DEFAULT))
      return true;
  }

  // If there are browser windows, and this notification has been suppressed for
  // this session or permanently, then just return false to prevent Chrome from
  // quitting. If there are no browser windows, always show the notification.
  bool suppress_always = !g_browser_process->local_state()->GetBoolean(
      prefs::kNotifyWhenAppsKeepChromeAlive);
  if (!BrowserList::GetInstance()->empty() &&
      (suppress_for_session_ || suppress_always)) {
    return false;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  DCHECK(profiles.size());

  // Delete any existing notification to ensure this one is shown. If
  // notification_profile_ is NULL then it must be that no notification has been
  // added by this class yet.
  if (notification_profile_) {
    g_browser_process->notification_ui_manager()->CancelById(
        id(), NotificationUIManager::GetProfileID(notification_profile_));
  }
  notification_profile_ = profiles[0];
  g_browser_process->notification_ui_manager()->Add(*notification_,
                                                    notification_profile_);

  // Always return false, the notification UI can be used to quit all apps which
  // will cause Chrome to quit.
  return false;
}

// static
void QuitWithAppsController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNotifyWhenAppsKeepChromeAlive, true);
}
