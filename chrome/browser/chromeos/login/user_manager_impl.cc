// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager_impl.h"

#include <cstddef>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/auth_sync_observer.h"
#include "chrome/browser/chromeos/login/auth_sync_observer_factory.h"
#include "chrome/browser/chromeos/login/default_pinned_apps_field_trial.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/remove_user_delegate.h"
#include "chrome/browser/chromeos/login/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/session_length_limiter.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "policy/policy_constants.h"
#include "ui/views/corewm/corewm_switches.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// A vector pref of the the regular users known on this device, arranged in LRU
// order.
const char kRegularUsers[] = "LoggedInUsers";

// A vector pref of the public accounts defined on this device.
const char kPublicAccounts[] = "PublicAccounts";

// A map from locally managed user local user id to sync user id.
const char kManagedUserSyncId[] =
    "ManagedUserSyncId";

// A map from locally managed user id to manager user id.
const char kManagedUserManagers[] =
    "ManagedUserManagers";

// A map from locally managed user id to manager display name.
const char kManagedUserManagerNames[] =
    "ManagedUserManagerNames";

// A map from locally managed user id to manager display e-mail.
const char kManagedUserManagerDisplayEmails[] =
    "ManagedUserManagerDisplayEmails";

// A vector pref of the locally managed accounts defined on this device, that
// had not logged in yet.
const char kLocallyManagedUsersFirstRun[] = "LocallyManagedUsersFirstRun";

// A pref of the next id for locally managed users generation.
const char kLocallyManagedUsersNextId[] =
    "LocallyManagedUsersNextId";

// A pref of the next id for locally managed users generation.
const char kLocallyManagedUserCreationTransactionDisplayName[] =
    "LocallyManagedUserCreationTransactionDisplayName";

// A pref of the next id for locally managed users generation.
const char kLocallyManagedUserCreationTransactionUserId[] =
    "LocallyManagedUserCreationTransactionUserId";

// A string pref that gets set when a public account is removed but a user is
// currently logged into that account, requiring the account's data to be
// removed after logout.
const char kPublicAccountPendingDataRemoval[] =
    "PublicAccountPendingDataRemoval";

// A dictionary that maps usernames to the displayed name.
const char kUserDisplayName[] = "UserDisplayName";

// A dictionary that maps usernames to the displayed (non-canonical) emails.
const char kUserDisplayEmail[] = "UserDisplayEmail";

// A dictionary that maps usernames to OAuth token presence flag.
const char kUserOAuthTokenStatus[] = "OAuthTokenStatus";

// A string pref containing the ID of the last user who logged in if it was
// a regular user or an empty string if it was another type of user (guest,
// kiosk, public account, etc.).
const char kLastLoggedInRegularUser[] = "LastLoggedInRegularUser";

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

// This method is used to implement UserManager::RemoveUser.
void RemoveUserInternal(const std::string& user_email,
                        chromeos::RemoveUserDelegate* delegate) {
  CrosSettings* cros_settings = CrosSettings::Get();

  // Ensure the value of owner email has been fetched.
  if (CrosSettingsProvider::TRUSTED != cros_settings->PrepareTrustedValues(
          base::Bind(&RemoveUserInternal, user_email, delegate))) {
    // Value of owner email is not fetched yet.  RemoveUserInternal will be
    // called again after fetch completion.
    return;
  }
  std::string owner;
  cros_settings->GetString(kDeviceOwner, &owner);
  if (user_email == owner) {
    // Owner is not allowed to be removed from the device.
    return;
  }

  if (delegate)
    delegate->OnBeforeUserRemoved(user_email);

  chromeos::UserManager::Get()->RemoveUserFromList(user_email);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      user_email, base::Bind(&OnRemoveUserComplete, user_email));

  if (delegate)
    delegate->OnUserRemoved(user_email);
}

// Helper function that copies users from |users_list| to |users_vector| and
// |users_set|. Duplicates and users already present in |existing_users| are
// skipped.
void ParseUserList(const ListValue& users_list,
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

}  // namespace

// static
void UserManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kRegularUsers);
  registry->RegisterListPref(kPublicAccounts);
  registry->RegisterListPref(kLocallyManagedUsersFirstRun);
  registry->RegisterIntegerPref(kLocallyManagedUsersNextId, 0);
  registry->RegisterStringPref(kPublicAccountPendingDataRemoval, "");
  registry->RegisterStringPref(
      kLocallyManagedUserCreationTransactionDisplayName, "");
  registry->RegisterStringPref(
      kLocallyManagedUserCreationTransactionUserId, "");
  registry->RegisterStringPref(kLastLoggedInRegularUser, "");
  registry->RegisterDictionaryPref(kUserOAuthTokenStatus);
  registry->RegisterDictionaryPref(kUserDisplayName);
  registry->RegisterDictionaryPref(kUserDisplayEmail);
  registry->RegisterDictionaryPref(kManagedUserSyncId);
  registry->RegisterDictionaryPref(kManagedUserManagers);
  registry->RegisterDictionaryPref(kManagedUserManagerNames);
  registry->RegisterDictionaryPref(kManagedUserManagerDisplayEmails);

  SessionLengthLimiter::RegisterPrefs(registry);
}

UserManagerImpl::UserManagerImpl()
    : cros_settings_(CrosSettings::Get()),
      device_local_account_policy_service_(NULL),
      users_loaded_(false),
      active_user_(NULL),
      primary_user_(NULL),
      session_started_(false),
      user_sessions_restored_(false),
      is_current_user_owner_(false),
      is_current_user_new_(false),
      is_current_user_ephemeral_regular_user_(false),
      ephemeral_users_enabled_(false),
      user_image_manager_(new UserImageManagerImpl),
      manager_creation_time_(base::TimeTicks::Now()) {
  // UserManager instance should be used only on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this, chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllSources());
  RetrieveTrustedDevicePolicies();
  local_accounts_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::Bind(&UserManagerImpl::RetrieveTrustedDevicePolicies,
                 base::Unretained(this)));
  supervised_users_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefSupervisedUsersEnabled,
      base::Bind(&UserManagerImpl::RetrieveTrustedDevicePolicies,
                 base::Unretained(this)));
  multi_profile_user_controller_.reset(new MultiProfileUserController(
      this, g_browser_process->local_state()));
  UpdateLoginState();
}

UserManagerImpl::~UserManagerImpl() {
  // Can't use STLDeleteElements because of the private destructor of User.
  for (UserList::iterator it = users_.begin(); it != users_.end();
       it = users_.erase(it)) {
    if (active_user_ == *it)
      active_user_ = NULL;
    delete *it;
  }
  // These are pointers to the same User instances that were in users_ list.
  logged_in_users_.clear();
  lru_logged_in_users_.clear();

  delete active_user_;
}

void UserManagerImpl::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  local_accounts_subscription_.reset();
  supervised_users_subscription_.reset();
  // Stop the session length limiter.
  session_length_limiter_.reset();

  if (device_local_account_policy_service_)
    device_local_account_policy_service_->RemoveObserver(this);

  user_image_manager_->Shutdown();
  multi_profile_user_controller_.reset();
}

UserImageManager* UserManagerImpl::GetUserImageManager() {
  return user_image_manager_.get();
}

const UserList& UserManagerImpl::GetUsers() const {
  const_cast<UserManagerImpl*>(this)->EnsureUsersLoaded();
  return users_;
}

UserList UserManagerImpl::GetUsersAdmittedForMultiProfile() const {
  if (!UserManager::IsMultipleProfilesAllowed())
    return UserList();

  UserList result;
  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetType() == User::USER_TYPE_REGULAR &&
        !(*it)->is_logged_in() &&
        multi_profile_user_controller_->IsUserAllowedInSession(
            (*it)->email())) {
      result.push_back(*it);
    }
  }
  return result;
}

