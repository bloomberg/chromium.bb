// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_notification_controller.h"

#include "ash/common/system/system_notifier.h"
#include "ash/strings/grit/ash_strings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
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

namespace chromeos {
namespace quick_unlock {
namespace {

constexpr char kPinDelegateId[] = "pinunlock_delegate";
constexpr char kPinNotificationId[] = "pinunlock_notification";
constexpr char kPinSetupUrl[] = "chrome://md-settings/lockScreen";

}  // namespace

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
QuickUnlockNotificationController*
QuickUnlockNotificationController::CreateForPin(Profile* profile) {
  QuickUnlockNotificationController* controller =
      new QuickUnlockNotificationController(profile);

  // Set the PIN notification parameters.
  controller->params_.delegate_id = kPinDelegateId;
  controller->params_.title_message_id =
      IDS_ASH_QUICK_UNLOCK_NOTIFICATION_TITLE;
  controller->params_.body_message_id = IDS_ASH_QUICK_UNLOCK_NOTIFICATION_BODY;
  controller->params_.icon_id = IDR_SCREENSHOT_NOTIFICATION_ICON;
  controller->params_.notifier = ash::system_notifier::kNotifierQuickUnlock;
  controller->params_.feature_name_id =
      IDS_MESSAGE_CENTER_NOTIFIER_QUICK_UNLOCK_FEATURE_NAME;
  controller->params_.notification_id = kPinNotificationId;
  controller->params_.url = GURL(kPinSetupUrl);
  controller->params_.was_shown_pref_id =
      prefs::kQuickUnlockFeatureNotificationShown;

  controller->should_show_notification_callback_ =
      base::Bind(&QuickUnlockNotificationController::ShouldShowPinNotification);

  return controller;
}

// static
bool QuickUnlockNotificationController::ShouldShowPinNotification(
    Profile* profile) {
  // Do not show notification if this is a guest session.
  if (profile->IsGuestSession())
    return false;

  // Do not show notification to user if already displayed in the past.
  if (profile->GetPrefs()->GetBoolean(
          prefs::kQuickUnlockFeatureNotificationShown)) {
    return false;
  }

  // Do not show notification if policy does not allow PIN, or if user is
  // supervised.
  if (!IsPinEnabled(profile->GetPrefs()))
    return false;

  // Do not show the notification if the pin is already set.
  PinStorage* pin_storage =
      QuickUnlockFactory::GetForProfile(profile)->pin_storage();
  if (pin_storage->IsPinSet())
    return false;

  // TODO(jdufault): Enable once quick unlock settings land(crbug.com/291747).
  return false;
}

// NotificationDelegate override:
std::string QuickUnlockNotificationController::id() const {
  return params_.delegate_id;
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
  DCHECK(!should_show_notification_callback_.is_null());
  if (should_show_notification_callback_.Run(profile_)) {
    SetNotificationPreferenceWasShown();
    return;
  }

  std::unique_ptr<Notification> notification = CreateNotification();
  g_browser_process->notification_ui_manager()->Add(*notification, profile_);
}

// message_center::NotificationDelegate override:
void QuickUnlockNotificationController::Close(bool by_user) {
  if (by_user)
    SetNotificationPreferenceWasShown();
}

// message_center::NotificationDelegate override:
void QuickUnlockNotificationController::Click() {
  chrome::NavigateParams params(profile_, params_.url,
                                ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);

  SetNotificationPreferenceWasShown();

  // Remove the notification from tray.
  g_browser_process->notification_ui_manager()->CancelById(
      id(), NotificationUIManager::GetProfileID(profile_));
}

void QuickUnlockNotificationController::SetNotificationPreferenceWasShown() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(params_.was_shown_pref_id, true);
}

void QuickUnlockNotificationController::UnregisterObserver() {
  if (registrar_.IsRegistered(this,
                              chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                              content::NotificationService::AllSources())) {
    registrar_.Remove(this, chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                      content::NotificationService::AllSources());
  }
}

std::unique_ptr<Notification>
QuickUnlockNotificationController::CreateNotification() {
  return base::MakeUnique<Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      l10n_util::GetStringUTF16(params_.title_message_id),
      l10n_util::GetStringUTF16(params_.body_message_id),
      // TODO(http://crbug.com/291747): Change this to actual icon for
      // quick unlock feature notification.
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(params_.icon_id),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 params_.notifier),
      l10n_util::GetStringUTF16(params_.feature_name_id), GURL(),
      params_.notification_id, message_center::RichNotificationData(), this);
}

QuickUnlockNotificationController::NotificationParams::NotificationParams() {}

QuickUnlockNotificationController::NotificationParams::~NotificationParams() {}

}  // namespace quick_unlock
}  // namespace chromeos
