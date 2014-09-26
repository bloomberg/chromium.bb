// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager_base.h"

#include <cstddef>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/login/login_state.h"
#include "chromeos/login/user_names.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/remove_user_delegate.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_manager {
namespace {

// A vector pref of the the regular users known on this device, arranged in LRU
// order.
const char kRegularUsers[] = "LoggedInUsers";

// A dictionary that maps user IDs to the displayed name.
const char kUserDisplayName[] = "UserDisplayName";

// A dictionary that maps user IDs to the user's given name.
const char kUserGivenName[] = "UserGivenName";

// A dictionary that maps user IDs to the displayed (non-canonical) emails.
const char kUserDisplayEmail[] = "UserDisplayEmail";

// A dictionary that maps user IDs to OAuth token presence flag.
const char kUserOAuthTokenStatus[] = "OAuthTokenStatus";

// A dictionary that maps user IDs to a flag indicating whether online
// authentication against GAIA should be enforced during the next sign-in.
const char kUserForceOnlineSignin[] = "UserForceOnlineSignin";

// A string pref containing the ID of the last user who logged in if it was
// a regular user or an empty string if it was another type of user (guest,
// kiosk, public account, etc.).
const char kLastLoggedInRegularUser[] = "LastLoggedInRegularUser";

// A string pref containing the ID of the last active user.
// In case of browser crash, this pref will be used to set active user after
// session restore.
const char kLastActiveUser[] = "LastActiveUser";

// Upper bound for a histogram metric reporting the amount of time between
// one regular user logging out and a different regular user logging in.
const int kLogoutToLoginDelayMaxSec = 1800;

// Callback that is called after user removal is complete.
void OnRemoveUserComplete(const std::string& user_email,
                          bool success,
                          cryptohome::MountError return_code) {
  // Log the error, but there's not much we can do.
  if (!success) {
    LOG(ERROR) << "Removal of cryptohome for " << user_email
               << " failed, return code: " << return_code;
  }
}

// Runs on SequencedWorkerPool thread. Passes resolved locale to UI thread.
void ResolveLocale(const std::string& raw_locale,
                   std::string* resolved_locale) {
  ignore_result(l10n_util::CheckAndResolveLocale(raw_locale, resolved_locale));
}

}  // namespace

// static
void UserManagerBase::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kRegularUsers);
  registry->RegisterStringPref(kLastLoggedInRegularUser, std::string());
  registry->RegisterDictionaryPref(kUserDisplayName);
  registry->RegisterDictionaryPref(kUserGivenName);
  registry->RegisterDictionaryPref(kUserDisplayEmail);
  registry->RegisterDictionaryPref(kUserOAuthTokenStatus);
  registry->RegisterDictionaryPref(kUserForceOnlineSignin);
  registry->RegisterStringPref(kLastActiveUser, std::string());
}

UserManagerBase::UserManagerBase(
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<base::TaskRunner> blocking_task_runner)
    : active_user_(NULL),
      primary_user_(NULL),
      user_loading_stage_(STAGE_NOT_LOADED),
      session_started_(false),
      is_current_user_owner_(false),
      is_current_user_new_(false),
      is_current_user_ephemeral_regular_user_(false),
      ephemeral_users_enabled_(false),
      manager_creation_time_(base::TimeTicks::Now()),
      last_session_active_user_initialized_(false),
      task_runner_(task_runner),
      blocking_task_runner_(blocking_task_runner),
      weak_factory_(this) {
  UpdateLoginState();
}

UserManagerBase::~UserManagerBase() {
  // Can't use STLDeleteElements because of the private destructor of User.
  for (UserList::iterator it = users_.begin(); it != users_.end();
       it = users_.erase(it)) {
    DeleteUser(*it);
  }
  // These are pointers to the same User instances that were in users_ list.
  logged_in_users_.clear();
  lru_logged_in_users_.clear();

  DeleteUser(active_user_);
}

void UserManagerBase::Shutdown() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
}

const UserList& UserManagerBase::GetUsers() const {
  const_cast<UserManagerBase*>(this)->EnsureUsersLoaded();
  return users_;
}

const UserList& UserManagerBase::GetLoggedInUsers() const {
  return logged_in_users_;
}

