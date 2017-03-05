// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_error_notifier_ash.h"

#include "ash/common/system/system_notifier.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/core/account_id/account_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "components/user_manager/user_manager.h"
#endif

namespace {

const char kProfileSyncNotificationId[] = "chrome://settings/sync/";

// A simple notification delegate for the sync setup button.
class SyncNotificationDelegate : public NotificationDelegate {
 public:
  SyncNotificationDelegate(const std::string& id,
                           Profile* profile);

  // NotificationDelegate:
  void Click() override;
  void ButtonClick(int button_index) override;
  std::string id() const override;

 protected:
  ~SyncNotificationDelegate() override;

 private:
  void ShowSyncSetup();

  // Unique id of the notification.
  const std::string id_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SyncNotificationDelegate);
};

SyncNotificationDelegate::SyncNotificationDelegate(
    const std::string& id,
    Profile* profile)
    : id_(id),
      profile_(profile) {
}

SyncNotificationDelegate::~SyncNotificationDelegate() {
}

void SyncNotificationDelegate::Click() {
  ShowSyncSetup();
}

void SyncNotificationDelegate::ButtonClick(int button_index) {
  ShowSyncSetup();
}

std::string SyncNotificationDelegate::id() const {
  return id_;
}

void SyncNotificationDelegate::ShowSyncSetup() {
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(profile_);
  if (login_ui->current_login_ui()) {
    // TODO(michaelpg): The LoginUI might be on an inactive desktop.
    // See crbug.com/354280.
    login_ui->current_login_ui()->FocusUI();
    return;
  }

  chrome::ShowSettingsSubPageForProfile(profile_, chrome::kSyncSetupSubPage);
}

}  // namespace

SyncErrorNotifier::SyncErrorNotifier(syncer::SyncErrorController* controller,
                                     Profile* profile)
    : error_controller_(controller), profile_(profile) {
  // Create a unique notification ID for this profile.
  notification_id_ =
      kProfileSyncNotificationId + profile_->GetProfileUserName();

  error_controller_->AddObserver(this);
  OnErrorChanged();
}

SyncErrorNotifier::~SyncErrorNotifier() {
  DCHECK(!error_controller_)
      << "SyncErrorNotifier::Shutdown() was not called";
}

void SyncErrorNotifier::Shutdown() {
  error_controller_->RemoveObserver(this);
  error_controller_ = nullptr;
}

void SyncErrorNotifier::OnErrorChanged() {
  NotificationUIManager* notification_ui_manager =
      g_browser_process->notification_ui_manager();

  // notification_ui_manager() may return null when shutting down.
  if (!notification_ui_manager)
    return;

  if (!error_controller_->HasError()) {
    g_browser_process->notification_ui_manager()->CancelById(
        notification_id_, NotificationUIManager::GetProfileID(profile_));
    return;
  }

#if defined(OS_CHROMEOS)
  if (user_manager::UserManager::IsInitialized()) {
    chromeos::UserFlow* user_flow =
        chromeos::ChromeUserManager::Get()->GetCurrentUserFlow();

    // Check whether Chrome OS user flow allows launching browser.
    // Example: Supervised user creation flow which handles token invalidation
    // itself and notifications should be suppressed. http://crbug.com/359045
    if (!user_flow->ShouldLaunchBrowser())
      return;
  }
#endif

  // Keep the existing notification if there is one.
  if (notification_ui_manager->FindById(
          notification_id_, NotificationUIManager::GetProfileID(profile_)))
    return;

  // Add an accept button to launch the sync setup settings subpage.
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_SYNC_NOTIFICATION_ACCEPT)));

  // Set the delegate for the notification's sync setup button.
  SyncNotificationDelegate* delegate =
      new SyncNotificationDelegate(notification_id_, profile_);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      kProfileSyncNotificationId);

  // Set |profile_id| for multi-user notification blocker.
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile_).GetUserEmail();

  // Add a new notification.
  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      l10n_util::GetStringUTF16(IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE),
      l10n_util::GetStringUTF16(IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_MESSAGE),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_ALERT),
      notifier_id,
      base::string16(),  // display_source
      GURL(notification_id_), notification_id_, data, delegate);
  notification_ui_manager->Add(notification, profile_);
}