const UserList& UserManagerImpl::GetLoggedInUsers() const {
  return logged_in_users_;
}

const UserList& UserManagerImpl::GetLRULoggedInUsers() {
  // If there is no user logged in, we return the active user as the only one.
  if (lru_logged_in_users_.empty() && active_user_) {
    temp_single_logged_in_users_.clear();
    temp_single_logged_in_users_.insert(temp_single_logged_in_users_.begin(),
                                        active_user_);
    return temp_single_logged_in_users_;
  }
  return lru_logged_in_users_;
}

UserList UserManagerImpl::GetUnlockUsers() const {
  UserList unlock_users;
  CHECK(primary_user_);
  unlock_users.push_back(primary_user_);
  return unlock_users;
}

const std::string& UserManagerImpl::GetOwnerEmail() {
  return owner_email_;
}

void UserManagerImpl::UserLoggedIn(const std::string& email,
                                   const std::string& username_hash,
                                   bool browser_restart) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!CommandLine::ForCurrentProcess()->HasSwitch(::switches::kMultiProfiles))
    DCHECK(!IsUserLoggedIn());

  User* user = FindUserInListAndModify(email);
  if (active_user_ && user) {
    user->set_is_logged_in(true);
    user->set_username_hash(username_hash);
    logged_in_users_.push_back(user);
    lru_logged_in_users_.push_back(user);
    // Reset the new user flag if the user already exists.
    is_current_user_new_ = false;
    // Set active user wallpaper back.
    WallpaperManager::Get()->SetUserWallpaper(active_user_->email());
    return;
  }

  if (email == UserManager::kGuestUserName) {
    GuestUserLoggedIn();
  } else if (email == UserManager::kRetailModeUserName) {
    RetailModeUserLoggedIn();
  } else if (policy::IsKioskAppUser(email)) {
    KioskAppLoggedIn(email);
  } else {
    EnsureUsersLoaded();

    if (user && user->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT) {
      PublicAccountUserLoggedIn(user);
    } else if ((user && user->GetType() == User::USER_TYPE_LOCALLY_MANAGED) ||
               (!user && gaia::ExtractDomainName(email) ==
                    UserManager::kLocallyManagedUserDomain)) {
      LocallyManagedUserLoggedIn(email);
    } else if (browser_restart && email == g_browser_process->local_state()->
                   GetString(kPublicAccountPendingDataRemoval)) {
      PublicAccountUserLoggedIn(User::CreatePublicAccountUser(email));
    } else if (email != owner_email_ && !user &&
               (AreEphemeralUsersEnabled() || browser_restart)) {
      RegularUserLoggedInAsEphemeral(email);
    } else {
      RegularUserLoggedIn(email);
    }

    // Initialize the session length limiter and start it only if
    // session limit is defined by the policy.
    session_length_limiter_.reset(new SessionLengthLimiter(NULL,
                                                           browser_restart));
  }
  DCHECK(active_user_);
  active_user_->set_is_logged_in(true);
  active_user_->set_is_active(true);
  active_user_->set_username_hash(username_hash);

  // Place user who just signed in to the top of the logged in users.
  logged_in_users_.insert(logged_in_users_.begin(), active_user_);
  SetLRUUser(active_user_);

  if (!primary_user_)
    primary_user_ = active_user_;

  UMA_HISTOGRAM_ENUMERATION("UserManager.LoginUserType",
                            active_user_->GetType(), User::NUM_USER_TYPES);

  if (active_user_->GetType() == User::USER_TYPE_REGULAR)
    SendRegularUserLoginMetrics(email);
  g_browser_process->local_state()->SetString(kLastLoggedInRegularUser,
    (active_user_->GetType() == User::USER_TYPE_REGULAR) ? email : "");

  NotifyOnLogin();
}

void UserManagerImpl::SwitchActiveUser(const std::string& email) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(::switches::kMultiProfiles))
    return;

  User* user = FindUserAndModify(email);
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
  if (user->GetType() != User::USER_TYPE_REGULAR) {
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

void UserManagerImpl::RestoreActiveSessions() {
  DBusThreadManager::Get()->GetSessionManagerClient()->RetrieveActiveSessions(
      base::Bind(&UserManagerImpl::OnRestoreActiveSessions,
                 base::Unretained(this)));
}

void UserManagerImpl::SessionStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  session_started_ = true;
  UpdateLoginState();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::Source<UserManager>(this),
      content::Details<const User>(active_user_));
  if (is_current_user_new_) {
    // Make sure that the new user's data is persisted to Local State.
    g_browser_process->local_state()->CommitPendingWrite();
  }
}

std::string UserManagerImpl::GenerateUniqueLocallyManagedUserId() {
  int counter = g_browser_process->local_state()->
      GetInteger(kLocallyManagedUsersNextId);
  std::string id;
  bool user_exists;
  do {
    id = base::StringPrintf("%d@%s", counter, kLocallyManagedUserDomain);
    counter++;
    user_exists = (NULL != FindUser(id));
    DCHECK(!user_exists);
    if (user_exists) {
      LOG(ERROR) << "Locally managed user with id " << id << " already exists.";
    }
  } while (user_exists);

  g_browser_process->local_state()->
      SetInteger(kLocallyManagedUsersNextId, counter);

  g_browser_process->local_state()->CommitPendingWrite();
  return id;
}

const User* UserManagerImpl::CreateLocallyManagedUserRecord(
      const std::string& manager_id,
      const std::string& local_user_id,
      const std::string& sync_user_id,
      const string16& display_name) {
  const User* user = FindLocallyManagedUser(display_name);
  DCHECK(!user);
  if (user)
    return user;

  PrefService* local_state = g_browser_process->local_state();

  User* new_user = User::CreateLocallyManagedUser(local_user_id);
  ListPrefUpdate prefs_users_update(local_state, kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(local_user_id));
  ListPrefUpdate prefs_new_users_update(local_state,
                                        kLocallyManagedUsersFirstRun);
  prefs_new_users_update->Insert(0, new base::StringValue(local_user_id));
  users_.insert(users_.begin(), new_user);

  const User* manager = FindUser(manager_id);
  CHECK(manager);

  DictionaryPrefUpdate sync_id_update(local_state, kManagedUserSyncId);
  DictionaryPrefUpdate manager_update(local_state, kManagedUserManagers);
  DictionaryPrefUpdate manager_name_update(local_state,
                                           kManagedUserManagerNames);
  DictionaryPrefUpdate manager_email_update(local_state,
                                            kManagedUserManagerDisplayEmails);
  sync_id_update->SetWithoutPathExpansion(local_user_id,
      new base::StringValue(sync_user_id));
  manager_update->SetWithoutPathExpansion(local_user_id,
      new base::StringValue(manager->email()));
  manager_name_update->SetWithoutPathExpansion(local_user_id,
      new base::StringValue(manager->GetDisplayName()));
  manager_email_update->SetWithoutPathExpansion(local_user_id,
      new base::StringValue(manager->display_email()));

  SaveUserDisplayName(local_user_id, display_name);
  g_browser_process->local_state()->CommitPendingWrite();
  return new_user;
}

std::string UserManagerImpl::GetManagedUserSyncId(
    const std::string& managed_user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* sync_user_ids =
      local_state->GetDictionary(kManagedUserSyncId);
  std::string result;
  sync_user_ids->GetStringWithoutPathExpansion(managed_user_id, &result);
  return result;
}