const UserList& UserManagerBase::GetLRULoggedInUsers() const {
  return lru_logged_in_users_;
}

const std::string& UserManagerBase::GetOwnerEmail() const {
  return owner_email_;
}

void UserManagerBase::UserLoggedIn(const std::string& user_id,
                                   const std::string& username_hash,
                                   bool browser_restart) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!last_session_active_user_initialized_) {
    last_session_active_user_ = GetLocalState()->GetString(kLastActiveUser);
    last_session_active_user_initialized_ = true;
  }

  User* user = FindUserInListAndModify(user_id);
  if (active_user_ && user) {
    user->set_is_logged_in(true);
    user->set_username_hash(username_hash);
    logged_in_users_.push_back(user);
    lru_logged_in_users_.push_back(user);

    // Reset the new user flag if the user already exists.
    SetIsCurrentUserNew(false);
    NotifyUserAddedToSession(user, true /* user switch pending */);

    return;
  }

  if (user_id == chromeos::login::kGuestUserName) {
    GuestUserLoggedIn();
  } else if (user_id == chromeos::login::kRetailModeUserName) {
    RetailModeUserLoggedIn();
  } else if (IsKioskApp(user_id)) {
    KioskAppLoggedIn(user_id);
  } else if (IsDemoApp(user_id)) {
    DemoAccountLoggedIn();
  } else {
    EnsureUsersLoaded();

    if (user && user->GetType() == USER_TYPE_PUBLIC_ACCOUNT) {
      PublicAccountUserLoggedIn(user);
    } else if ((user && user->GetType() == USER_TYPE_SUPERVISED) ||
               (!user &&
                gaia::ExtractDomainName(user_id) ==
                    chromeos::login::kSupervisedUserDomain)) {
      SupervisedUserLoggedIn(user_id);
    } else if (browser_restart && IsPublicAccountMarkedForRemoval(user_id)) {
      PublicAccountUserLoggedIn(User::CreatePublicAccountUser(user_id));
    } else if (user_id != GetOwnerEmail() && !user &&
               (AreEphemeralUsersEnabled() || browser_restart)) {
      RegularUserLoggedInAsEphemeral(user_id);
    } else {
      RegularUserLoggedIn(user_id);
    }
  }

  DCHECK(active_user_);
  active_user_->set_is_logged_in(true);
  active_user_->set_is_active(true);
  active_user_->set_username_hash(username_hash);

  // Place user who just signed in to the top of the logged in users.
  logged_in_users_.insert(logged_in_users_.begin(), active_user_);
  SetLRUUser(active_user_);

  if (!primary_user_) {
    primary_user_ = active_user_;
    if (primary_user_->GetType() == USER_TYPE_REGULAR)
      SendRegularUserLoginMetrics(user_id);
  }

  UMA_HISTOGRAM_ENUMERATION(
      "UserManager.LoginUserType", active_user_->GetType(), NUM_USER_TYPES);

  GetLocalState()->SetString(
      kLastLoggedInRegularUser,
      (active_user_->GetType() == USER_TYPE_REGULAR) ? user_id : "");

  NotifyOnLogin();
  PerformPostUserLoggedInActions(browser_restart);
}

void UserManagerBase::SwitchActiveUser(const std::string& user_id) {
  User* user = FindUserAndModify(user_id);
  if (!user) {
    NOTREACHED() << "Switching to a non-existing user";
    return;
  }
  if (user == active_user_) {
    NOTREACHED() << "Switching to a user who is already active";
    return;
  }
  if (!user->is_logged_in()) {
    NOTREACHED() << "Switching to a user that is not logged in";
    return;
  }
  if (user->GetType() != USER_TYPE_REGULAR) {
    NOTREACHED() << "Switching to a non-regular user";
    return;
  }
  if (user->username_hash().empty()) {
    NOTREACHED() << "Switching to a user that doesn't have username_hash set";
    return;
  }

  DCHECK(active_user_);
  active_user_->set_is_active(false);
  user->set_is_active(true);
  active_user_ = user;

  // Move the user to the front.
  SetLRUUser(active_user_);

  NotifyActiveUserHashChanged(active_user_->username_hash());
  NotifyActiveUserChanged(active_user_);
}

