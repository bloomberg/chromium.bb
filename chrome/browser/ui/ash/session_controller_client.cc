// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_controller_client.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/session_types.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/ash/multi_user/user_switch_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/equals_traits.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using session_manager::Session;
using session_manager::SessionManager;
using session_manager::SessionState;
using user_manager::UserManager;
using user_manager::User;
using user_manager::UserList;

namespace {

SessionControllerClient* g_instance = nullptr;

// Returns the session id of a given user or 0 if user has no session.
uint32_t GetSessionId(const User& user) {
  const AccountId& account_id = user.GetAccountId();
  for (auto& session : SessionManager::Get()->sessions()) {
    if (session.user_account_id == account_id)
      return session.id;
  }

  return 0u;
}

// Creates a mojom::UserSession for the given user. Returns nullptr if there is
// no user session started for the given user.
ash::mojom::UserSessionPtr UserToUserSession(const User& user) {
  const uint32_t user_session_id = GetSessionId(user);
  if (user_session_id == 0u)
    return nullptr;

  ash::mojom::UserSessionPtr session = ash::mojom::UserSession::New();
  session->session_id = user_session_id;
  session->type = user.GetType();
  session->account_id = user.GetAccountId();
  session->display_name = base::UTF16ToUTF8(user.display_name());
  session->display_email = user.display_email();

  session->avatar = user.GetImage();
  if (session->avatar.isNull()) {
    session->avatar = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_PROFILE_PICTURE_LOADING);
  }

  if (user.IsSupervised()) {
    Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(&user);
    if (profile) {
      SupervisedUserService* service =
          SupervisedUserServiceFactory::GetForProfile(profile);
      session->custodian_email = service->GetCustodianEmailAddress();
      session->second_custodian_email =
          service->GetSecondCustodianEmailAddress();
    }
  }

  chromeos::UserFlow* const user_flow =
      chromeos::ChromeUserManager::Get()->GetUserFlow(user.GetAccountId());
  session->should_enable_settings = user_flow->ShouldEnableSettings();
  session->should_show_notification_tray =
      user_flow->ShouldShowNotificationTray();

  return session;
}

void DoSwitchUser(const AccountId& account_id) {
  UserManager::Get()->SwitchActiveUser(account_id);
}

}  // namespace

namespace mojo {

// When comparing two mojom::UserSession objects we need to decide if the avatar
// images are changed. Consider them equal if they have the same storage rather
// than comparing the backing pixels.
template <>
struct EqualsTraits<gfx::ImageSkia> {
  static bool Equals(const gfx::ImageSkia& a, const gfx::ImageSkia& b) {
    return a.BackedBySameObjectAs(b);
  }
};

}  // namespace mojo

SessionControllerClient::SessionControllerClient()
    : binding_(this), weak_ptr_factory_(this) {
  SessionManager::Get()->AddObserver(this);
  UserManager::Get()->AddSessionStateObserver(this);
  UserManager::Get()->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());

  DCHECK(!g_instance);
  g_instance = this;
}

SessionControllerClient::~SessionControllerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;

  if (supervised_user_profile_) {
    SupervisedUserServiceFactory::GetForProfile(supervised_user_profile_)
        ->RemoveObserver(this);
  }

  SessionManager::Get()->RemoveObserver(this);
  UserManager::Get()->RemoveObserver(this);
  UserManager::Get()->RemoveSessionStateObserver(this);
}

void SessionControllerClient::Init() {
  ConnectToSessionController();
  session_controller_->SetClient(binding_.CreateInterfacePtrAndBind());
  SendSessionInfoIfChanged();
  // User sessions and their order will be sent via UserSessionStateObserver
  // even for crash-n-restart.
}

// static
SessionControllerClient* SessionControllerClient::Get() {
  return g_instance;
}

void SessionControllerClient::StartLock(StartLockCallback callback) {
  session_controller_->StartLock(callback);
}

void SessionControllerClient::NotifyChromeLockAnimationsComplete() {
  session_controller_->NotifyChromeLockAnimationsComplete();
}

void SessionControllerClient::RunUnlockAnimation(
    base::Closure animation_finished_callback) {
  session_controller_->RunUnlockAnimation(animation_finished_callback);
}

void SessionControllerClient::RequestLockScreen() {
  DoLockScreen();
}