string16 UserManagerImpl::GetManagerDisplayNameForManagedUser(
    const std::string& managed_user_id) const {
  PrefService* local_state = g_browser_process->local_state();

  const DictionaryValue* manager_names =
      local_state->GetDictionary(kManagedUserManagerNames);
  string16 result;
  if (manager_names->GetStringWithoutPathExpansion(managed_user_id, &result) &&
      !result.empty())
    return result;
  return UTF8ToUTF16(GetManagerDisplayEmailForManagedUser(managed_user_id));
}

std::string UserManagerImpl::GetManagerUserIdForManagedUser(
      const std::string& managed_user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* manager_ids =
      local_state->GetDictionary(kManagedUserManagers);
  std::string result;
  manager_ids->GetStringWithoutPathExpansion(managed_user_id, &result);
  return result;
}

std::string UserManagerImpl::GetManagerDisplayEmailForManagedUser(
      const std::string& managed_user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* manager_mails =
      local_state->GetDictionary(kManagedUserManagerDisplayEmails);
  std::string result;
  if (manager_mails->GetStringWithoutPathExpansion(managed_user_id, &result) &&
      !result.empty()) {
    return result;
  }
  return GetManagerUserIdForManagedUser(managed_user_id);
}

void UserManagerImpl::RemoveUser(const std::string& email,
                                 RemoveUserDelegate* delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const User* user = FindUser(email);
  if (!user || (user->GetType() != User::USER_TYPE_REGULAR &&
                user->GetType() != User::USER_TYPE_LOCALLY_MANAGED))
    return;

  // Sanity check: we must not remove single user. This check may seem
  // redundant at a first sight because this single user must be an owner and
  // we perform special check later in order not to remove an owner.  However
  // due to non-instant nature of ownership assignment this later check may
  // sometimes fail. See http://crosbug.com/12723
  if (users_.size() < 2)
    return;

  // Sanity check: do not allow any of the the logged in users to be removed.
  for (UserList::const_iterator it = logged_in_users_.begin();
       it != logged_in_users_.end(); ++it) {
    if ((*it)->email() == email)
      return;
  }

  RemoveUserInternal(email, delegate);
}

void UserManagerImpl::RemoveUserFromList(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  EnsureUsersLoaded();
  RemoveNonCryptohomeData(email);
  delete RemoveRegularOrLocallyManagedUserFromList(email);
  // Make sure that new data is persisted to Local State.
  g_browser_process->local_state()->CommitPendingWrite();
}

bool UserManagerImpl::IsKnownUser(const std::string& email) const {
  return FindUser(email) != NULL;
}

const User* UserManagerImpl::FindUser(const std::string& email) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (active_user_ && active_user_->email() == email)
    return active_user_;
  return FindUserInList(email);
}

const User* UserManagerImpl::FindLocallyManagedUser(
    const string16& display_name) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if (((*it)->GetType() == User::USER_TYPE_LOCALLY_MANAGED) &&
        ((*it)->display_name() == display_name)) {
      return *it;
    }
  }
  return NULL;
}

const User* UserManagerImpl::GetLoggedInUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

User* UserManagerImpl::GetLoggedInUser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

const User* UserManagerImpl::GetActiveUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

User* UserManagerImpl::GetActiveUser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

const User* UserManagerImpl::GetPrimaryUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return primary_user_;
}

void UserManagerImpl::SaveUserOAuthStatus(
    const std::string& username,
    User::OAuthTokenStatus oauth_token_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Saving user OAuth token status in Local State";
  User* user = FindUserAndModify(username);
  if (user)
    user->set_oauth_token_status(oauth_token_status);

  GetUserFlow(username)->HandleOAuthTokenStatusChange(oauth_token_status);

  // Do not update local store if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(username))
    return;

  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate oauth_status_update(local_state, kUserOAuthTokenStatus);
  oauth_status_update->SetWithoutPathExpansion(username,
      new base::FundamentalValue(static_cast<int>(oauth_token_status)));
}

User::OAuthTokenStatus UserManagerImpl::LoadUserOAuthStatus(
    const std::string& username) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* prefs_oauth_status =
      local_state->GetDictionary(kUserOAuthTokenStatus);
  int oauth_token_status = User::OAUTH_TOKEN_STATUS_UNKNOWN;
  if (prefs_oauth_status &&
      prefs_oauth_status->GetIntegerWithoutPathExpansion(
          username, &oauth_token_status)) {
    User::OAuthTokenStatus result =
        static_cast<User::OAuthTokenStatus>(oauth_token_status);
    if (result == User::OAUTH2_TOKEN_STATUS_INVALID)
      GetUserFlow(username)->HandleOAuthTokenStatusChange(result);
    return result;
  }
  return User::OAUTH_TOKEN_STATUS_UNKNOWN;
}

void UserManagerImpl::SaveUserDisplayName(const std::string& username,
                                          const string16& display_name) {
  UpdateUserAccountDataImpl(username, display_name, NULL);
}

void UserManagerImpl::UpdateUserAccountData(const std::string& username,
                                            const string16& display_name,
                                            const std::string& locale) {
  UpdateUserAccountDataImpl(username, display_name, &locale);
}

void UserManagerImpl::UpdateUserAccountDataImpl(const std::string& username,
                                                const string16& display_name,
                                                const std::string* locale) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  User* user = FindUserAndModify(username);
  if (!user)
    return;  // Ignore if there is no such user.

  // locale is not NULL if User Account has been downloaded
  // (i.e. it is UpdateUserAccountData(), not SaveUserDisplayName() )
  if (locale != NULL) {
    user->SetAccountLocale(*locale);
    RespectLocalePreference(GetProfileByUser(user), user);
  }

  if (display_name.empty())
    return;

  user->set_display_name(display_name);

  // Do not update local store if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(username))
    return;

  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate display_name_update(local_state, kUserDisplayName);
  display_name_update->SetWithoutPathExpansion(
      username,
      new base::StringValue(display_name));

  // Update name if this user is manager of some managed users.
  const DictionaryValue* manager_ids =
      local_state->GetDictionary(kManagedUserManagers);

  DictionaryPrefUpdate manager_name_update(local_state,
                                           kManagedUserManagerNames);
  for (DictionaryValue::Iterator it(*manager_ids); !it.IsAtEnd();
      it.Advance()) {
    std::string manager_id;
    bool has_manager_id = it.value().GetAsString(&manager_id);
    DCHECK(has_manager_id);
    if (manager_id == username) {
      manager_name_update->SetWithoutPathExpansion(
          it.key(),
          new base::StringValue(display_name));
    }
  }
}

string16 UserManagerImpl::GetUserDisplayName(
    const std::string& username) const {
  const User* user = FindUser(username);
  return user ? user->display_name() : string16();
}

void UserManagerImpl::SaveUserDisplayEmail(const std::string& username,
                                           const std::string& display_email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  User* user = FindUserAndModify(username);
  if (!user)
    return;  // Ignore if there is no such user.

  user->set_display_email(display_email);

  // Do not update local store if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(username))
    return;

  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate display_email_update(local_state, kUserDisplayEmail);
  display_email_update->SetWithoutPathExpansion(
      username,
      new base::StringValue(display_email));
}

std::string UserManagerImpl::GetUserDisplayEmail(
    const std::string& username) const {
  const User* user = FindUser(username);
  return user ? user->display_email() : username;
}