void UserManagerBase::SwitchToLastActiveUser() {
  if (last_session_active_user_.empty())
    return;

  if (GetActiveUser()->email() != last_session_active_user_)
    SwitchActiveUser(last_session_active_user_);

  // Make sure that this function gets run only once.
  last_session_active_user_.clear();
}

void UserManagerBase::SessionStarted() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  session_started_ = true;

  UpdateLoginState();
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SESSION_STATE_ACTIVE);

  if (IsCurrentUserNew()) {
    // Make sure that the new user's data is persisted to Local State.
    GetLocalState()->CommitPendingWrite();
  }
}

void UserManagerBase::RemoveUser(const std::string& user_id,
                                 RemoveUserDelegate* delegate) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!CanUserBeRemoved(FindUser(user_id)))
    return;

  RemoveUserInternal(user_id, delegate);
}

void UserManagerBase::RemoveUserInternal(const std::string& user_email,
                                         RemoveUserDelegate* delegate) {
  RemoveNonOwnerUserInternal(user_email, delegate);
}

void UserManagerBase::RemoveNonOwnerUserInternal(const std::string& user_email,
                                                 RemoveUserDelegate* delegate) {
  if (delegate)
    delegate->OnBeforeUserRemoved(user_email);
  RemoveUserFromList(user_email);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      user_email, base::Bind(&OnRemoveUserComplete, user_email));

  if (delegate)
    delegate->OnUserRemoved(user_email);
}

void UserManagerBase::RemoveUserFromList(const std::string& user_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  RemoveNonCryptohomeData(user_id);
  if (user_loading_stage_ == STAGE_LOADED) {
    DeleteUser(RemoveRegularOrSupervisedUserFromList(user_id));
  } else if (user_loading_stage_ == STAGE_LOADING) {
    DCHECK(gaia::ExtractDomainName(user_id) ==
           chromeos::login::kSupervisedUserDomain);
    // Special case, removing partially-constructed supervised user during user
    // list loading.
    ListPrefUpdate users_update(GetLocalState(), kRegularUsers);
    users_update->Remove(base::StringValue(user_id), NULL);
  } else {
    NOTREACHED() << "Users are not loaded yet.";
    return;
  }

  // Make sure that new data is persisted to Local State.
  GetLocalState()->CommitPendingWrite();
}

bool UserManagerBase::IsKnownUser(const std::string& user_id) const {
  return FindUser(user_id) != NULL;
}

const User* UserManagerBase::FindUser(const std::string& user_id) const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (active_user_ && active_user_->email() == user_id)
    return active_user_;
  return FindUserInList(user_id);
}

User* UserManagerBase::FindUserAndModify(const std::string& user_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (active_user_ && active_user_->email() == user_id)
    return active_user_;
  return FindUserInListAndModify(user_id);
}

const User* UserManagerBase::GetLoggedInUser() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return active_user_;
}

User* UserManagerBase::GetLoggedInUser() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return active_user_;
}

const User* UserManagerBase::GetActiveUser() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return active_user_;
}

User* UserManagerBase::GetActiveUser() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return active_user_;
}

const User* UserManagerBase::GetPrimaryUser() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return primary_user_;
}

void UserManagerBase::SaveUserOAuthStatus(
    const std::string& user_id,
    User::OAuthTokenStatus oauth_token_status) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  DVLOG(1) << "Saving user OAuth token status in Local State";
  User* user = FindUserAndModify(user_id);
  if (user)
    user->set_oauth_token_status(oauth_token_status);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(user_id))
    return;

  DictionaryPrefUpdate oauth_status_update(GetLocalState(),
                                           kUserOAuthTokenStatus);
  oauth_status_update->SetWithoutPathExpansion(
      user_id,
      new base::FundamentalValue(static_cast<int>(oauth_token_status)));
}

void UserManagerBase::SaveForceOnlineSignin(const std::string& user_id,
                                            bool force_online_signin) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(user_id))
    return;

  DictionaryPrefUpdate force_online_update(GetLocalState(),
                                           kUserForceOnlineSignin);
  force_online_update->SetBooleanWithoutPathExpansion(user_id,
                                                      force_online_signin);
}

