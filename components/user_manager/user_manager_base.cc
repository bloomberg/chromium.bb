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

// A dictionary that maps user ID to the user type.
const char kUserType[] = "UserType";

// A string pref containing the ID of the last user who logged in if it was
// a user with gaia account (regular) or an empty string if it was another type
// of user (guest, kiosk, public account, etc.).
const char kLastLoggedInGaiaUser[] = "LastLoggedInRegularUser";

// A string pref containing the ID of the last active user.
// In case of browser crash, this pref will be used to set active user after
// session restore.
const char kLastActiveUser[] = "LastActiveUser";

// A vector pref of preferences of known users. All new preferences should be
// placed in this list.
const char kKnownUsers[] = "KnownUsers";

// Known user preferences keys (stored in Local State).

// Key of canonical e-mail value.
const char kCanonicalEmail[] = "email";

// Key of obfuscated GAIA id value.
const char kGAIAIdKey[] = "gaia_id";

// Key of whether this user ID refers to a SAML user.
const char kUsingSAMLKey[] = "using_saml";

// Key of Device Id.
const char kDeviceId[] = "device_id";

// Key of GAPS cookie.
const char kGAPSCookie[] = "gaps_cookie";

// Key of the reason for re-auth.
const char kReauthReasonKey[] = "reauth_reason";

// Upper bound for a histogram metric reporting the amount of time between
// one regular user logging out and a different regular user logging in.
const int kLogoutToLoginDelayMaxSec = 1800;

// Callback that is called after user removal is complete.
void OnRemoveUserComplete(const AccountId& account_id,
                          bool success,
                          cryptohome::MountError return_code) {
  // Log the error, but there's not much we can do.
  if (!success) {
    LOG(ERROR) << "Removal of cryptohome for " << account_id.Serialize()
               << " failed, return code: " << return_code;
  }
}

// Runs on SequencedWorkerPool thread. Passes resolved locale to UI thread.
void ResolveLocale(const std::string& raw_locale,
                   std::string* resolved_locale) {
  ignore_result(l10n_util::CheckAndResolveLocale(raw_locale, resolved_locale));
}

// Checks if values in |dict| correspond with |account_id| identity.
bool UserMatches(const AccountId& account_id,
                 const base::DictionaryValue& dict) {
  std::string value;

  // TODO(alemate): update code once user id is really a struct.
  bool has_gaia_id = dict.GetString(kGAIAIdKey, &value);
  if (has_gaia_id && account_id.GetGaiaId() == value)
    return true;

  bool has_email = dict.GetString(kCanonicalEmail, &value);
  if (has_email && account_id.GetUserEmail() == value)
    return true;

  return false;
}

// Fills relevant |dict| values based on |account_id|.
void UpdateIdentity(const AccountId& account_id, base::DictionaryValue& dict) {
  dict.SetString(kCanonicalEmail, account_id.GetUserEmail());
}

}  // namespace

// static
void UserManagerBase::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kRegularUsers);
  registry->RegisterListPref(kKnownUsers);
  registry->RegisterStringPref(kLastLoggedInGaiaUser, std::string());
  registry->RegisterDictionaryPref(kUserDisplayName);
  registry->RegisterDictionaryPref(kUserGivenName);
  registry->RegisterDictionaryPref(kUserDisplayEmail);
  registry->RegisterDictionaryPref(kUserOAuthTokenStatus);
  registry->RegisterDictionaryPref(kUserForceOnlineSignin);
  registry->RegisterDictionaryPref(kUserType);
  registry->RegisterStringPref(kLastActiveUser, std::string());
}

UserManagerBase::UserManagerBase(
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<base::TaskRunner> blocking_task_runner)
    : task_runner_(task_runner),
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

const AccountId& UserManagerBase::GetOwnerAccountId() const {
  return owner_account_id_;
}