// TODO(alemate): http://crbug.com/288941 : Respect preferred language list in
// the Google user profile.
void UserManagerImpl::RespectLocalePreference(Profile* profile,
                                              const User* user) const {
  if (g_browser_process == NULL)
    return;
  if ((user == NULL) || (user != GetPrimaryUser()) ||
      (!user->is_profile_created()))
    return;

  // In case of Multi Profile mode we don't apply profile locale because it is
  // unsafe.
  if (GetLoggedInUsers().size() != 1)
    return;
  const PrefService* prefs = profile->GetPrefs();
  if (prefs == NULL)
    return;

  std::string pref_locale;
  const std::string pref_app_locale =
      prefs->GetString(prefs::kApplicationLocale);
  const std::string pref_bkup_locale =
      prefs->GetString(prefs::kApplicationLocaleBackup);

  pref_locale = pref_app_locale;
  if (pref_locale.empty())
    pref_locale = pref_bkup_locale;

  const std::string* account_locale = NULL;
  if (pref_locale.empty() && user->has_gaia_account()) {
    if (user->GetAccountLocale() == NULL)
      return;  // wait until Account profile is loaded.
    account_locale = user->GetAccountLocale();
    pref_locale = *account_locale;
  }
  const std::string global_app_locale =
      g_browser_process->GetApplicationLocale();
  if (pref_locale.empty())
    pref_locale = global_app_locale;
  DCHECK(!pref_locale.empty());
  LOG(WARNING) << "RespectLocalePreference: "
               << "app_locale='" << pref_app_locale << "', "
               << "bkup_locale='" << pref_bkup_locale << "', "
               << (account_locale != NULL
                       ? (std::string("account_locale='") + (*account_locale) +
                          "'. ")
                       : (std::string("account_locale - unused. ")))
               << " Selected '" << pref_locale << "'";
  profile->ChangeAppLocale(pref_locale, Profile::APP_LOCALE_CHANGED_VIA_LOGIN);
  // Here we don't enable keyboard layouts. Input methods are set up when
  // the user first logs in. Then the user may customize the input methods.
  // Hence changing input methods here, just because the user's UI language
  // is different from the login screen UI language, is not desirable. Note
  // that input method preferences are synced, so users can use their
  // farovite input methods as soon as the preferences are synced.
  chromeos::LanguageSwitchMenu::SwitchLanguage(pref_locale);
}

class UserHashMatcher {
 public:
  explicit UserHashMatcher(const std::string& h) : username_hash(h) {}
  bool operator()(const User* user) const {
    return user->username_hash() == username_hash;
  }

 private:
  const std::string& username_hash;
};

// Returns NULL if user is not created
User* UserManagerImpl::GetUserByProfile(Profile* profile) const {
  if (IsMultipleProfilesAllowed()) {
    const std::string username_hash =
        ProfileHelper::GetUserIdHashFromProfile(profile);
    const UserList& users = GetUsers();
    const UserList::const_iterator pos = std::find_if(
        users.begin(), users.end(), UserHashMatcher(username_hash));
    return (pos != users.end()) ? *pos : NULL;
  }
  return active_user_;
}

Profile* UserManagerImpl::GetProfileByUser(const User* user) const {
  if (IsMultipleProfilesAllowed())
    return ProfileHelper::GetProfileByUserIdHash(user->username_hash());
  return g_browser_process->profile_manager()->GetDefaultProfile();
}

void UserManagerImpl::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED:
      if (!device_local_account_policy_service_) {
        device_local_account_policy_service_ =
            g_browser_process->browser_policy_connector()->
                GetDeviceLocalAccountPolicyService();
        if (device_local_account_policy_service_)
          device_local_account_policy_service_->AddObserver(this);
      }
      RetrieveTrustedDevicePolicies();
      UpdateOwnership();
      break;
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED:
      if (IsUserLoggedIn() &&
          !IsLoggedInAsGuest() &&
          !IsLoggedInAsKioskApp()) {
        Profile* profile = content::Details<Profile>(details).ptr();
        if (!profile->IsOffTheRecord()) {
          AuthSyncObserver* sync_observer =
              AuthSyncObserverFactory::GetInstance()->GetForProfile(profile);
          sync_observer->StartObserving();
          multi_profile_user_controller_->StartObserving(profile);
        }
      }
      break;
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      User* user = GetUserByProfile(profile);
      if (user != NULL) {
        user->set_profile_is_created();
        if (user == active_user_)
          RespectLocalePreference(profile, user);
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void UserManagerImpl::OnPolicyUpdated(const std::string& user_id) {
  UpdatePublicAccountDisplayName(user_id);
  NotifyUserListChanged();
}

void UserManagerImpl::OnDeviceLocalAccountsChanged() {
  // No action needed here, changes to the list of device-local accounts get
  // handled via the kAccountsPrefDeviceLocalAccounts device setting observer.
}

bool UserManagerImpl::IsCurrentUserOwner() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lk(is_current_user_owner_lock_);
  return is_current_user_owner_;
}

void UserManagerImpl::SetCurrentUserIsOwner(bool is_current_user_owner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  {
    base::AutoLock lk(is_current_user_owner_lock_);
    is_current_user_owner_ = is_current_user_owner;
  }
  UpdateLoginState();
}

bool UserManagerImpl::IsCurrentUserNew() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return is_current_user_new_;
}

bool UserManagerImpl::IsCurrentUserNonCryptohomeDataEphemeral() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         IsUserNonCryptohomeDataEphemeral(GetLoggedInUser()->email());
}

bool UserManagerImpl::CanCurrentUserLock() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() && active_user_->can_lock();
}

bool UserManagerImpl::IsUserLoggedIn() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

bool UserManagerImpl::IsLoggedInAsRegularUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         active_user_->GetType() == User::USER_TYPE_REGULAR;
}

bool UserManagerImpl::IsLoggedInAsDemoUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         active_user_->GetType() == User::USER_TYPE_RETAIL_MODE;
}

bool UserManagerImpl::IsLoggedInAsPublicAccount() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
      active_user_->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT;
}

bool UserManagerImpl::IsLoggedInAsGuest() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         active_user_->GetType() == User::USER_TYPE_GUEST;
}

bool UserManagerImpl::IsLoggedInAsLocallyManagedUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
      active_user_->GetType() == User::USER_TYPE_LOCALLY_MANAGED;
}

bool UserManagerImpl::IsLoggedInAsKioskApp() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
      active_user_->GetType() == User::USER_TYPE_KIOSK_APP;
}

bool UserManagerImpl::IsLoggedInAsStub() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() && active_user_->email() == kStubUser;
}

bool UserManagerImpl::IsSessionStarted() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return session_started_;
}

bool UserManagerImpl::UserSessionsRestored() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return user_sessions_restored_;
}

bool UserManagerImpl::HasBrowserRestarted() const {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return base::chromeos::IsRunningOnChromeOS() &&
         command_line->HasSwitch(switches::kLoginUser) &&
         !command_line->HasSwitch(switches::kLoginPassword);
}

bool UserManagerImpl::IsUserNonCryptohomeDataEphemeral(
    const std::string& email) const {
  // Data belonging to the guest, retail mode and stub users is always
  // ephemeral.
  if (email == UserManager::kGuestUserName ||
      email == UserManager::kRetailModeUserName ||
      email == kStubUser) {
    return true;
  }

  // Data belonging to the owner, anyone found on the user list and obsolete
  // public accounts whose data has not been removed yet is not ephemeral.
  if (email == owner_email_  || FindUserInList(email) ||
      email == g_browser_process->local_state()->
          GetString(kPublicAccountPendingDataRemoval)) {
    return false;
  }

  // Data belonging to the currently logged-in user is ephemeral when:
  // a) The user logged into a regular account while the ephemeral users policy
  //    was enabled.
  //    - or -
  // b) The user logged into any other account type.
  if (IsUserLoggedIn() && (email == GetLoggedInUser()->email()) &&
      (is_current_user_ephemeral_regular_user_ || !IsLoggedInAsRegularUser())) {
    return true;
  }

  // Data belonging to any other user is ephemeral when:
  // a) Going through the regular login flow and the ephemeral users policy is
  //    enabled.
  //    - or -
  // b) The browser is restarting after a crash.
  return AreEphemeralUsersEnabled() || HasBrowserRestarted();
}