void UserManagerBase::SaveUserDisplayName(const std::string& user_id,
                                          const base::string16& display_name) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (User* user = FindUserAndModify(user_id)) {
    user->set_display_name(display_name);

    // Do not update local state if data stored or cached outside the user's
    // cryptohome is to be treated as ephemeral.
    if (!IsUserNonCryptohomeDataEphemeral(user_id)) {
      DictionaryPrefUpdate display_name_update(GetLocalState(),
                                               kUserDisplayName);
      display_name_update->SetWithoutPathExpansion(
          user_id, new base::StringValue(display_name));
    }
  }
}

base::string16 UserManagerBase::GetUserDisplayName(
    const std::string& user_id) const {
  const User* user = FindUser(user_id);
  return user ? user->display_name() : base::string16();
}

void UserManagerBase::SaveUserDisplayEmail(const std::string& user_id,
                                           const std::string& display_email) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  User* user = FindUserAndModify(user_id);
  if (!user) {
    LOG(ERROR) << "User not found: " << user_id;
    return;  // Ignore if there is no such user.
  }

  user->set_display_email(display_email);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(user_id))
    return;

  DictionaryPrefUpdate display_email_update(GetLocalState(), kUserDisplayEmail);
  display_email_update->SetWithoutPathExpansion(
      user_id, new base::StringValue(display_email));
}

std::string UserManagerBase::GetUserDisplayEmail(
    const std::string& user_id) const {
  const User* user = FindUser(user_id);
  return user ? user->display_email() : user_id;
}

void UserManagerBase::UpdateUserAccountData(
    const std::string& user_id,
    const UserAccountData& account_data) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  SaveUserDisplayName(user_id, account_data.display_name());

  if (User* user = FindUserAndModify(user_id)) {
    base::string16 given_name = account_data.given_name();
    user->set_given_name(given_name);
    if (!IsUserNonCryptohomeDataEphemeral(user_id)) {
      DictionaryPrefUpdate given_name_update(GetLocalState(), kUserGivenName);
      given_name_update->SetWithoutPathExpansion(
          user_id, new base::StringValue(given_name));
    }
  }

  UpdateUserAccountLocale(user_id, account_data.locale());
}

// static
void UserManagerBase::ParseUserList(const base::ListValue& users_list,
                                    const std::set<std::string>& existing_users,
                                    std::vector<std::string>* users_vector,
                                    std::set<std::string>* users_set) {
  users_vector->clear();
  users_set->clear();
  for (size_t i = 0; i < users_list.GetSize(); ++i) {
    std::string email;
    if (!users_list.GetString(i, &email) || email.empty()) {
      LOG(ERROR) << "Corrupt entry in user list at index " << i << ".";
      continue;
    }
    if (existing_users.find(email) != existing_users.end() ||
        !users_set->insert(email).second) {
      LOG(ERROR) << "Duplicate user: " << email;
      continue;
    }
    users_vector->push_back(email);
  }
}

bool UserManagerBase::IsCurrentUserOwner() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::AutoLock lk(is_current_user_owner_lock_);
  return is_current_user_owner_;
}

void UserManagerBase::SetCurrentUserIsOwner(bool is_current_user_owner) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  {
    base::AutoLock lk(is_current_user_owner_lock_);
    is_current_user_owner_ = is_current_user_owner;
  }
  UpdateLoginState();
}

bool UserManagerBase::IsCurrentUserNew() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return is_current_user_new_;
}

bool UserManagerBase::IsCurrentUserNonCryptohomeDataEphemeral() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() &&
         IsUserNonCryptohomeDataEphemeral(GetLoggedInUser()->email());
}

bool UserManagerBase::CanCurrentUserLock() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() && active_user_->can_lock();
}

bool UserManagerBase::IsUserLoggedIn() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return active_user_;
}

bool UserManagerBase::IsLoggedInAsRegularUser() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_REGULAR;
}

bool UserManagerBase::IsLoggedInAsDemoUser() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_RETAIL_MODE;
}

bool UserManagerBase::IsLoggedInAsPublicAccount() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() &&
         active_user_->GetType() == USER_TYPE_PUBLIC_ACCOUNT;
}

