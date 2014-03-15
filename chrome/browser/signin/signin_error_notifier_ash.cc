// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_error_notifier_ash.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/system_notifier.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

namespace {

const char kProfileSigninNotificationId[] = "chrome://settings/signin/";

// A notification delegate for the sign-out button.
class SigninNotificationDelegate : public NotificationDelegate {
 public:
  SigninNotificationDelegate(const std::string& id,
                             Profile* profile);

  // NotificationDelegate:
  virtual void Display() OVERRIDE;
  virtual void Error() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual bool HasClickedListener() OVERRIDE;
  virtual void Click() OVERRIDE;
  virtual void ButtonClick(int button_index) OVERRIDE;
  virtual std::string id() const OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;

 protected:
  virtual ~SigninNotificationDelegate();

 private:
  // Unique id of the notification.
  const std::string id_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SigninNotificationDelegate);
};

SigninNotificationDelegate::SigninNotificationDelegate(
    const std::string& id,
    Profile* profile)
    : id_(id),
      profile_(profile) {
}

SigninNotificationDelegate::~SigninNotificationDelegate() {
}

void SigninNotificationDelegate::Display() {
}

void SigninNotificationDelegate::Error() {
}

void SigninNotificationDelegate::Close(bool by_user) {
}

bool SigninNotificationDelegate::HasClickedListener() {
  return false;
}

void SigninNotificationDelegate::Click() {
}

void SigninNotificationDelegate::ButtonClick(int button_index) {
#if defined(OS_CHROMEOS)
  chrome::AttemptUserExit();
#else
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(profile_);
  if (login_ui->current_login_ui()) {
    login_ui->current_login_ui()->FocusUI();
    return;
  }

  // Find a browser instance or create one.
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      profile_, chrome::HOST_DESKTOP_TYPE_ASH);

  // Navigate to the sync setup subpage, which will launch a login page.
  chrome::ShowSettingsSubPage(browser_displayer.browser(),
                              chrome::kSyncSetupSubPage);
#endif
}

std::string SigninNotificationDelegate::id() const {
  return id_;
}

content::RenderViewHost* SigninNotificationDelegate::GetRenderViewHost() const {
  return NULL;
}

}  // namespace

SigninErrorNotifier::SigninErrorNotifier(SigninErrorController* controller,
                                         Profile* profile)
    : error_controller_(controller),
      profile_(profile) {
  // Create a unique notification ID for this profile.
  notification_id_ = kProfileSigninNotificationId + profile->GetProfileName();

  error_controller_->AddObserver(this);
  OnErrorChanged();
}

SigninErrorNotifier::~SigninErrorNotifier() {
  DCHECK(!error_controller_)
      << "SigninErrorNotifier::Shutdown() was not called";
}

void SigninErrorNotifier::Shutdown() {
  error_controller_->RemoveObserver(this);
  error_controller_ = NULL;
}

void SigninErrorNotifier::OnErrorChanged() {
  NotificationUIManager* notification_ui_manager =
      g_browser_process->notification_ui_manager();

  // notification_ui_manager() may return NULL when shutting down.
  if (!notification_ui_manager)
    return;

  if (!error_controller_->HasError()) {
    g_browser_process->notification_ui_manager()->CancelById(notification_id_);
    return;
  }

  // Add an accept button to sign the user out.
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL)));

  // Set the delegate for the notification's sign-out button.
  SigninNotificationDelegate* delegate =
      new SigninNotificationDelegate(notification_id_, profile_);

  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GURL(notification_id_),
      GetMessageTitle(),
      GetMessageBody(),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_ALERT),
      blink::WebTextDirectionDefault,
      message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT,
          ash::system_notifier::kNotifierAuthError),
      base::string16(),  // display_source
      base::ASCIIToUTF16(notification_id_),
      data,
      delegate);

  // Update or add the notification.
  if (notification_ui_manager->FindById(notification_id_))
    notification_ui_manager->Update(notification, profile_);
  else
    notification_ui_manager->Add(notification, profile_);
}

base::string16 SigninErrorNotifier::GetMessageTitle() const {
  if (ash::Shell::HasInstance() &&
      ash::Shell::GetInstance()->delegate()->IsMultiProfilesEnabled()) {
    // Include the account id in the message text to differentiate between
    // profiles.
    return l10n_util::GetStringFUTF16(
        IDS_SIGNIN_ERROR_NOTIFICATION_TITLE_MULTIPROFILE,
        base::ASCIIToUTF16(error_controller_->error_account_id()));
  }

  return l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_BUBBLE_VIEW_TITLE);
}

base::string16 SigninErrorNotifier::GetMessageBody() const {
  switch (error_controller_->auth_error().state()) {
    // TODO(rogerta): use account id in error messages.

    // User credentials are invalid (bad acct, etc).
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE);
      break;

    // Sync service is not available for this account's domain.
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_MESSAGE);
      break;

    // Generic message for "other" errors.
    default:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_OTHER_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE);
  }
}