void SessionControllerClient::SwitchActiveUser(const AccountId& account_id) {
  DoSwitchActiveUser(account_id);
}

void SessionControllerClient::CycleActiveUser(
    ash::CycleUserDirection direction) {
  DoCycleActiveUser(direction);
}

void SessionControllerClient::ActiveUserChanged(const User* active_user) {
  SendSessionInfoIfChanged();

  // UserAddedToSession is not called for the primary user session so its meta
  // data here needs to be sent to ash before setting user session order.
  // However, ActiveUserChanged happens at different timing for primary user
  // and secondary users. For primary user, it happens before user profile load.
  // For secondary users, it happens after user profile load. This caused
  // confusing down the path. Bail out here to defer the primary user session
  // metadata  sent until it becomes active so that ash side could expect a
  // consistent state.
  // TODO(xiyuan): Get rid of this after http://crbug.com/657149 refactoring.
  if (!primary_user_session_sent_ &&
      UserManager::Get()->GetPrimaryUser() == active_user) {
    return;
  }

  SendUserSessionOrder();
}

void SessionControllerClient::UserAddedToSession(const User* added_user) {
  SendSessionInfoIfChanged();
  SendUserSession(*added_user);
}

void SessionControllerClient::UserChangedChildStatus(User* user) {
  SendUserSession(*user);
}

void SessionControllerClient::OnUserImageChanged(
    const user_manager::User& user) {
  SendUserSession(user);
}

// static
bool SessionControllerClient::CanLockScreen() {
  return !UserManager::Get()->GetUnlockUsers().empty();
}

// static
bool SessionControllerClient::ShouldLockScreenAutomatically() {
  // TODO(xiyuan): Observe prefs::kEnableAutoScreenLock and update ash.
  // Tracked in http://crbug.com/670423
  const UserList logged_in_users = UserManager::Get()->GetLoggedInUsers();
  for (auto* user : logged_in_users) {
    Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
    if (profile &&
        profile->GetPrefs()->GetBoolean(prefs::kEnableAutoScreenLock)) {
      return true;
    }
  }
  return false;
}

// static
ash::AddUserSessionPolicy SessionControllerClient::GetAddUserSessionPolicy() {
  UserManager* const user_manager = UserManager::Get();
  if (user_manager->GetUsersAllowedForMultiProfile().empty())
    return ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS;

  if (chromeos::MultiProfileUserController::GetPrimaryUserPolicy() !=
      chromeos::MultiProfileUserController::ALLOWED) {
    return ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER;
  }

  if (UserManager::Get()->GetLoggedInUsers().size() >=
      session_manager::kMaxmiumNumberOfUserSessions)
    return ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED;

  return ash::AddUserSessionPolicy::ALLOWED;
}

// static
void SessionControllerClient::DoLockScreen() {
  if (!CanLockScreen())
    return;

  VLOG(1) << "Requesting screen lock from SessionControllerClient";
  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->RequestLockScreen();
}

// static
void SessionControllerClient::DoSwitchActiveUser(const AccountId& account_id) {
  // Disallow switching to an already active user since that might crash.
  if (account_id == UserManager::Get()->GetActiveUser()->GetAccountId())
    return;

  TrySwitchingActiveUser(base::Bind(&DoSwitchUser, account_id));
}

// static
void SessionControllerClient::DoCycleActiveUser(
    ash::CycleUserDirection direction) {
  const UserList& logged_in_users = UserManager::Get()->GetLoggedInUsers();
  if (logged_in_users.size() <= 1)
    return;

  AccountId account_id = UserManager::Get()->GetActiveUser()->GetAccountId();

  // Get an iterator positioned at the active user.
  auto it = std::find_if(logged_in_users.begin(), logged_in_users.end(),
                         [account_id](const User* user) {
                           return user->GetAccountId() == account_id;
                         });

  // Active user not found.
  if (it == logged_in_users.end())
    return;

  // Get the user's email to select, wrapping to the start/end of the list if
  // necessary.
  if (direction == ash::CycleUserDirection::NEXT) {
    if (++it == logged_in_users.end())
      account_id = (*logged_in_users.begin())->GetAccountId();
    else
      account_id = (*it)->GetAccountId();
  } else if (direction == ash::CycleUserDirection::PREVIOUS) {
    if (it == logged_in_users.begin())
      it = logged_in_users.end();
    account_id = (*(--it))->GetAccountId();
  } else {
    NOTREACHED() << "Invalid direction=" << static_cast<int>(direction);
    return;
  }

  DoSwitchActiveUser(account_id);
}