bool UserManagerBase::IsLoggedInAsGuest() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_GUEST;
}

bool UserManagerBase::IsLoggedInAsSupervisedUser() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_SUPERVISED;
}

bool UserManagerBase::IsLoggedInAsKioskApp() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_KIOSK_APP;
}

bool UserManagerBase::IsLoggedInAsStub() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() &&
         active_user_->email() == chromeos::login::kStubUser;
}

bool UserManagerBase::IsSessionStarted() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return session_started_;
}

bool UserManagerBase::IsUserNonCryptohomeDataEphemeral(
    const std::string& user_id) const {
  // Data belonging to the guest, retail mode and stub users is always
  // ephemeral.
  if (user_id == chromeos::login::kGuestUserName ||
      user_id == chromeos::login::kRetailModeUserName ||
      user_id == chromeos::login::kStubUser) {
    return true;
  }

  // Data belonging to the owner, anyone found on the user list and obsolete
  // public accounts whose data has not been removed yet is not ephemeral.
  if (user_id == GetOwnerEmail() || UserExistsInList(user_id) ||
      IsPublicAccountMarkedForRemoval(user_id)) {
    return false;
  }

  // Data belonging to the currently logged-in user is ephemeral when:
  // a) The user logged into a regular account while the ephemeral users policy
  //    was enabled.
  //    - or -
  // b) The user logged into any other account type.
  if (IsUserLoggedIn() && (user_id == GetLoggedInUser()->email()) &&
      (is_current_user_ephemeral_regular_user_ || !IsLoggedInAsRegularUser())) {
    return true;
  }

  // Data belonging to any other user is ephemeral when:
  // a) Going through the regular login flow and the ephemeral users policy is
  //    enabled.
  //    - or -
  // b) The browser is restarting after a crash.
  return AreEphemeralUsersEnabled() ||
         session_manager::SessionManager::HasBrowserRestarted();
}

void UserManagerBase::AddObserver(UserManager::Observer* obs) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  observer_list_.AddObserver(obs);
}

void UserManagerBase::RemoveObserver(UserManager::Observer* obs) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  observer_list_.RemoveObserver(obs);
}

void UserManagerBase::AddSessionStateObserver(
    UserManager::UserSessionStateObserver* obs) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  session_state_observer_list_.AddObserver(obs);
}

void UserManagerBase::RemoveSessionStateObserver(
    UserManager::UserSessionStateObserver* obs) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  session_state_observer_list_.RemoveObserver(obs);
}

void UserManagerBase::NotifyLocalStateChanged() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(
      UserManager::Observer, observer_list_, LocalStateChanged(this));
}

void UserManagerBase::ForceUpdateState() {
  UpdateLoginState();
}

bool UserManagerBase::CanUserBeRemoved(const User* user) const {
  // Only regular and supervised users are allowed to be manually removed.
  if (!user || (user->GetType() != USER_TYPE_REGULAR &&
                user->GetType() != USER_TYPE_SUPERVISED)) {
    return false;
  }

  // Sanity check: we must not remove single user unless it's an enterprise
  // device. This check may seem redundant at a first sight because
  // this single user must be an owner and we perform special check later
  // in order not to remove an owner. However due to non-instant nature of
  // ownership assignment this later check may sometimes fail.
  // See http://crosbug.com/12723
  if (users_.size() < 2 && !IsEnterpriseManaged())
    return false;

  // Sanity check: do not allow any of the the logged in users to be removed.
  for (UserList::const_iterator it = logged_in_users_.begin();
       it != logged_in_users_.end();
       ++it) {
    if ((*it)->email() == user->email())
      return false;
  }

  return true;
}

bool UserManagerBase::GetEphemeralUsersEnabled() const {
  return ephemeral_users_enabled_;
}

void UserManagerBase::SetEphemeralUsersEnabled(bool enabled) {
  ephemeral_users_enabled_ = enabled;
}

void UserManagerBase::SetIsCurrentUserNew(bool is_new) {
  is_current_user_new_ = is_new;
}

void UserManagerBase::SetOwnerEmail(std::string owner_user_id) {
  owner_email_ = owner_user_id;
}

