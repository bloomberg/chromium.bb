// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/authpolicy/auth_policy_credentials_manager.h"

#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

constexpr base::TimeDelta kGetUserStatusCallsInterval =
    base::TimeDelta::FromHours(1);
const char kProfileSigninNotificationId[] = "chrome://settings/signin/";

// A notification delegate for the sign-out button.
class SigninNotificationDelegate : public NotificationDelegate {
 public:
  explicit SigninNotificationDelegate(const std::string& id);

  // NotificationDelegate:
  void Click() override;
  void ButtonClick(int button_index) override;
  std::string id() const override;

 protected:
  ~SigninNotificationDelegate() override = default;

 private:
  // Unique id of the notification.
  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(SigninNotificationDelegate);
};

SigninNotificationDelegate::SigninNotificationDelegate(const std::string& id)
    : id_(id) {}

void SigninNotificationDelegate::Click() {
  chrome::AttemptUserExit();
}

void SigninNotificationDelegate::ButtonClick(int button_index) {
  chrome::AttemptUserExit();
}

std::string SigninNotificationDelegate::id() const {
  return id_;
}

}  // namespace

namespace chromeos {

AuthPolicyCredentialsManager::AuthPolicyCredentialsManager(Profile* profile)
    : profile_(profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  CHECK(user && user->IsActiveDirectoryUser());
  StartObserveNetwork();
  account_id_ = user->GetAccountId();
  GetUserStatus();
}

AuthPolicyCredentialsManager::~AuthPolicyCredentialsManager() {}

void AuthPolicyCredentialsManager::Shutdown() {
  StopObserveNetwork();
}

void AuthPolicyCredentialsManager::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  GetUserStatusIfConnected(network);
}

void AuthPolicyCredentialsManager::NetworkConnectionStateChanged(
    const chromeos::NetworkState* network) {
  GetUserStatusIfConnected(network);
}

void AuthPolicyCredentialsManager::OnShuttingDown() {
  StopObserveNetwork();
}

void AuthPolicyCredentialsManager::GetUserStatus() {
  DCHECK(!weak_factory_.HasWeakPtrs());
  rerun_get_status_on_error_ = false;
  scheduled_get_user_status_call_.Cancel();
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->GetUserStatus(
      account_id_.GetObjGuid(),
      base::BindOnce(&AuthPolicyCredentialsManager::OnGetUserStatusCallback,
                     weak_factory_.GetWeakPtr()));
}

void AuthPolicyCredentialsManager::OnGetUserStatusCallback(
    authpolicy::ErrorType error,
    const authpolicy::ActiveDirectoryUserStatus& user_status) {
  DCHECK(weak_factory_.HasWeakPtrs());
  ScheduleGetUserStatus();
  last_error_ = error;
  if (error != authpolicy::ERROR_NONE) {
    DLOG(ERROR) << "GetUserStatus failed with " << error;
    if (rerun_get_status_on_error_) {
      rerun_get_status_on_error_ = false;
      GetUserStatus();
    }
    return;
  }
  CHECK(user_status.account_info().account_id() == account_id_.GetObjGuid());
  rerun_get_status_on_error_ = false;
  if (user_status.has_account_info())
    UpdateDisplayAndGivenName(user_status.account_info());

  DCHECK(user_status.has_password_status());
  switch (user_status.password_status()) {
    case authpolicy::ActiveDirectoryUserStatus::PASSWORD_VALID:
      // do nothing
      break;
    case authpolicy::ActiveDirectoryUserStatus::PASSWORD_EXPIRED:
      ShowNotification(IDS_ACTIVE_DIRECTORY_PASSWORD_EXPIRED);
      break;
    case authpolicy::ActiveDirectoryUserStatus::PASSWORD_CHANGED:
      ShowNotification(IDS_ACTIVE_DIRECTORY_PASSWORD_CHANGED);
      break;
  }

  DCHECK(user_status.has_tgt_status());
  switch (user_status.tgt_status()) {
    case authpolicy::ActiveDirectoryUserStatus::TGT_VALID:
      // do nothing
      break;
    case authpolicy::ActiveDirectoryUserStatus::TGT_EXPIRED:
    case authpolicy::ActiveDirectoryUserStatus::TGT_NOT_FOUND:
      ShowNotification(IDS_ACTIVE_DIRECTORY_REFRESH_AUTH_TOKEN);
      break;
  }
  const bool ok = user_status.tgt_status() ==
                      authpolicy::ActiveDirectoryUserStatus::TGT_VALID &&
                  user_status.password_status() ==
                      authpolicy::ActiveDirectoryUserStatus::PASSWORD_VALID;
  user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id_, !ok);
}