void UserManagerImpl::AddObserver(UserManager::Observer* obs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_list_.AddObserver(obs);
}

void UserManagerImpl::RemoveObserver(UserManager::Observer* obs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_list_.RemoveObserver(obs);
}

void UserManagerImpl::AddSessionStateObserver(
    UserManager::UserSessionStateObserver* obs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  session_state_observer_list_.AddObserver(obs);
}

void UserManagerImpl::RemoveSessionStateObserver(
    UserManager::UserSessionStateObserver* obs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  session_state_observer_list_.RemoveObserver(obs);
}

void UserManagerImpl::NotifyLocalStateChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(UserManager::Observer, observer_list_,
                    LocalStateChanged(this));
}

void UserManagerImpl::OnProfilePrepared(Profile* profile) {
  LoginUtils::Get()->DoBrowserLaunch(profile,
                                     NULL);     // host_, not needed here

  if (!CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestName)) {
    // Did not log in (we crashed or are debugging), need to restore Sync.
    // TODO(nkostylev): Make sure that OAuth state is restored correctly for all
    // users once it is fully multi-profile aware. http://crbug.com/238987
    // For now if we have other user pending sessions they'll override OAuth
    // session restore for previous users.
    LoginUtils::Get()->RestoreAuthenticationSession(profile);
  }

  // Restore other user sessions if any.
  RestorePendingUserSessions();
}

void UserManagerImpl::EnsureUsersLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_browser_process || !g_browser_process->local_state())
    return;

  if (users_loaded_)
    return;
  users_loaded_ = true;

  // Clean up user list first.
  if (HasFailedLocallyManagedUserCreationTransaction())
    RollbackLocallyManagedUserCreationTransaction();

  PrefService* local_state = g_browser_process->local_state();
  const ListValue* prefs_regular_users = local_state->GetList(kRegularUsers);
  const ListValue* prefs_public_accounts =
      local_state->GetList(kPublicAccounts);
  const DictionaryValue* prefs_display_names =
      local_state->GetDictionary(kUserDisplayName);
  const DictionaryValue* prefs_display_emails =
      local_state->GetDictionary(kUserDisplayEmail);

  // Load regular users and locally managed users.
  std::vector<std::string> regular_users;
  std::set<std::string> regular_users_set;
  ParseUserList(*prefs_regular_users, std::set<std::string>(),
                &regular_users, &regular_users_set);
  for (std::vector<std::string>::const_iterator it = regular_users.begin();
       it != regular_users.end(); ++it) {
    User* user = NULL;
    const std::string domain = gaia::ExtractDomainName(*it);
    if (domain == UserManager::kLocallyManagedUserDomain)
      user = User::CreateLocallyManagedUser(*it);
    else
      user = User::CreateRegularUser(*it);
    user->set_oauth_token_status(LoadUserOAuthStatus(*it));
    users_.push_back(user);

    string16 display_name;
    if (prefs_display_names->GetStringWithoutPathExpansion(*it,
                                                           &display_name)) {
      user->set_display_name(display_name);
    }

    std::string display_email;
    if (prefs_display_emails->GetStringWithoutPathExpansion(*it,
                                                            &display_email)) {
      user->set_display_email(display_email);
    }
  }

  // Load public accounts.
  std::vector<std::string> public_accounts;
  std::set<std::string> public_accounts_set;
  ParseUserList(*prefs_public_accounts, regular_users_set,
                &public_accounts, &public_accounts_set);
  for (std::vector<std::string>::const_iterator it = public_accounts.begin();
       it != public_accounts.end(); ++it) {
    users_.push_back(User::CreatePublicAccountUser(*it));
    UpdatePublicAccountDisplayName(*it);
  }

  user_image_manager_->LoadUserImages(users_);
}

void UserManagerImpl::RetrieveTrustedDevicePolicies() {
  ephemeral_users_enabled_ = false;
  owner_email_ = "";

  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED != cros_settings_->PrepareTrustedValues(
      base::Bind(&UserManagerImpl::RetrieveTrustedDevicePolicies,
                 base::Unretained(this)))) {
    return;
  }

  cros_settings_->GetBoolean(kAccountsPrefEphemeralUsersEnabled,
                             &ephemeral_users_enabled_);
  cros_settings_->GetString(kDeviceOwner, &owner_email_);

  EnsureUsersLoaded();

  bool changed = UpdateAndCleanUpPublicAccounts(
      policy::GetDeviceLocalAccounts(cros_settings_));

  // If ephemeral users are enabled and we are on the login screen, take this
  // opportunity to clean up by removing all regular users except the owner.
  if (ephemeral_users_enabled_ && !IsUserLoggedIn()) {
    ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                      kRegularUsers);
    prefs_users_update->Clear();
    for (UserList::iterator it = users_.begin(); it != users_.end(); ) {
      const std::string user_email = (*it)->email();
      if ((*it)->GetType() == User::USER_TYPE_REGULAR &&
          user_email != owner_email_) {
        RemoveNonCryptohomeData(user_email);
        delete *it;
        it = users_.erase(it);
        changed = true;
      } else {
        if ((*it)->GetType() != User::USER_TYPE_PUBLIC_ACCOUNT)
          prefs_users_update->Append(new base::StringValue(user_email));
        ++it;
      }
    }
  }

  if (changed)
    NotifyUserListChanged();
}

bool UserManagerImpl::AreEphemeralUsersEnabled() const {
  return ephemeral_users_enabled_ &&
      (g_browser_process->browser_policy_connector()->IsEnterpriseManaged() ||
      !owner_email_.empty());
}

UserList& UserManagerImpl::GetUsersAndModify() {
  EnsureUsersLoaded();
  return users_;
}

User* UserManagerImpl::FindUserAndModify(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (active_user_ && active_user_->email() == email)
    return active_user_;
  return FindUserInListAndModify(email);
}

const User* UserManagerImpl::FindUserInList(const std::string& email) const {
  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->email() == email)
      return *it;
  }
  return NULL;
}

User* UserManagerImpl::FindUserInListAndModify(const std::string& email) {
  UserList& users = GetUsersAndModify();
  for (UserList::iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->email() == email)
      return *it;
  }
  return NULL;
}

void UserManagerImpl::GuestUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  active_user_ = User::CreateGuestUser();
  // TODO(nkostylev): Add support for passing guest session cryptohome
  // mount point. Legacy (--login-profile) value will be used for now.
  // http://crosbug.com/230859
  active_user_->SetStubImage(User::kInvalidImageIndex, false);
  // Initializes wallpaper after active_user_ is set.
  WallpaperManager::Get()->SetInitialUserWallpaper(UserManager::kGuestUserName,
                                                   false);
}

void UserManagerImpl::RegularUserLoggedIn(const std::string& email) {
  // Remove the user from the user list.
  active_user_ = RemoveRegularOrLocallyManagedUserFromList(email);

  // If the user was not found on the user list, create a new user.
  is_current_user_new_ = !active_user_;
  if (!active_user_) {
    active_user_ = User::CreateRegularUser(email);
    active_user_->set_oauth_token_status(LoadUserOAuthStatus(email));
    SaveUserDisplayName(active_user_->email(),
                        UTF8ToUTF16(active_user_->GetAccountName(true)));
    WallpaperManager::Get()->SetInitialUserWallpaper(email, true);
  }

  // Add the user to the front of the user list.
  ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                    kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(email));
  users_.insert(users_.begin(), active_user_);

  user_image_manager_->UserLoggedIn(email, is_current_user_new_, false);

  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();

  default_pinned_apps_field_trial::SetupForUser(email, is_current_user_new_);

  // Make sure that new data is persisted to Local State.
  g_browser_process->local_state()->CommitPendingWrite();
}