void UserManagerBase::UserLoggedIn(const AccountId& account_id,
                                   const std::string& username_hash,
                                   bool browser_restart) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!last_session_active_account_id_initialized_) {
    last_session_active_account_id_ =
        AccountId::FromUserEmail(GetLocalState()->GetString(kLastActiveUser));
    last_session_active_account_id_initialized_ = true;
  }

  User* user = FindUserInListAndModify(account_id);
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

  if (account_id == chromeos::login::GuestAccountId()) {
    GuestUserLoggedIn();
  } else if (IsKioskApp(account_id)) {
    KioskAppLoggedIn(account_id);
  } else if (IsDemoApp(account_id)) {
    DemoAccountLoggedIn();
  } else {
    EnsureUsersLoaded();

    if (user && user->GetType() == USER_TYPE_PUBLIC_ACCOUNT) {
      PublicAccountUserLoggedIn(user);
    } else if ((user && user->GetType() == USER_TYPE_SUPERVISED) ||
               (!user &&
                gaia::ExtractDomainName(account_id.GetUserEmail()) ==
                    chromeos::login::kSupervisedUserDomain)) {
      SupervisedUserLoggedIn(account_id);
    } else if (browser_restart && IsPublicAccountMarkedForRemoval(account_id)) {
      PublicAccountUserLoggedIn(User::CreatePublicAccountUser(account_id));
    } else if (account_id != GetOwnerAccountId() && !user &&
               (AreEphemeralUsersEnabled() || browser_restart)) {
      RegularUserLoggedInAsEphemeral(account_id);
    } else {
      RegularUserLoggedIn(account_id);
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
    if (primary_user_->HasGaiaAccount())
      SendGaiaUserLoginMetrics(account_id);
  }

  UMA_HISTOGRAM_ENUMERATION(
      "UserManager.LoginUserType", active_user_->GetType(), NUM_USER_TYPES);

  GetLocalState()->SetString(
      kLastLoggedInGaiaUser,
      active_user_->HasGaiaAccount() ? account_id.GetUserEmail() : "");

  NotifyOnLogin();
  PerformPostUserLoggedInActions(browser_restart);
}

void UserManagerBase::SwitchActiveUser(const AccountId& account_id) {
  User* user = FindUserAndModify(account_id);
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
  if (!user->HasGaiaAccount()) {
    NOTREACHED() <<
        "Switching to a user without gaia account (non-regular one)";
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
  if (!last_session_active_account_id_.is_valid())
    return;

  if (AccountId::FromUserEmail(GetActiveUser()->email()) !=
      last_session_active_account_id_)
    SwitchActiveUser(last_session_active_account_id_);

  // Make sure that this function gets run only once.
  last_session_active_account_id_.clear();
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

void UserManagerBase::RemoveUser(const AccountId& account_id,
                                 RemoveUserDelegate* delegate) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!CanUserBeRemoved(FindUser(account_id)))
    return;

  RemoveUserInternal(account_id, delegate);
}

void UserManagerBase::RemoveUserInternal(const AccountId& account_id,
                                         RemoveUserDelegate* delegate) {
  RemoveNonOwnerUserInternal(account_id, delegate);
}

void UserManagerBase::RemoveNonOwnerUserInternal(const AccountId& account_id,
                                                 RemoveUserDelegate* delegate) {
  if (delegate)
    delegate->OnBeforeUserRemoved(account_id);
  RemoveUserFromList(account_id);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      account_id.GetUserEmail(), base::Bind(&OnRemoveUserComplete, account_id));

  if (delegate)
    delegate->OnUserRemoved(account_id);
}

void UserManagerBase::RemoveUserFromList(const AccountId& account_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  RemoveNonCryptohomeData(account_id);
  if (user_loading_stage_ == STAGE_LOADED) {
    DeleteUser(RemoveRegularOrSupervisedUserFromList(account_id));
  } else if (user_loading_stage_ == STAGE_LOADING) {
    DCHECK(gaia::ExtractDomainName(account_id.GetUserEmail()) ==
               chromeos::login::kSupervisedUserDomain ||
           HasPendingBootstrap(account_id));
    // Special case, removing partially-constructed supervised user or
    // boostrapping user during user list loading.
    ListPrefUpdate users_update(GetLocalState(), kRegularUsers);
    users_update->Remove(base::StringValue(account_id.GetUserEmail()), nullptr);
    OnUserRemoved(account_id);
  } else {
    NOTREACHED() << "Users are not loaded yet.";
    return;
  }

  // Make sure that new data is persisted to Local State.
  GetLocalState()->CommitPendingWrite();
}