// static
void SessionControllerClient::FlushForTesting() {
  g_instance->session_controller_.FlushForTesting();
}

void SessionControllerClient::OnSessionStateChanged() {
  // Sent the primary user metadata and user session order that are deferred
  // from ActiveUserChanged before update session state.
  if (!primary_user_session_sent_ &&
      SessionManager::Get()->session_state() == SessionState::ACTIVE) {
    DCHECK_EQ(UserManager::Get()->GetPrimaryUser(),
              UserManager::Get()->GetActiveUser());
    primary_user_session_sent_ = true;
    SendUserSession(*UserManager::Get()->GetPrimaryUser());
    SendUserSessionOrder();
  }

  SendSessionInfoIfChanged();
}

void SessionControllerClient::OnCustodianInfoChanged() {
  DCHECK(supervised_user_profile_);
  User* user = chromeos::ProfileHelper::Get()->GetUserByProfile(
      supervised_user_profile_);
  if (user)
    SendUserSession(*user);
}

void SessionControllerClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_APP_TERMINATING:
      session_controller_->NotifyChromeTerminating();
      break;
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED: {
      Profile* profile = content::Details<Profile>(details).ptr();
      OnLoginUserProfilePrepared(profile);
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
      break;
  }
}

void SessionControllerClient::OnLoginUserProfilePrepared(Profile* profile) {
  const User* user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);

  if (profile->IsSupervised()) {
    // There can be only one supervised user per session.
    DCHECK(!supervised_user_profile_);
    supervised_user_profile_ = profile;

    // Watch for changes to supervised user manager/custodians.
    SupervisedUserServiceFactory::GetForProfile(supervised_user_profile_)
        ->AddObserver(this);
  }

  // Needed because the user-to-profile mapping isn't available until later,
  // which is needed in UserToUserSession().
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&SessionControllerClient::SendUserSessionForProfile,
                            weak_ptr_factory_.GetWeakPtr(), profile));
}

void SessionControllerClient::SendUserSessionForProfile(Profile* profile) {
  DCHECK(profile);
  const User* user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  SendUserSession(*user);
}

void SessionControllerClient::ConnectToSessionController() {
  // Tests may bind to their own SessionController.
  if (session_controller_)
    return;

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &session_controller_);
}

void SessionControllerClient::SendSessionInfoIfChanged() {
  SessionManager* const session_manager = SessionManager::Get();

  ash::mojom::SessionInfoPtr info = ash::mojom::SessionInfo::New();
  info->can_lock_screen = CanLockScreen();
  info->should_lock_screen_automatically = ShouldLockScreenAutomatically();
  info->add_user_session_policy = GetAddUserSessionPolicy();
  info->state = session_manager->session_state();

  if (info != last_sent_session_info_) {
    last_sent_session_info_ = info->Clone();
    session_controller_->SetSessionInfo(std::move(info));
  }
}

void SessionControllerClient::SendUserSession(const User& user) {
  ash::mojom::UserSessionPtr user_session = UserToUserSession(user);

  // Bail if the user has no session. Currently the only code path that hits
  // this condition is from OnUserImageChanged when user images are changed
  // on the login screen (e.g. policy change that adds a public session user,
  // or tests that create new users on the login screen).
  if (!user_session)
    return;

  if (user_session != last_sent_user_session_) {
    last_sent_user_session_ = user_session->Clone();
    session_controller_->UpdateUserSession(std::move(user_session));
  }
}

void SessionControllerClient::SendUserSessionOrder() {
  UserManager* const user_manager = UserManager::Get();

  const UserList logged_in_users = user_manager->GetLoggedInUsers();
  std::vector<uint32_t> user_session_ids;
  for (auto* user : user_manager->GetLRULoggedInUsers()) {
    const uint32_t user_session_id = GetSessionId(*user);
    DCHECK_NE(0u, user_session_id);
    user_session_ids.push_back(user_session_id);
  }

  session_controller_->SetUserSessionOrder(user_session_ids);
}