void UserManagerImpl::RegularUserLoggedInAsEphemeral(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  is_current_user_ephemeral_regular_user_ = true;
  active_user_ = User::CreateRegularUser(email);
  user_image_manager_->UserLoggedIn(email, is_current_user_new_, false);
  WallpaperManager::Get()->SetInitialUserWallpaper(email, false);
}

void UserManagerImpl::LocallyManagedUserLoggedIn(
    const std::string& username) {
  // TODO(nkostylev): Refactor, share code with RegularUserLoggedIn().

  // Remove the user from the user list.
  active_user_ = RemoveRegularOrLocallyManagedUserFromList(username);
  // If the user was not found on the user list, create a new user.
  if (!active_user_) {
    is_current_user_new_ = true;
    active_user_ = User::CreateLocallyManagedUser(username);
    // Leaving OAuth token status at the default state = unknown.
    WallpaperManager::Get()->SetInitialUserWallpaper(username, true);
  } else {
    ListPrefUpdate prefs_new_users_update(g_browser_process->local_state(),
                                          kLocallyManagedUsersFirstRun);
    if (prefs_new_users_update->Remove(base::StringValue(username), NULL)) {
      is_current_user_new_ = true;
      WallpaperManager::Get()->SetInitialUserWallpaper(username, true);
    } else {
      is_current_user_new_ = false;
    }
  }

  // Add the user to the front of the user list.
  ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                    kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(username));
  users_.insert(users_.begin(), active_user_);

  // Now that user is in the list, save display name.
  if (is_current_user_new_) {
    SaveUserDisplayName(active_user_->email(),
                        active_user_->GetDisplayName());
  }

  user_image_manager_->UserLoggedIn(username, is_current_user_new_, true);
  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();

  // Make sure that new data is persisted to Local State.
  g_browser_process->local_state()->CommitPendingWrite();
}

void UserManagerImpl::PublicAccountUserLoggedIn(User* user) {
  is_current_user_new_ = true;
  active_user_ = user;
  // The UserImageManager chooses a random avatar picture when a user logs in
  // for the first time. Tell the UserImageManager that this user is not new to
  // prevent the avatar from getting changed.
  user_image_manager_->UserLoggedIn(user->email(), false, true);
  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();
}

void UserManagerImpl::KioskAppLoggedIn(const std::string& username) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(policy::IsKioskAppUser(username));

  active_user_ = User::CreateKioskAppUser(username);
  active_user_->SetStubImage(User::kInvalidImageIndex, false);
  WallpaperManager::Get()->SetInitialUserWallpaper(username, false);

  // TODO(bartfab): Add KioskAppUsers to the users_ list and keep metadata like
  // the kiosk_app_id in these objects, removing the need to re-parse the
  // device-local account list here to extract the kiosk_app_id.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);
  const policy::DeviceLocalAccount* account = NULL;
  for (std::vector<policy::DeviceLocalAccount>::const_iterator
           it = device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->user_id == username) {
      account = &*it;
      break;
    }
  }
  std::string kiosk_app_id;
  if (account) {
    kiosk_app_id = account->kiosk_app_id;
  } else {
    LOG(ERROR) << "Logged into nonexistent kiosk-app account: " << username;
    NOTREACHED();
  }

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  command_line->AppendSwitchASCII(::switches::kAppId, kiosk_app_id);

  // Disable window animation since kiosk app runs in a single full screen
  // window and window animation causes start-up janks.
  command_line->AppendSwitch(
      views::corewm::switches::kWindowAnimationsDisabled);
}

void UserManagerImpl::RetailModeUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  active_user_ = User::CreateRetailModeUser();
  user_image_manager_->UserLoggedIn(UserManager::kRetailModeUserName,
                                    is_current_user_new_,
                                    true);
  WallpaperManager::Get()->SetInitialUserWallpaper(
      UserManager::kRetailModeUserName, false);
}

void UserManagerImpl::NotifyOnLogin() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NotifyActiveUserHashChanged(active_user_->username_hash());
  NotifyActiveUserChanged(active_user_);

  UpdateLoginState();
  // TODO(nkostylev): Deprecate this notification in favor of
  // ActiveUserChanged() observer call.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<UserManager>(this),
      content::Details<const User>(active_user_));

  // Owner must be first user in session. DeviceSettingsService can't deal with
  // multiple user and will mix up ownership, crbug.com/230018.
  if (GetLoggedInUsers().size() == 1) {
    // Indicate to DeviceSettingsService that the owner key may have become
    // available.
    DeviceSettingsService::Get()->SetUsername(active_user_->email());
  }
}

void UserManagerImpl::UpdateOwnership() {
  bool is_owner = DeviceSettingsService::Get()->HasPrivateOwnerKey();
  VLOG(1) << "Current user " << (is_owner ? "is owner" : "is not owner");

  SetCurrentUserIsOwner(is_owner);
}

void UserManagerImpl::RemoveNonCryptohomeData(const std::string& email) {
  WallpaperManager::Get()->RemoveUserWallpaperInfo(email);
  user_image_manager_->DeleteUserImage(email);

  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate prefs_oauth_update(prefs, kUserOAuthTokenStatus);
  int oauth_status;
  prefs_oauth_update->GetIntegerWithoutPathExpansion(email, &oauth_status);
  prefs_oauth_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_display_name_update(prefs, kUserDisplayName);
  prefs_display_name_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_display_email_update(prefs, kUserDisplayEmail);
  prefs_display_email_update->RemoveWithoutPathExpansion(email, NULL);

  ListPrefUpdate prefs_new_users_update(prefs, kLocallyManagedUsersFirstRun);
  prefs_new_users_update->Remove(base::StringValue(email), NULL);

  DictionaryPrefUpdate managers_update(prefs, kManagedUserManagers);
  managers_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate manager_names_update(prefs,
                                            kManagedUserManagerNames);
  manager_names_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate manager_emails_update(prefs,
                                             kManagedUserManagerDisplayEmails);
  manager_emails_update->RemoveWithoutPathExpansion(email, NULL);

  multi_profile_user_controller_->RemoveCachedValue(email);
}

User* UserManagerImpl::RemoveRegularOrLocallyManagedUserFromList(
    const std::string& username) {
  ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                    kRegularUsers);
  prefs_users_update->Clear();
  User* user = NULL;
  for (UserList::iterator it = users_.begin(); it != users_.end(); ) {
    const std::string user_email = (*it)->email();
    if (user_email == username) {
      user = *it;
      it = users_.erase(it);
    } else {
      if ((*it)->GetType() == User::USER_TYPE_REGULAR ||
          (*it)->GetType() == User::USER_TYPE_LOCALLY_MANAGED) {
        prefs_users_update->Append(new base::StringValue(user_email));
      }
      ++it;
    }
  }
  return user;
}

void UserManagerImpl::CleanUpPublicAccountNonCryptohomeDataPendingRemoval() {
  PrefService* local_state = g_browser_process->local_state();
  const std::string public_account_pending_data_removal =
      local_state->GetString(kPublicAccountPendingDataRemoval);
  if (public_account_pending_data_removal.empty() ||
      (IsUserLoggedIn() &&
       public_account_pending_data_removal == GetActiveUser()->email())) {
    return;
  }

  RemoveNonCryptohomeData(public_account_pending_data_removal);
  local_state->ClearPref(kPublicAccountPendingDataRemoval);
}