bool UserManagerBase::IsKnownUser(const AccountId& account_id) const {
  return FindUser(account_id) != nullptr;
}

const User* UserManagerBase::FindUser(const AccountId& account_id) const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (active_user_ && active_user_->GetAccountId() == account_id)
    return active_user_;
  return FindUserInList(account_id);
}

User* UserManagerBase::FindUserAndModify(const AccountId& account_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (active_user_ && active_user_->GetAccountId() == account_id)
    return active_user_;
  return FindUserInListAndModify(account_id);
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
    const AccountId& account_id,
    User::OAuthTokenStatus oauth_token_status) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  DVLOG(1) << "Saving user OAuth token status in Local State";
  User* user = FindUserAndModify(account_id);
  if (user)
    user->set_oauth_token_status(oauth_token_status);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id))
    return;

  DictionaryPrefUpdate oauth_status_update(GetLocalState(),
                                           kUserOAuthTokenStatus);
  oauth_status_update->SetWithoutPathExpansion(
      account_id.GetUserEmail(),
      new base::FundamentalValue(static_cast<int>(oauth_token_status)));
}

void UserManagerBase::SaveForceOnlineSignin(const AccountId& account_id,
                                            bool force_online_signin) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id))
    return;

  DictionaryPrefUpdate force_online_update(GetLocalState(),
                                           kUserForceOnlineSignin);
  force_online_update->SetBooleanWithoutPathExpansion(account_id.GetUserEmail(),
                                                      force_online_signin);
}

void UserManagerBase::SaveUserDisplayName(const AccountId& account_id,
                                          const base::string16& display_name) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (User* user = FindUserAndModify(account_id)) {
    user->set_display_name(display_name);

    // Do not update local state if data stored or cached outside the user's
    // cryptohome is to be treated as ephemeral.
    if (!IsUserNonCryptohomeDataEphemeral(account_id)) {
      DictionaryPrefUpdate display_name_update(GetLocalState(),
                                               kUserDisplayName);
      display_name_update->SetWithoutPathExpansion(
          account_id.GetUserEmail(), new base::StringValue(display_name));
    }
  }
}

base::string16 UserManagerBase::GetUserDisplayName(
    const AccountId& account_id) const {
  const User* user = FindUser(account_id);
  return user ? user->display_name() : base::string16();
}

void UserManagerBase::SaveUserDisplayEmail(const AccountId& account_id,
                                           const std::string& display_email) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  User* user = FindUserAndModify(account_id);
  if (!user) {
    LOG(ERROR) << "User not found: " << account_id.GetUserEmail();
    return;  // Ignore if there is no such user.
  }

  user->set_display_email(display_email);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id))
    return;

  DictionaryPrefUpdate display_email_update(GetLocalState(), kUserDisplayEmail);
  display_email_update->SetWithoutPathExpansion(
      account_id.GetUserEmail(), new base::StringValue(display_email));
}

std::string UserManagerBase::GetUserDisplayEmail(
    const AccountId& account_id) const {
  const User* user = FindUser(account_id);
  return user ? user->display_email() : account_id.GetUserEmail();
}

void UserManagerBase::SaveUserType(const AccountId& account_id,
                                   const UserType& user_type) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  User* user = FindUserAndModify(account_id);
  if (!user) {
    LOG(ERROR) << "User not found: " << account_id.GetUserEmail();
    return;  // Ignore if there is no such user.
  }

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id))
    return;

  DictionaryPrefUpdate user_type_update(GetLocalState(), kUserType);
  user_type_update->SetWithoutPathExpansion(
      account_id.GetUserEmail(),
      new base::FundamentalValue(static_cast<int>(user_type)));
  GetLocalState()->CommitPendingWrite();
}

void UserManagerBase::UpdateUsingSAML(const AccountId& account_id,
                                      const bool using_saml) {
  SetKnownUserBooleanPref(account_id, kUsingSAMLKey, using_saml);
}

bool UserManagerBase::FindUsingSAML(const AccountId& account_id) {
  bool using_saml;
  if (GetKnownUserBooleanPref(account_id, kUsingSAMLKey, &using_saml))
    return using_saml;
  return false;
}