void AuthPolicyCredentialsManager::ScheduleGetUserStatus() {
  // Unretained is safe here because it is a CancelableClosure and owned by this
  // object.
  scheduled_get_user_status_call_.Reset(base::Bind(
      &AuthPolicyCredentialsManager::GetUserStatus, base::Unretained(this)));
  // TODO(rsorokin): This does not re-schedule after wake from sleep
  // (and thus the maximal interval between two calls can be (sleep time +
  // kGetUserStatusCallsInterval)) (see crbug.com/726672).
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, scheduled_get_user_status_call_.callback(),
      kGetUserStatusCallsInterval);
}

void AuthPolicyCredentialsManager::StartObserveNetwork() {
  DCHECK(chromeos::NetworkHandler::IsInitialized());
  if (is_observing_network_)
    return;
  is_observing_network_ = true;
  chromeos::NetworkHandler::Get()->network_state_handler()->AddObserver(
      this, FROM_HERE);
}

void AuthPolicyCredentialsManager::StopObserveNetwork() {
  if (!is_observing_network_)
    return;
  DCHECK(chromeos::NetworkHandler::IsInitialized());
  is_observing_network_ = false;
  chromeos::NetworkHandler::Get()->network_state_handler()->RemoveObserver(
      this, FROM_HERE);
}

void AuthPolicyCredentialsManager::UpdateDisplayAndGivenName(
    const authpolicy::ActiveDirectoryAccountInfo& account_info) {
  if (display_name_ == account_info.display_name() &&
      given_name_ == account_info.given_name()) {
    return;
  }
  display_name_ = account_info.display_name();
  given_name_ = account_info.given_name();
  user_manager::UserManager::Get()->UpdateUserAccountData(
      account_id_,
      user_manager::UserManager::UserAccountData(
          base::UTF8ToUTF16(display_name_), base::UTF8ToUTF16(given_name_),
          std::string() /* locale */));
}

void AuthPolicyCredentialsManager::ShowNotification(int message_id) {
  if (shown_notifications_.count(message_id) > 0)
    return;

  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL)));

  const std::string notification_id = kProfileSigninNotificationId +
                                      profile_->GetProfileUserName() +
                                      std::to_string(message_id);
  // Set the delegate for the notification's sign-out button.
  SigninNotificationDelegate* delegate =
      new SigninNotificationDelegate(notification_id);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      kProfileSigninNotificationId);

  // Set |profile_id| for multi-user notification blocker.
  notifier_id.profile_id = profile_->GetProfileUserName();

  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_BUBBLE_VIEW_TITLE),
      l10n_util::GetStringUTF16(message_id),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_ALERT),
      notifier_id,
      base::string16(),  // display_source
      GURL(notification_id), notification_id, data, delegate);
  notification.SetSystemPriority();

  NotificationUIManager* notification_ui_manager =
      g_browser_process->notification_ui_manager();
  // Add the notification.
  notification_ui_manager->Add(notification, profile_);
  shown_notifications_.insert(message_id);
}

void AuthPolicyCredentialsManager::GetUserStatusIfConnected(
    const chromeos::NetworkState* network) {
  if (!network || !network->IsConnectedState())
    return;
  if (weak_factory_.HasWeakPtrs()) {
    // Another call is in progress.
    rerun_get_status_on_error_ = true;
    return;
  }
  if (last_error_ != authpolicy::ERROR_NONE)
    GetUserStatus();
}

// static
AuthPolicyCredentialsManagerFactory*
AuthPolicyCredentialsManagerFactory::GetInstance() {
  return base::Singleton<AuthPolicyCredentialsManagerFactory>::get();
}

// static
KeyedService*
AuthPolicyCredentialsManagerFactory::BuildForProfileIfActiveDirectory(
    Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || !user->IsActiveDirectoryUser())
    return nullptr;
  return GetInstance()->GetServiceForBrowserContext(profile, true /* create */);
}

AuthPolicyCredentialsManagerFactory::AuthPolicyCredentialsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "AuthPolicyCredentialsManager",
          BrowserContextDependencyManager::GetInstance()) {}

AuthPolicyCredentialsManagerFactory::~AuthPolicyCredentialsManagerFactory() {}

KeyedService* AuthPolicyCredentialsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new AuthPolicyCredentialsManager(profile);
}

}  // namespace chromeos