const std::string& UserManagerBase::GetPendingUserSwitchID() const {
  return pending_user_switch_;
}

void UserManagerBase::SetPendingUserSwitchID(std::string user_id) {
  pending_user_switch_ = user_id;
}

void UserManagerBase::EnsureUsersLoaded() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (!GetLocalState())
    return;

  if (user_loading_stage_ != STAGE_NOT_LOADED)
    return;
  user_loading_stage_ = STAGE_LOADING;

  PerformPreUserListLoadingActions();

  PrefService* local_state = GetLocalState();
  const base::ListValue* prefs_regular_users =
      local_state->GetList(kRegularUsers);

  const base::DictionaryValue* prefs_display_names =
      local_state->GetDictionary(kUserDisplayName);
  const base::DictionaryValue* prefs_given_names =
      local_state->GetDictionary(kUserGivenName);
  const base::DictionaryValue* prefs_display_emails =
      local_state->GetDictionary(kUserDisplayEmail);

  // Load public sessions first.
  std::set<std::string> public_sessions_set;
  LoadPublicAccounts(&public_sessions_set);

  // Load regular users and supervised users.
  std::vector<std::string> regular_users;
  std::set<std::string> regular_users_set;
  ParseUserList(*prefs_regular_users,
                public_sessions_set,
                &regular_users,
                &regular_users_set);
  for (std::vector<std::string>::const_iterator it = regular_users.begin();
       it != regular_users.end();
       ++it) {
    User* user = NULL;
    const std::string domain = gaia::ExtractDomainName(*it);
    if (domain == chromeos::login::kSupervisedUserDomain)
      user = User::CreateSupervisedUser(*it);
    else
      user = User::CreateRegularUser(*it);
    user->set_oauth_token_status(LoadUserOAuthStatus(*it));
    user->set_force_online_signin(LoadForceOnlineSignin(*it));
    users_.push_back(user);

    base::string16 display_name;
    if (prefs_display_names->GetStringWithoutPathExpansion(*it,
                                                           &display_name)) {
      user->set_display_name(display_name);
    }

    base::string16 given_name;
    if (prefs_given_names->GetStringWithoutPathExpansion(*it, &given_name)) {
      user->set_given_name(given_name);
    }

    std::string display_email;
    if (prefs_display_emails->GetStringWithoutPathExpansion(*it,
                                                            &display_email)) {
      user->set_display_email(display_email);
    }
  }

  user_loading_stage_ = STAGE_LOADED;

  PerformPostUserListLoadingActions();
}

UserList& UserManagerBase::GetUsersAndModify() {
  EnsureUsersLoaded();
  return users_;
}

const User* UserManagerBase::FindUserInList(const std::string& user_id) const {
  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->email() == user_id)
      return *it;
  }
  return NULL;
}

const bool UserManagerBase::UserExistsInList(const std::string& user_id) const {
  const base::ListValue* user_list = GetLocalState()->GetList(kRegularUsers);
  for (size_t i = 0; i < user_list->GetSize(); ++i) {
    std::string email;
    if (user_list->GetString(i, &email) && (user_id == email))
      return true;
  }
  return false;
}

User* UserManagerBase::FindUserInListAndModify(const std::string& user_id) {
  UserList& users = GetUsersAndModify();
  for (UserList::iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->email() == user_id)
      return *it;
  }
  return NULL;
}

void UserManagerBase::GuestUserLoggedIn() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  active_user_ = User::CreateGuestUser();
}

void UserManagerBase::AddUserRecord(User* user) {
  // Add the user to the front of the user list.
  ListPrefUpdate prefs_users_update(GetLocalState(), kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(user->email()));
  users_.insert(users_.begin(), user);
}

void UserManagerBase::RegularUserLoggedIn(const std::string& user_id) {
  // Remove the user from the user list.
  active_user_ = RemoveRegularOrSupervisedUserFromList(user_id);

  // If the user was not found on the user list, create a new user.
  SetIsCurrentUserNew(!active_user_);
  if (IsCurrentUserNew()) {
    active_user_ = User::CreateRegularUser(user_id);
    active_user_->set_oauth_token_status(LoadUserOAuthStatus(user_id));
    SaveUserDisplayName(active_user_->email(),
                        base::UTF8ToUTF16(active_user_->GetAccountName(true)));
  }

  AddUserRecord(active_user_);

  // Make sure that new data is persisted to Local State.
  GetLocalState()->CommitPendingWrite();
}