void UserManagerBase::UpdateReauthReason(const AccountId& account_id,
                                         const int reauth_reason) {
  SetKnownUserIntegerPref(account_id, kReauthReasonKey, reauth_reason);
}

bool UserManagerBase::FindReauthReason(const AccountId& account_id,
                                       int* out_value) {
  return GetKnownUserIntegerPref(account_id, kReauthReasonKey, out_value);
}

void UserManagerBase::UpdateUserAccountData(
    const AccountId& account_id,
    const UserAccountData& account_data) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  SaveUserDisplayName(account_id, account_data.display_name());

  if (User* user = FindUserAndModify(account_id)) {
    base::string16 given_name = account_data.given_name();
    user->set_given_name(given_name);
    if (!IsUserNonCryptohomeDataEphemeral(account_id)) {
      DictionaryPrefUpdate given_name_update(GetLocalState(), kUserGivenName);
      given_name_update->SetWithoutPathExpansion(
          account_id.GetUserEmail(), new base::StringValue(given_name));
    }
  }

  UpdateUserAccountLocale(account_id, account_data.locale());
}

void UserManagerBase::ParseUserList(const base::ListValue& users_list,
                                    const std::set<AccountId>& existing_users,
                                    std::vector<AccountId>* users_vector,
                                    std::set<AccountId>* users_set) {
  users_vector->clear();
  users_set->clear();
  for (size_t i = 0; i < users_list.GetSize(); ++i) {
    std::string email;
    if (!users_list.GetString(i, &email) || email.empty()) {
      LOG(ERROR) << "Corrupt entry in user list at index " << i << ".";
      continue;
    }

    const AccountId account_id = GetKnownUserAccountId(email, std::string());

    if (existing_users.find(account_id) != existing_users.end() ||
        !users_set->insert(account_id).second) {
      LOG(ERROR) << "Duplicate user: " << email;
      continue;
    }
    users_vector->push_back(account_id);
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
         IsUserNonCryptohomeDataEphemeral(GetLoggedInUser()->GetAccountId());
}

bool UserManagerBase::CanCurrentUserLock() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() && active_user_->can_lock();
}

bool UserManagerBase::IsUserLoggedIn() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return active_user_;
}

bool UserManagerBase::IsLoggedInAsUserWithGaiaAccount() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() && active_user_->HasGaiaAccount();
}