void UserManagerImpl::CleanUpPublicAccountNonCryptohomeData(
    const std::vector<std::string>& old_public_accounts) {
  std::set<std::string> users;
  for (UserList::const_iterator it = users_.begin(); it != users_.end(); ++it)
    users.insert((*it)->email());

  // If the user is logged into a public account that has been removed from the
  // user list, mark the account's data as pending removal after logout.
  if (IsLoggedInAsPublicAccount()) {
    const std::string active_user_id = GetActiveUser()->email();
    if (users.find(active_user_id) == users.end()) {
      g_browser_process->local_state()->SetString(
          kPublicAccountPendingDataRemoval, active_user_id);
      users.insert(active_user_id);
    }
  }

  // Remove the data belonging to any other public accounts that are no longer
  // found on the user list.
  for (std::vector<std::string>::const_iterator
           it = old_public_accounts.begin();
       it != old_public_accounts.end(); ++it) {
    if (users.find(*it) == users.end())
      RemoveNonCryptohomeData(*it);
  }
}

bool UserManagerImpl::UpdateAndCleanUpPublicAccounts(
    const std::vector<policy::DeviceLocalAccount>& device_local_accounts) {
  // Try to remove any public account data marked as pending removal.
  CleanUpPublicAccountNonCryptohomeDataPendingRemoval();

  // Get the current list of public accounts.
  std::vector<std::string> old_public_accounts;
  for (UserList::const_iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT)
      old_public_accounts.push_back((*it)->email());
  }

  // Get the new list of public accounts from policy.
  std::vector<std::string> new_public_accounts;
  for (std::vector<policy::DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    // TODO(mnissler, nkostylev, bartfab): Process Kiosk Apps within the
    // standard login framework: http://crbug.com/234694
    if (it->type == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION)
      new_public_accounts.push_back(it->user_id);
  }

  // If the list of public accounts has not changed, return.
  if (new_public_accounts.size() == old_public_accounts.size()) {
    bool changed = false;
    for (size_t i = 0; i < new_public_accounts.size(); ++i) {
      if (new_public_accounts[i] != old_public_accounts[i]) {
        changed = true;
        break;
      }
    }
    if (!changed)
      return false;
  }

  // Persist the new list of public accounts in a pref.
  ListPrefUpdate prefs_public_accounts_update(g_browser_process->local_state(),
                                              kPublicAccounts);
  prefs_public_accounts_update->Clear();
  for (std::vector<std::string>::const_iterator
           it = new_public_accounts.begin();
       it != new_public_accounts.end(); ++it) {
    prefs_public_accounts_update->AppendString(*it);
  }

  // Remove the old public accounts from the user list.
  for (UserList::iterator it = users_.begin(); it != users_.end(); ) {
    if ((*it)->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT) {
      if (*it != GetLoggedInUser())
        delete *it;
      it = users_.erase(it);
    } else {
      ++it;
    }
  }

  // Add the new public accounts to the front of the user list.
  for (std::vector<std::string>::const_reverse_iterator
           it = new_public_accounts.rbegin();
       it != new_public_accounts.rend(); ++it) {
    if (IsLoggedInAsPublicAccount() && *it == GetActiveUser()->email())
      users_.insert(users_.begin(), GetLoggedInUser());
    else
      users_.insert(users_.begin(), User::CreatePublicAccountUser(*it));
    UpdatePublicAccountDisplayName(*it);
  }

  user_image_manager_->LoadUserImages(
      UserList(users_.begin(), users_.begin() + new_public_accounts.size()));

  // Remove data belonging to public accounts that are no longer found on the
  // user list.
  CleanUpPublicAccountNonCryptohomeData(old_public_accounts);

  return true;
}

void UserManagerImpl::UpdatePublicAccountDisplayName(
    const std::string& username) {
  std::string display_name;

  if (device_local_account_policy_service_) {
    policy::DeviceLocalAccountPolicyBroker* broker =
        device_local_account_policy_service_->GetBrokerForUser(username);
    if (broker)
      display_name = broker->GetDisplayName();
  }

  // Set or clear the display name.
  SaveUserDisplayName(username, UTF8ToUTF16(display_name));
}

void UserManagerImpl::StartLocallyManagedUserCreationTransaction(
      const string16& display_name) {
  g_browser_process->local_state()->
      SetString(kLocallyManagedUserCreationTransactionDisplayName,
           UTF16ToASCII(display_name));
  g_browser_process->local_state()->CommitPendingWrite();
}

void UserManagerImpl::SetLocallyManagedUserCreationTransactionUserId(
      const std::string& email) {
  g_browser_process->local_state()->
      SetString(kLocallyManagedUserCreationTransactionUserId,
                email);
  g_browser_process->local_state()->CommitPendingWrite();
}

void UserManagerImpl::CommitLocallyManagedUserCreationTransaction() {
  g_browser_process->local_state()->
      ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
  g_browser_process->local_state()->
      ClearPref(kLocallyManagedUserCreationTransactionUserId);
  g_browser_process->local_state()->CommitPendingWrite();
}

bool UserManagerImpl::HasFailedLocallyManagedUserCreationTransaction() {
  return !(g_browser_process->local_state()->
               GetString(kLocallyManagedUserCreationTransactionDisplayName).
                   empty());
}

void UserManagerImpl::RollbackLocallyManagedUserCreationTransaction() {
  PrefService* prefs = g_browser_process->local_state();

  std::string display_name = prefs->
      GetString(kLocallyManagedUserCreationTransactionDisplayName);
  std::string user_id = prefs->
      GetString(kLocallyManagedUserCreationTransactionUserId);

  LOG(WARNING) << "Cleaning up transaction for "
               << display_name << "/" << user_id;

  if (user_id.empty()) {
    // Not much to do - just remove transaction.
    prefs->ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
    return;
  }

  if (gaia::ExtractDomainName(user_id) != kLocallyManagedUserDomain) {
    LOG(WARNING) << "Clean up transaction for  non-locally managed user found :"
                 << user_id << ", will not remove data";
    prefs->ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
    prefs->ClearPref(kLocallyManagedUserCreationTransactionUserId);
    return;
  }

  ListPrefUpdate prefs_users_update(prefs, kRegularUsers);
  prefs_users_update->Remove(base::StringValue(user_id), NULL);

  RemoveNonCryptohomeData(user_id);

  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      user_id, base::Bind(&OnRemoveUserComplete, user_id));

  prefs->ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
  prefs->ClearPref(kLocallyManagedUserCreationTransactionUserId);
  prefs->CommitPendingWrite();
}

UserFlow* UserManagerImpl::GetCurrentUserFlow() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!IsUserLoggedIn())
    return GetDefaultUserFlow();
  return GetUserFlow(GetLoggedInUser()->email());
}

UserFlow* UserManagerImpl::GetUserFlow(const std::string& email) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FlowMap::const_iterator it = specific_flows_.find(email);
  if (it != specific_flows_.end())
    return it->second;
  return GetDefaultUserFlow();
}

void UserManagerImpl::SetUserFlow(const std::string& email, UserFlow* flow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ResetUserFlow(email);
  specific_flows_[email] = flow;
}

void UserManagerImpl::ResetUserFlow(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FlowMap::iterator it = specific_flows_.find(email);
  if (it != specific_flows_.end()) {
    delete it->second;
    specific_flows_.erase(it);
  }
}

bool UserManagerImpl::GetAppModeChromeClientOAuthInfo(
    std::string* chrome_client_id, std::string* chrome_client_secret) {
  if (!chrome::IsRunningInForcedAppMode() ||
      chrome_client_id_.empty() ||
      chrome_client_secret_.empty()) {
    return false;
  }

  *chrome_client_id = chrome_client_id_;
  *chrome_client_secret = chrome_client_secret_;
  return true;
}

void UserManagerImpl::SetAppModeChromeClientOAuthInfo(
    const std::string& chrome_client_id,
    const std::string& chrome_client_secret) {
  if (!chrome::IsRunningInForcedAppMode())
    return;

  chrome_client_id_ = chrome_client_id;
  chrome_client_secret_ = chrome_client_secret;
}

