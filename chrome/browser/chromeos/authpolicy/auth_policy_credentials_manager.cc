// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/authpolicy/auth_policy_credentials_manager.h"

#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification.h"
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
#include "dbus/message.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/message_center_switches.h"

namespace {

constexpr base::TimeDelta kGetUserStatusCallsInterval =
    base::TimeDelta::FromHours(1);
constexpr char kProfileSigninNotificationId[] = "chrome://settings/signin/";

// Prefix for KRB5CCNAME environment variable. Defines credential cache type.
constexpr char kKrb5CCFilePrefix[] = "FILE:";
// Directory in the user home to store Kerberos files.
constexpr char kKrb5Directory[] = "kerberos";
// Environment variable pointing to credential cache file.
constexpr char kKrb5CCEnvName[] = "KRB5CCNAME";
// Credential cache file name.
constexpr char kKrb5CCFile[] = "krb5cc";
// Environment variable pointing to Kerberos config file.
constexpr char kKrb5ConfEnvName[] = "KRB5_CONFIG";
// Kerberos config file name.
constexpr char kKrb5ConfFile[] = "krb5.conf";

// A notification delegate for the sign-out button.
// TODO(estade): Can this be a HandleNotificationButtonClickDelegate?
class SigninNotificationDelegate : public message_center::NotificationDelegate {
 public:
  SigninNotificationDelegate();

  // NotificationDelegate:
  void Click() override;
  void ButtonClick(int button_index) override;

 protected:
  ~SigninNotificationDelegate() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninNotificationDelegate);
};

SigninNotificationDelegate::SigninNotificationDelegate() {}

void SigninNotificationDelegate::Click() {
  chrome::AttemptUserExit();
}

void SigninNotificationDelegate::ButtonClick(int button_index) {
  chrome::AttemptUserExit();
}

// Writes |blob| into file <UserPath>/kerberos/|file_name|. First writes into
// temporary file and then replace existing one.
void WriteFile(const std::string& file_name, const std::string& blob) {
  base::FilePath dir;
  PathService::Get(base::DIR_HOME, &dir);
  dir = dir.Append(kKrb5Directory);
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(dir, &error)) {
    LOG(ERROR) << "Failed to create '" << dir.value()
               << "' directory: " << base::File::ErrorToString(error);
    return;
  }

  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(dir, &temp_file))
    return;

  if (base::WriteFile(temp_file, blob.data(), blob.size()) !=
      static_cast<int>(blob.size())) {
    LOG(ERROR) << "Failed to write file: " << temp_file.value();
    return;
  }

  base::FilePath dest_file = dir.Append(file_name);
  if (!base::ReplaceFile(temp_file, dest_file, &error)) {
    LOG(ERROR) << "Failed to replace '" << dest_file.value() << "' with '"
               << temp_file.value()
               << "' :" << base::File::ErrorToString(error);
  }
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
  GetUserKerberosFiles();

  // Setting environment variables for GSSAPI library.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath path;
  PathService::Get(base::DIR_HOME, &path);
  path = path.Append(kKrb5Directory);
  env->SetVar(kKrb5CCEnvName,
              kKrb5CCFilePrefix + path.Append(kKrb5CCFile).value());
  env->SetVar(kKrb5ConfEnvName, path.Append(kKrb5ConfFile).value());

  // Connecting to the signal sent by authpolicyd notifying that Kerberos files
  // have changed.
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->ConnectToSignal(
      authpolicy::kUserKerberosFilesChangedSignal,
      base::Bind(
          &AuthPolicyCredentialsManager::OnUserKerberosFilesChangedCallback,
          weak_factory_.GetWeakPtr()),
      base::Bind(&AuthPolicyCredentialsManager::OnSignalConnectedCallback,
                 weak_factory_.GetWeakPtr()));
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
  DCHECK(!is_get_status_in_progress_);
  is_get_status_in_progress_ = true;
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
  DCHECK(is_get_status_in_progress_);
  is_get_status_in_progress_ = false;
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

void AuthPolicyCredentialsManager::GetUserKerberosFiles() {
  chromeos::DBusThreadManager::Get()
      ->GetAuthPolicyClient()
      ->GetUserKerberosFiles(
          account_id_.GetObjGuid(),
          base::BindOnce(
              &AuthPolicyCredentialsManager::OnGetUserKerberosFilesCallback,
              weak_factory_.GetWeakPtr()));
}

void AuthPolicyCredentialsManager::OnGetUserKerberosFilesCallback(
    authpolicy::ErrorType error,
    const authpolicy::KerberosFiles& kerberos_files) {
  if (kerberos_files.has_krb5cc()) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::BindOnce(&WriteFile, kKrb5CCFile, kerberos_files.krb5cc()));
  }
  if (kerberos_files.has_krb5conf()) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::BindOnce(&WriteFile, kKrb5ConfFile, kerberos_files.krb5conf()));
  }
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
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      kProfileSigninNotificationId);

  // Set |profile_id| for multi-user notification blocker.
  notifier_id.profile_id = profile_->GetProfileUserName();

  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_BUBBLE_VIEW_TITLE),
      l10n_util::GetStringUTF16(message_id),
      message_center::IsNewStyleNotificationEnabled()
          ? gfx::Image()
          : ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                IDR_NOTIFICATION_ALERT),
      notifier_id, l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DISPLAY_SOURCE),
      GURL(notification_id), notification_id, data,
      new SigninNotificationDelegate());
  if (message_center::IsNewStyleNotificationEnabled()) {
    notification.set_accent_color(
        message_center::kSystemNotificationColorCriticalWarning);
    notification.set_small_image(gfx::Image(gfx::CreateVectorIcon(
        kNotificationWarningIcon, message_center::kSmallImageSizeMD,
        message_center::kSystemNotificationColorWarning)));
    notification.set_vector_small_image(kNotificationWarningIcon);
  }
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
  if (is_get_status_in_progress_) {
    rerun_get_status_on_error_ = true;
    return;
  }
  if (last_error_ != authpolicy::ERROR_NONE)
    GetUserStatus();
}

void AuthPolicyCredentialsManager::OnUserKerberosFilesChangedCallback(
    dbus::Signal* signal) {
  DCHECK_EQ(signal->GetInterface(), authpolicy::kAuthPolicyInterface);
  DCHECK_EQ(signal->GetMember(), authpolicy::kUserKerberosFilesChangedSignal);
  GetUserKerberosFiles();
}

void AuthPolicyCredentialsManager::OnSignalConnectedCallback(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  DCHECK_EQ(interface_name, authpolicy::kAuthPolicyInterface);
  DCHECK_EQ(signal_name, authpolicy::kUserKerberosFilesChangedSignal);
  DCHECK(success);
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