bool UserManagerBase::IsLoggedInAsChildUser() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_CHILD;
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
    const AccountId& account_id) const {
  // Data belonging to the guest and stub users is always ephemeral.
  if (account_id == chromeos::login::GuestAccountId() ||
      account_id == chromeos::login::StubAccountId()) {
    return true;
  }

  // Data belonging to the owner, anyone found on the user list and obsolete
  // public accounts whose data has not been removed yet is not ephemeral.
  if (account_id == GetOwnerAccountId() || UserExistsInList(account_id) ||
      IsPublicAccountMarkedForRemoval(account_id)) {
    return false;
  }

  // Data belonging to the currently logged-in user is ephemeral when:
  // a) The user logged into a regular gaia account while the ephemeral users
  //    policy was enabled.
  //    - or -
  // b) The user logged into any other account type.
  if (IsUserLoggedIn() && (account_id == GetLoggedInUser()->GetAccountId()) &&
      (is_current_user_ephemeral_regular_user_ ||
       !IsLoggedInAsUserWithGaiaAccount())) {
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

bool UserManagerBase::CanUserBeRemoved(const User* user) const {
  // Only regular and supervised users are allowed to be manually removed.
  if (!user || !(user->HasGaiaAccount() || user->IsSupervised()))
    return false;

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

bool UserManagerBase::HasPendingBootstrap(const AccountId& account_id) const {
  return false;
}

void UserManagerBase::SetOwnerId(const AccountId& owner_account_id) {
  owner_account_id_ = owner_account_id;
}

const AccountId& UserManagerBase::GetPendingUserSwitchID() const {
  return pending_user_switch_;
}

void UserManagerBase::SetPendingUserSwitchId(const AccountId& account_id) {
  pending_user_switch_ = account_id;
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
  const base::DictionaryValue* prefs_user_types =
      local_state->GetDictionary(kUserType);

  // Load public sessions first.
  std::set<AccountId> public_sessions_set;
  LoadPublicAccounts(&public_sessions_set);

  // Load regular users and supervised users.
  std::vector<AccountId> regular_users;
  std::set<AccountId> regular_users_set;
  ParseUserList(*prefs_regular_users,
                public_sessions_set,
                &regular_users,
                &regular_users_set);
  for (std::vector<AccountId>::const_iterator it = regular_users.begin();
       it != regular_users.end(); ++it) {
    User* user = nullptr;
    const std::string domain = gaia::ExtractDomainName(it->GetUserEmail());
    if (domain == chromeos::login::kSupervisedUserDomain) {
      user = User::CreateSupervisedUser(*it);
    } else {
      user = User::CreateRegularUser(*it);
      int user_type;
      if (prefs_user_types->GetIntegerWithoutPathExpansion(it->GetUserEmail(),
                                                           &user_type) &&
          user_type == USER_TYPE_CHILD) {
        ChangeUserChildStatus(user, true /* is child */);
      }
    }
    const AccountId account_id = user->GetAccountId();
    user->set_oauth_token_status(LoadUserOAuthStatus(*it));
    user->set_force_online_signin(LoadForceOnlineSignin(*it));
    user->set_using_saml(FindUsingSAML(*it));
    users_.push_back(user);

    base::string16 display_name;
    if (prefs_display_names->GetStringWithoutPathExpansion(it->GetUserEmail(),
                                                           &display_name)) {
      user->set_display_name(display_name);
    }

    base::string16 given_name;
    if (prefs_given_names->GetStringWithoutPathExpansion(it->GetUserEmail(),
                                                         &given_name)) {
      user->set_given_name(given_name);
    }

    std::string display_email;
    if (prefs_display_emails->GetStringWithoutPathExpansion(it->GetUserEmail(),
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

const User* UserManagerBase::FindUserInList(const AccountId& account_id) const {
  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetAccountId() == account_id)
      return *it;
  }
  return nullptr;
}

bool UserManagerBase::UserExistsInList(const AccountId& account_id) const {
  const base::ListValue* user_list = GetLocalState()->GetList(kRegularUsers);
  for (size_t i = 0; i < user_list->GetSize(); ++i) {
    std::string email;
    if (user_list->GetString(i, &email) && (account_id.GetUserEmail() == email))
      return true;
  }
  return false;
}

User* UserManagerBase::FindUserInListAndModify(const AccountId& account_id) {
  UserList& users = GetUsersAndModify();
  for (UserList::iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetAccountId() == account_id)
      return *it;
  }
  return nullptr;
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

void UserManagerBase::RegularUserLoggedIn(const AccountId& account_id) {
  // Remove the user from the user list.
  active_user_ = RemoveRegularOrSupervisedUserFromList(account_id);

  // If the user was not found on the user list, create a new user.
  SetIsCurrentUserNew(!active_user_);
  if (IsCurrentUserNew()) {
    active_user_ = User::CreateRegularUser(account_id);
    active_user_->set_oauth_token_status(LoadUserOAuthStatus(account_id));
    SaveUserDisplayName(active_user_->GetAccountId(),
                        base::UTF8ToUTF16(active_user_->GetAccountName(true)));
  }

  AddUserRecord(active_user_);

  // Make sure that new data is persisted to Local State.
  GetLocalState()->CommitPendingWrite();
}

void UserManagerBase::RegularUserLoggedInAsEphemeral(
    const AccountId& account_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  SetIsCurrentUserNew(true);
  is_current_user_ephemeral_regular_user_ = true;
  active_user_ = User::CreateRegularUser(account_id);
}

void UserManagerBase::NotifyOnLogin() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  NotifyActiveUserHashChanged(active_user_->username_hash());
  NotifyActiveUserChanged(active_user_);
  UpdateLoginState();
}

User::OAuthTokenStatus UserManagerBase::LoadUserOAuthStatus(
    const AccountId& account_id) const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  const base::DictionaryValue* prefs_oauth_status =
      GetLocalState()->GetDictionary(kUserOAuthTokenStatus);
  int oauth_token_status = User::OAUTH_TOKEN_STATUS_UNKNOWN;
  if (prefs_oauth_status &&
      prefs_oauth_status->GetIntegerWithoutPathExpansion(
          account_id.GetUserEmail(), &oauth_token_status)) {
    User::OAuthTokenStatus status =
        static_cast<User::OAuthTokenStatus>(oauth_token_status);
    HandleUserOAuthTokenStatusChange(account_id, status);

    return status;
  }
  return User::OAUTH_TOKEN_STATUS_UNKNOWN;
}

bool UserManagerBase::LoadForceOnlineSignin(const AccountId& account_id) const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  const base::DictionaryValue* prefs_force_online =
      GetLocalState()->GetDictionary(kUserForceOnlineSignin);
  bool force_online_signin = false;
  if (prefs_force_online) {
    prefs_force_online->GetBooleanWithoutPathExpansion(
        account_id.GetUserEmail(), &force_online_signin);
  }
  return force_online_signin;
}

void UserManagerBase::RemoveNonCryptohomeData(const AccountId& account_id) {
  PrefService* prefs = GetLocalState();
  DictionaryPrefUpdate prefs_display_name_update(prefs, kUserDisplayName);
  prefs_display_name_update->RemoveWithoutPathExpansion(
      account_id.GetUserEmail(), nullptr);

  DictionaryPrefUpdate prefs_given_name_update(prefs, kUserGivenName);
  prefs_given_name_update->RemoveWithoutPathExpansion(account_id.GetUserEmail(),
                                                      nullptr);

  DictionaryPrefUpdate prefs_display_email_update(prefs, kUserDisplayEmail);
  prefs_display_email_update->RemoveWithoutPathExpansion(
      account_id.GetUserEmail(), nullptr);

  DictionaryPrefUpdate prefs_oauth_update(prefs, kUserOAuthTokenStatus);
  prefs_oauth_update->RemoveWithoutPathExpansion(account_id.GetUserEmail(),
                                                 nullptr);

  DictionaryPrefUpdate prefs_force_online_update(prefs, kUserForceOnlineSignin);
  prefs_force_online_update->RemoveWithoutPathExpansion(
      account_id.GetUserEmail(), nullptr);

  RemoveKnownUserPrefs(account_id);

  const AccountId last_active_user =
      AccountId::FromUserEmail(GetLocalState()->GetString(kLastActiveUser));
  if (account_id == last_active_user)
    GetLocalState()->SetString(kLastActiveUser, std::string());
}

bool UserManagerBase::FindKnownUserPrefs(
    const AccountId& account_id,
    const base::DictionaryValue** out_value) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return false;

  if (IsUserNonCryptohomeDataEphemeral(account_id))
    return false;

  const base::ListValue* known_users = local_state->GetList(kKnownUsers);
  for (size_t i = 0; i < known_users->GetSize(); ++i) {
    const base::DictionaryValue* element = nullptr;
    if (known_users->GetDictionary(i, &element)) {
      if (UserMatches(account_id, *element)) {
        known_users->GetDictionary(i, out_value);
        return true;
      }
    }
  }
  return false;
}

void UserManagerBase::UpdateKnownUserPrefs(const AccountId& account_id,
                                           const base::DictionaryValue& values,
                                           bool clear) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  if (IsUserNonCryptohomeDataEphemeral(account_id))
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* element = nullptr;
    if (update->GetDictionary(i, &element)) {
      if (UserMatches(account_id, *element)) {
        if (clear)
          element->Clear();
        element->MergeDictionary(&values);
        UpdateIdentity(account_id, *element);
        return;
      }
    }
  }
  scoped_ptr<base::DictionaryValue> new_value(new base::DictionaryValue());
  new_value->MergeDictionary(&values);
  UpdateIdentity(account_id, *new_value);
  update->Append(new_value.release());
}

