// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_notification_controller.h"

#include "ash/common/strings/grit/ash_strings.h"
#include "ash/common/system/system_notifier.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const char kDelegateId[] = "quickunlock_delegate";
const char kNotificationId[] = "quickunlock_notification";
const char kChromeAuthenticationSettingsURL[] =
    "chrome://md-settings/quickUnlock/authenticate";

void UpdatePreferenceForProfile(Profile* profile) {
  PrefService* pref_service = profile->GetPrefs();
  pref_service->SetBoolean(prefs::kQuickUnlockFeatureNotificationShown, true);
}

}  // namespace

namespace chromeos {

QuickUnlockNotificationController::QuickUnlockNotificationController(
    Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                 content::NotificationService::AllSources());
}

QuickUnlockNotificationController::~QuickUnlockNotificationController() {
  UnregisterObserver();
}

// static
// TODO(http://crbug.com/291747): Add check for a policy that might disable
// quick unlock.
bool QuickUnlockNotificationController::ShouldShow(Profile* profile) {
  // Do not show notification if this is a guest session.
  if (profile->IsGuestSession())
    return false;

  // Do not show notification to user if already displayed in the past.
  if (profile->GetPrefs()->GetBoolean(
          prefs::kQuickUnlockFeatureNotificationShown)) {
    return false;
  }

  // Do not show the notification if the pin is already set.
  PinStorage* pin_storage = PinStorageFactory::GetForProfile(profile);
  if (pin_storage->IsPinSet())
    return false;

  // TODO(jdufault): Enable once quick unlock settings land(crbug.com/291747).
  return false;
}

// NotificationDelegate override:
std::string QuickUnlockNotificationController::id() const {
  return kDelegateId;
}

void QuickUnlockNotificationController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED)
    return;

  bool is_screen_locked = *content::Details<bool>(details).ptr();

  // Return if the screen is locked, indicating that the notification was
  // emitted due to a screen lock event.
  if (is_screen_locked)
    return;

  UnregisterObserver();

  // The user may have enabled the quick unlock feature during the current
  // session and after the notificaiton controller has already been initialized.
  if (!ShouldShow(profile_)) {
    UpdatePreferenceForProfile(profile_);
    return;
  }

  // Create and add notification to notification manager.
  std::unique_ptr<Notification> notification(CreateNotification());
  g_browser_process->notification_ui_manager()->Add(*notification, profile_);
}

// message_center::NotificationDelegate override:
void QuickUnlockNotificationController::Close(bool by_user) {
  if (by_user)
    UpdatePreferenceForProfile(profile_);
}

// message_center::NotificationDelegate override:
void QuickUnlockNotificationController::Click() {
  chrome::NavigateParams params(profile_,
                                GURL(kChromeAuthenticationSettingsURL),
                                ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);

  UpdatePreferenceForProfile(profile_);

  // Remove the notification from tray.
  g_browser_process->notification_ui_manager()->CancelById(
      id(), NotificationUIManager::GetProfileID(profile_));
}

void QuickUnlockNotificationController::UnregisterObserver() {
  if (registrar_.IsRegistered(this,
                              chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                              content::NotificationService::AllSources())) {
    registrar_.Remove(this, chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                      content::NotificationService::AllSources());
  }
}

Notification* QuickUnlockNotificationController::CreateNotification() {
  return new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      l10n_util::GetStringUTF16(IDS_ASH_QUICK_UNLOCK_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_QUICK_UNLOCK_NOTIFICATION_BODY),
      // TODO(http://crbug.com/291747): Change this to actual icon for
      // quick unlock feature notification.
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_SCREENSHOT_NOTIFICATION_ICON),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 ash::system_notifier::kNotifierQuickUnlock),
      l10n_util::GetStringUTF16(
          IDS_MESSAGE_CENTER_NOTIFIER_QUICK_UNLOCK_FEATURE_NAME),
      GURL(), kNotificationId, message_center::RichNotificationData(), this);
}

}  // namespace chromeos