void UserManagerBase::RegularUserLoggedInAsEphemeral(
    const std::string& user_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  SetIsCurrentUserNew(true);
  is_current_user_ephemeral_regular_user_ = true;
  active_user_ = User::CreateRegularUser(user_id);
}

void UserManagerBase::NotifyOnLogin() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  NotifyActiveUserHashChanged(active_user_->username_hash());
  NotifyActiveUserChanged(active_user_);
  UpdateLoginState();
}

User::OAuthTokenStatus UserManagerBase::LoadUserOAuthStatus(
    const std::string& user_id) const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  const base::DictionaryValue* prefs_oauth_status =
      GetLocalState()->GetDictionary(kUserOAuthTokenStatus);
  int oauth_token_status = User::OAUTH_TOKEN_STATUS_UNKNOWN;
  if (prefs_oauth_status &&
      prefs_oauth_status->GetIntegerWithoutPathExpansion(user_id,
                                                         &oauth_token_status)) {
    User::OAuthTokenStatus status =
        static_cast<User::OAuthTokenStatus>(oauth_token_status);
    HandleUserOAuthTokenStatusChange(user_id, status);

    return status;
  }
  return User::OAUTH_TOKEN_STATUS_UNKNOWN;
}

bool UserManagerBase::LoadForceOnlineSignin(const std::string& user_id) const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  const base::DictionaryValue* prefs_force_online =
      GetLocalState()->GetDictionary(kUserForceOnlineSignin);
  bool force_online_signin = false;
  if (prefs_force_online) {
    prefs_force_online->GetBooleanWithoutPathExpansion(user_id,
                                                       &force_online_signin);
  }
  return force_online_signin;
}

void UserManagerBase::RemoveNonCryptohomeData(const std::string& user_id) {
  PrefService* prefs = GetLocalState();
  DictionaryPrefUpdate prefs_display_name_update(prefs, kUserDisplayName);
  prefs_display_name_update->RemoveWithoutPathExpansion(user_id, NULL);

  DictionaryPrefUpdate prefs_given_name_update(prefs, kUserGivenName);
  prefs_given_name_update->RemoveWithoutPathExpansion(user_id, NULL);

  DictionaryPrefUpdate prefs_display_email_update(prefs, kUserDisplayEmail);
  prefs_display_email_update->RemoveWithoutPathExpansion(user_id, NULL);

  DictionaryPrefUpdate prefs_oauth_update(prefs, kUserOAuthTokenStatus);
  prefs_oauth_update->RemoveWithoutPathExpansion(user_id, NULL);

  DictionaryPrefUpdate prefs_force_online_update(prefs, kUserForceOnlineSignin);
  prefs_force_online_update->RemoveWithoutPathExpansion(user_id, NULL);

  std::string last_active_user = GetLocalState()->GetString(kLastActiveUser);
  if (user_id == last_active_user)
    GetLocalState()->SetString(kLastActiveUser, std::string());
}

User* UserManagerBase::RemoveRegularOrSupervisedUserFromList(
    const std::string& user_id) {
  ListPrefUpdate prefs_users_update(GetLocalState(), kRegularUsers);
  prefs_users_update->Clear();
  User* user = NULL;
  for (UserList::iterator it = users_.begin(); it != users_.end();) {
    const std::string user_email = (*it)->email();
    if (user_email == user_id) {
      user = *it;
      it = users_.erase(it);
    } else {
      if ((*it)->GetType() == USER_TYPE_REGULAR ||
          (*it)->GetType() == USER_TYPE_SUPERVISED) {
        prefs_users_update->Append(new base::StringValue(user_email));
      }
      ++it;
    }
  }
  return user;
}

void UserManagerBase::NotifyActiveUserChanged(const User* active_user) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    ActiveUserChanged(active_user));
}

void UserManagerBase::NotifyUserAddedToSession(const User* added_user,
                                               bool user_switch_pending) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    UserAddedToSession(added_user));
}