bool UserManagerBase::GetKnownUserStringPref(const AccountId& account_id,
                                             const std::string& path,
                                             std::string* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindKnownUserPrefs(account_id, &user_pref_dict))
    return false;

  return user_pref_dict->GetString(path, out_value);
}

void UserManagerBase::SetKnownUserStringPref(const AccountId& account_id,
                                             const std::string& path,
                                             const std::string& in_value) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  base::DictionaryValue dict;
  dict.SetString(path, in_value);
  UpdateKnownUserPrefs(account_id, dict, false);
}

bool UserManagerBase::GetKnownUserBooleanPref(const AccountId& account_id,
                                              const std::string& path,
                                              bool* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindKnownUserPrefs(account_id, &user_pref_dict))
    return false;

  return user_pref_dict->GetBoolean(path, out_value);
}

void UserManagerBase::SetKnownUserBooleanPref(const AccountId& account_id,
                                              const std::string& path,
                                              const bool in_value) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  base::DictionaryValue dict;
  dict.SetBoolean(path, in_value);
  UpdateKnownUserPrefs(account_id, dict, false);
}

bool UserManagerBase::GetKnownUserIntegerPref(const AccountId& account_id,
                                              const std::string& path,
                                              int* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindKnownUserPrefs(account_id, &user_pref_dict))
    return false;
  return user_pref_dict->GetInteger(path, out_value);
}