bool UserManagerImpl::AreLocallyManagedUsersAllowed() const {
  bool locally_managed_users_allowed = false;
  cros_settings_->GetBoolean(kAccountsPrefSupervisedUsersEnabled,
                             &locally_managed_users_allowed);
  return ManagedUserService::AreManagedUsersEnabled() &&
        (locally_managed_users_allowed ||
         !g_browser_process->browser_policy_connector()->IsEnterpriseManaged());
}

base::FilePath UserManagerImpl::GetUserProfileDir(
    const std::string& email) const {
  // TODO(dpolukhin): Remove Chrome OS specific profile path logic from
  // ProfileManager and use only this function to construct profile path.
  // TODO(nkostylev): Cleanup profile dir related code paths crbug.com/294233
  base::FilePath profile_dir;
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(::switches::kMultiProfiles)) {
    const User* user = FindUser(email);
    if (user && !user->username_hash().empty()) {
      profile_dir = base::FilePath(
          chrome::kProfileDirPrefix + user->username_hash());
    }
  } else if (command_line.HasSwitch(chromeos::switches::kLoginProfile)) {
    std::string login_profile_value =
        command_line.GetSwitchValueASCII(chromeos::switches::kLoginProfile);
    if (login_profile_value == chrome::kLegacyProfileDir ||
        login_profile_value == chrome::kTestUserProfileDir) {
      profile_dir = base::FilePath(login_profile_value);
    } else {
      profile_dir = base::FilePath(
          chrome::kProfileDirPrefix + login_profile_value);
    }
  } else {
    // We should never be logged in with no profile dir unless
    // multi-profiles are enabled.
    NOTREACHED();
    profile_dir = base::FilePath();
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_dir = profile_manager->user_data_dir().Append(profile_dir);

  return profile_dir;
}

UserFlow* UserManagerImpl::GetDefaultUserFlow() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!default_flow_.get())
    default_flow_.reset(new DefaultUserFlow());
  return default_flow_.get();
}

void UserManagerImpl::NotifyUserListChanged() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      content::Source<UserManager>(this),
      content::NotificationService::NoDetails());
}

void UserManagerImpl::NotifyActiveUserChanged(const User* active_user) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    ActiveUserChanged(active_user));
}

void UserManagerImpl::NotifyActiveUserHashChanged(const std::string& hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    ActiveUserHashChanged(hash));
}

void UserManagerImpl::NotifyPendingUserSessionsRestoreFinished() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  user_sessions_restored_ = true;
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    PendingUserSessionsRestoreFinished());
}

void UserManagerImpl::UpdateLoginState() {
  if (!LoginState::IsInitialized())
    return;  // LoginState may not be intialized in tests.
  LoginState::LoggedInState logged_in_state;
  logged_in_state = active_user_ ? LoginState::LOGGED_IN_ACTIVE
      : LoginState::LOGGED_IN_NONE;

  LoginState::LoggedInUserType login_user_type;
  if (logged_in_state == LoginState::LOGGED_IN_NONE)
    login_user_type = LoginState::LOGGED_IN_USER_NONE;
  else if (is_current_user_owner_)
    login_user_type = LoginState::LOGGED_IN_USER_OWNER;
  else if (active_user_->GetType() == User::USER_TYPE_GUEST)
    login_user_type = LoginState::LOGGED_IN_USER_GUEST;
  else if (active_user_->GetType() == User::USER_TYPE_RETAIL_MODE)
    login_user_type = LoginState::LOGGED_IN_USER_RETAIL_MODE;
  else if (active_user_->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT)
    login_user_type = LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT;
  else if (active_user_->GetType() == User::USER_TYPE_LOCALLY_MANAGED)
    login_user_type = LoginState::LOGGED_IN_USER_LOCALLY_MANAGED;
  else if (active_user_->GetType() == User::USER_TYPE_KIOSK_APP)
    login_user_type = LoginState::LOGGED_IN_USER_KIOSK_APP;
  else
    login_user_type = LoginState::LOGGED_IN_USER_REGULAR;

  LoginState::Get()->SetLoggedInState(logged_in_state, login_user_type);
}

void UserManagerImpl::SetLRUUser(User* user) {
  UserList::iterator it = std::find(lru_logged_in_users_.begin(),
                                    lru_logged_in_users_.end(),
                                    user);
  if (it != lru_logged_in_users_.end())
    lru_logged_in_users_.erase(it);
  lru_logged_in_users_.insert(lru_logged_in_users_.begin(), user);
}

void UserManagerImpl::OnRestoreActiveSessions(
    const SessionManagerClient::ActiveSessionsMap& sessions,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Could not get list of active user sessions after crash.";
    // If we could not get list of active user sessions it is safer to just
    // sign out so that we don't get in the inconsistent state.
    DBusThreadManager::Get()->GetSessionManagerClient()->StopSession();
    return;
  }

  // One profile has been already loaded on browser start.
  DCHECK(GetLoggedInUsers().size() == 1);
  DCHECK(GetActiveUser());
  std::string active_user_id = GetActiveUser()->email();

  SessionManagerClient::ActiveSessionsMap::const_iterator it;
  for (it = sessions.begin(); it != sessions.end(); ++it) {
    if (active_user_id == it->first)
      continue;
    pending_user_sessions_[it->first] = it->second;
  }
  RestorePendingUserSessions();
}

void UserManagerImpl::RestorePendingUserSessions() {
  if (pending_user_sessions_.empty()) {
    NotifyPendingUserSessionsRestoreFinished();
    return;
  }

  // Get next user to restore sessions and delete it from list.
  SessionManagerClient::ActiveSessionsMap::const_iterator it =
      pending_user_sessions_.begin();
  std::string user_id = it->first;
  std::string user_id_hash = it->second;
  DCHECK(!user_id.empty());
  DCHECK(!user_id_hash.empty());
  pending_user_sessions_.erase(user_id);

  // Check that this user is not logged in yet.
  UserList logged_in_users = GetLoggedInUsers();
  bool user_already_logged_in = false;
  for (UserList::const_iterator it = logged_in_users.begin();
       it != logged_in_users.end(); ++it) {
    const User* user = (*it);
    if (user->email() == user_id) {
      user_already_logged_in = true;
      break;
    }
  }
  DCHECK(!user_already_logged_in);

  if (!user_already_logged_in) {
    // Will call OnProfilePrepared() once profile has been loaded.
    LoginUtils::Get()->PrepareProfile(UserContext(user_id,
                                                  std::string(),  // password
                                                  std::string(),  // auth_code
                                                  user_id_hash),
                                      std::string(),  // display_email
                                      false,          // using_oauth
                                      false,          // has_cookies
                                      true,           // has_active_session
                                      this);
  } else {
    RestorePendingUserSessions();
  }
}

void UserManagerImpl::SendRegularUserLoginMetrics(const std::string& email) {
  // If this isn't the first time Chrome was run after the system booted,
  // assume that Chrome was restarted because a previous session ended.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFirstExecAfterBoot)) {
    const std::string last_email =
        g_browser_process->local_state()->GetString(kLastLoggedInRegularUser);
    const base::TimeDelta time_to_login =
        base::TimeTicks::Now() - manager_creation_time_;
    if (!last_email.empty() && email != last_email &&
        time_to_login.InSeconds() <= kLogoutToLoginDelayMaxSec) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("UserManager.LogoutToLoginDelay",
          time_to_login.InSeconds(), 0, kLogoutToLoginDelayMaxSec, 50);
    }
  }
}

void UserManagerImpl::OnUserNotAllowed() {
  LOG(ERROR) << "Shutdown session because a user is not allowed to be in the "
                "current session";
  chrome::AttemptUserExit();
}

}  // namespace chromeos