void UserManagerBase::NotifyActiveUserHashChanged(const std::string& hash) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    ActiveUserHashChanged(hash));
}

void UserManagerBase::UpdateLoginState() {
  if (!chromeos::LoginState::IsInitialized())
    return;  // LoginState may not be intialized in tests.

  chromeos::LoginState::LoggedInState logged_in_state;
  logged_in_state = active_user_ ? chromeos::LoginState::LOGGED_IN_ACTIVE
                                 : chromeos::LoginState::LOGGED_IN_NONE;

  chromeos::LoginState::LoggedInUserType login_user_type;
  if (logged_in_state == chromeos::LoginState::LOGGED_IN_NONE)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_NONE;
  else if (is_current_user_owner_)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_OWNER;
  else if (active_user_->GetType() == USER_TYPE_GUEST)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_GUEST;
  else if (active_user_->GetType() == USER_TYPE_RETAIL_MODE)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_RETAIL_MODE;
  else if (active_user_->GetType() == USER_TYPE_PUBLIC_ACCOUNT)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT;
  else if (active_user_->GetType() == USER_TYPE_SUPERVISED)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_SUPERVISED;
  else if (active_user_->GetType() == USER_TYPE_KIOSK_APP)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_KIOSK_APP;
  else
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_REGULAR;

  if (primary_user_) {
    chromeos::LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        logged_in_state, login_user_type, primary_user_->username_hash());
  } else {
    chromeos::LoginState::Get()->SetLoggedInState(logged_in_state,
                                                  login_user_type);
  }
}

void UserManagerBase::SetLRUUser(User* user) {
  GetLocalState()->SetString(kLastActiveUser, user->email());
  GetLocalState()->CommitPendingWrite();

  UserList::iterator it =
      std::find(lru_logged_in_users_.begin(), lru_logged_in_users_.end(), user);
  if (it != lru_logged_in_users_.end())
    lru_logged_in_users_.erase(it);
  lru_logged_in_users_.insert(lru_logged_in_users_.begin(), user);
}

void UserManagerBase::SendRegularUserLoginMetrics(const std::string& user_id) {
  // If this isn't the first time Chrome was run after the system booted,
  // assume that Chrome was restarted because a previous session ended.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kFirstExecAfterBoot)) {
    const std::string last_email =
        GetLocalState()->GetString(kLastLoggedInRegularUser);
    const base::TimeDelta time_to_login =
        base::TimeTicks::Now() - manager_creation_time_;
    if (!last_email.empty() && user_id != last_email &&
        time_to_login.InSeconds() <= kLogoutToLoginDelayMaxSec) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("UserManager.LogoutToLoginDelay",
                                  time_to_login.InSeconds(),
                                  0,
                                  kLogoutToLoginDelayMaxSec,
                                  50);
    }
  }
}

void UserManagerBase::UpdateUserAccountLocale(const std::string& user_id,
                                              const std::string& locale) {
  scoped_ptr<std::string> resolved_locale(new std::string());
  if (!locale.empty() && locale != GetApplicationLocale()) {
    // base::Pased will NULL out |resolved_locale|, so cache the underlying ptr.
    std::string* raw_resolved_locale = resolved_locale.get();
    blocking_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::Bind(ResolveLocale,
                   locale,
                   base::Unretained(raw_resolved_locale)),
        base::Bind(&UserManagerBase::DoUpdateAccountLocale,
                   weak_factory_.GetWeakPtr(),
                   user_id,
                   base::Passed(&resolved_locale)));
  } else {
    resolved_locale.reset(new std::string(locale));
    DoUpdateAccountLocale(user_id, resolved_locale.Pass());
  }
}

void UserManagerBase::DoUpdateAccountLocale(
    const std::string& user_id,
    scoped_ptr<std::string> resolved_locale) {
  User* user = FindUserAndModify(user_id);
  if (user && resolved_locale)
    user->SetAccountLocale(*resolved_locale);
}

void UserManagerBase::DeleteUser(User* user) {
  const bool is_active_user = (user == active_user_);
  delete user;
  if (is_active_user)
    active_user_ = NULL;
}

}  // namespace user_manager