void UserManagerBase::SetKnownUserIntegerPref(const AccountId& account_id,
                                              const std::string& path,
                                              const int in_value) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  base::DictionaryValue dict;
  dict.SetInteger(path, in_value);
  UpdateKnownUserPrefs(account_id, dict, false);
}

AccountId UserManagerBase::GetKnownUserAccountIdImpl(
    const std::string& user_email,
    const std::string& gaia_id) {
  DCHECK(!(user_email.empty() && gaia_id.empty()));

  // We can have several users with the same gaia_id but different e-mails.
  // The opposite case is not possible.
  std::string stored_gaia_id;
  const std::string sanitized_email =
      user_email.empty()
          ? std::string()
          : gaia::CanonicalizeEmail(gaia::SanitizeEmail(user_email));
  if (!sanitized_email.empty() &&
      GetKnownUserStringPref(AccountId::FromUserEmail(sanitized_email),
                             kGAIAIdKey, &stored_gaia_id)) {
    if (!gaia_id.empty() && gaia_id != stored_gaia_id)
      LOG(ERROR) << "User gaia id has changed. Sync will not work.";

    // gaia_id is associated with cryptohome.
    return AccountId::FromUserEmailGaiaId(sanitized_email, stored_gaia_id);
  }

  std::string stored_email;
  // GetKnownUserStringPref() returns the first user record that matches
  // given ID. So we will get the first one if there are multiples.
  if (!gaia_id.empty() &&
      GetKnownUserStringPref(AccountId::FromGaiaId(gaia_id), kCanonicalEmail,
                             &stored_email)) {
    return AccountId::FromUserEmailGaiaId(stored_email, gaia_id);
  }

  return AccountId::FromUserEmailGaiaId(sanitized_email, gaia_id);
}

void UserManagerBase::UpdateGaiaID(const AccountId& account_id,
                                   const std::string& gaia_id) {
  SetKnownUserStringPref(account_id, kGAIAIdKey, gaia_id);
}

bool UserManagerBase::FindGaiaID(const AccountId& account_id,
                                 std::string* out_value) {
  return GetKnownUserStringPref(account_id, kGAIAIdKey, out_value);
}

void UserManagerBase::SetKnownUserDeviceId(const AccountId& account_id,
                                           const std::string& device_id) {
  const std::string known_device_id = GetKnownUserDeviceId(account_id);
  if (!known_device_id.empty() && device_id != known_device_id) {
    NOTREACHED() << "Trying to change device ID for known user.";
  }
  SetKnownUserStringPref(account_id, kDeviceId, device_id);
}

std::string UserManagerBase::GetKnownUserDeviceId(const AccountId& account_id) {
  std::string device_id;
  if (GetKnownUserStringPref(account_id, kDeviceId, &device_id)) {
    return device_id;
  }
  return std::string();
}

void UserManagerBase::SetKnownUserGAPSCookie(const AccountId& account_id,
                                             const std::string& gaps_cookie) {
  SetKnownUserStringPref(account_id, kGAPSCookie, gaps_cookie);
}

std::string UserManagerBase::GetKnownUserGAPSCookie(
    const AccountId& account_id) {
  std::string gaps_cookie;
  if (GetKnownUserStringPref(account_id, kGAPSCookie, &gaps_cookie)) {
    return gaps_cookie;
  }
  return std::string();
}

User* UserManagerBase::RemoveRegularOrSupervisedUserFromList(
    const AccountId& account_id) {
  ListPrefUpdate prefs_users_update(GetLocalState(), kRegularUsers);
  prefs_users_update->Clear();
  User* user = nullptr;
  for (UserList::iterator it = users_.begin(); it != users_.end();) {
    if ((*it)->GetAccountId() == account_id) {
      user = *it;
      it = users_.erase(it);
    } else {
      if ((*it)->HasGaiaAccount() || (*it)->IsSupervised()) {
        const std::string user_email = (*it)->email();
        prefs_users_update->Append(new base::StringValue(user_email));
      }
      ++it;
    }
  }
  OnUserRemoved(account_id);
  return user;
}

void UserManagerBase::RemoveKnownUserPrefs(const AccountId& account_id) {
  ListPrefUpdate update(GetLocalState(), kKnownUsers);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* element = nullptr;
    if (update->GetDictionary(i, &element)) {
      if (UserMatches(account_id, *element)) {
        update->Remove(i, nullptr);
        break;
      }
    }
  }
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

void UserManagerBase::ChangeUserChildStatus(User* user, bool is_child) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (user->IsSupervised() == is_child)
    return;
  user->SetIsChild(is_child);
  SaveUserType(user->GetAccountId(), is_child
                                         ? user_manager::USER_TYPE_CHILD
                                         : user_manager::USER_TYPE_REGULAR);
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    UserChangedChildStatus(user));
}

void UserManagerBase::UpdateLoginState() {
  if (!chromeos::LoginState::IsInitialized())
    return;  // LoginState may not be initialized in tests.

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

void UserManagerBase::SendGaiaUserLoginMetrics(const AccountId& account_id) {
  // If this isn't the first time Chrome was run after the system booted,
  // assume that Chrome was restarted because a previous session ended.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kFirstExecAfterBoot)) {
    const std::string last_email =
        GetLocalState()->GetString(kLastLoggedInGaiaUser);
    const base::TimeDelta time_to_login =
        base::TimeTicks::Now() - manager_creation_time_;
    if (!last_email.empty() &&
        account_id != AccountId::FromUserEmail(last_email) &&
        time_to_login.InSeconds() <= kLogoutToLoginDelayMaxSec) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("UserManager.LogoutToLoginDelay",
                                  time_to_login.InSeconds(),
                                  0,
                                  kLogoutToLoginDelayMaxSec,
                                  50);
    }
  }
}

void UserManagerBase::UpdateUserAccountLocale(const AccountId& account_id,
                                              const std::string& locale) {
  scoped_ptr<std::string> resolved_locale(new std::string());
  if (!locale.empty() && locale != GetApplicationLocale()) {
    // base::Pased will nullptr out |resolved_locale|, so cache the underlying
    // ptr.
    std::string* raw_resolved_locale = resolved_locale.get();
    blocking_task_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(ResolveLocale, locale,
                              base::Unretained(raw_resolved_locale)),
        base::Bind(&UserManagerBase::DoUpdateAccountLocale,
                   weak_factory_.GetWeakPtr(), account_id,
                   base::Passed(&resolved_locale)));
  } else {
    resolved_locale.reset(new std::string(locale));
    DoUpdateAccountLocale(account_id, resolved_locale.Pass());
  }
}

void UserManagerBase::DoUpdateAccountLocale(
    const AccountId& account_id,
    scoped_ptr<std::string> resolved_locale) {
  User* user = FindUserAndModify(account_id);
  if (user && resolved_locale)
    user->SetAccountLocale(*resolved_locale);
}

void UserManagerBase::DeleteUser(User* user) {
  const bool is_active_user = (user == active_user_);
  delete user;
  if (is_active_user)
    active_user_ = nullptr;
}

}  // namespace user_manager
