// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/user_manager_impl.h"

#include <cstddef>
#include <set>

#include "ash/multi_profile_uma.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/signin/auth_sync_observer.h"
#include "chrome/browser/chromeos/login/signin/auth_sync_observer_factory.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/remove_user_delegate.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager_impl.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/multiprofiles_session_aborted_dialog.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/session_length_limiter.h"
#include "chrome/browser/net/crl_set_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/chromeos/manager_password_service_factory.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_password_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/login/login_state.h"
#include "chromeos/login/user_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/theme_resources.h"
#include "policy/policy_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/wm/core/wm_core_switches.h"

using content::BrowserThread;

namespace chromeos {
namespace {

// A vector pref of the the regular users known on this device, arranged in LRU
// order.
const char kRegularUsers[] = "LoggedInUsers";

// A vector pref of the public accounts defined on this device.
const char kPublicAccounts[] = "PublicAccounts";

// A string pref that gets set when a public account is removed but a user is
// currently logged into that account, requiring the account's data to be
// removed after logout.
const char kPublicAccountPendingDataRemoval[] =
    "PublicAccountPendingDataRemoval";

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

// Helper function that copies users from |users_list| to |users_vector| and
// |users_set|. Duplicates and users already present in |existing_users| are
// skipped.
void ParseUserList(const base::ListValue& users_list,
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

// Runs on SequencedWorkerPool thread. Passes resolved locale to
// |on_resolve_callback| on UI thread.
void ResolveLocale(
    const std::string& raw_locale,
    base::Callback<void(const std::string&)> on_resolve_callback) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string resolved_locale;
  // Ignore result
  l10n_util::CheckAndResolveLocale(raw_locale, &resolved_locale);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(on_resolve_callback, resolved_locale));
}

}  // namespace

// static
void UserManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kRegularUsers);
  registry->RegisterListPref(kPublicAccounts);
  registry->RegisterStringPref(kPublicAccountPendingDataRemoval, "");
  registry->RegisterStringPref(kLastLoggedInRegularUser, "");
  registry->RegisterDictionaryPref(kUserDisplayName);
  registry->RegisterDictionaryPref(kUserGivenName);
  registry->RegisterDictionaryPref(kUserDisplayEmail);
  registry->RegisterDictionaryPref(kUserOAuthTokenStatus);
  registry->RegisterDictionaryPref(kUserForceOnlineSignin);
  SupervisedUserManager::RegisterPrefs(registry);
  SessionLengthLimiter::RegisterPrefs(registry);
}

UserManagerImpl::UserManagerImpl()
    : cros_settings_(CrosSettings::Get()),
      device_local_account_policy_service_(NULL),
      user_loading_stage_(STAGE_NOT_LOADED),
      active_user_(NULL),
      primary_user_(NULL),
      session_started_(false),
      is_current_user_owner_(false),
      is_current_user_new_(false),
      is_current_user_ephemeral_regular_user_(false),
      ephemeral_users_enabled_(false),
      supervised_user_manager_(new SupervisedUserManagerImpl(this)),
      manager_creation_time_(base::TimeTicks::Now()) {
  UpdateNumberOfUsers();
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
  multi_profile_user_controller_.reset(new MultiProfileUserController(
      this, g_browser_process->local_state()));

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  avatar_policy_observer_.reset(new policy::CloudExternalDataPolicyObserver(
      cros_settings_,
      connector->GetDeviceLocalAccountPolicyService(),
      policy::key::kUserAvatarImage,
      this));
  avatar_policy_observer_->Init();

  wallpaper_policy_observer_.reset(new policy::CloudExternalDataPolicyObserver(
      cros_settings_,
      connector->GetDeviceLocalAccountPolicyService(),
      policy::key::kWallpaperImage,
      this));
  wallpaper_policy_observer_->Init();

  UpdateLoginState();
}

UserManagerImpl::~UserManagerImpl() {
  // Can't use STLDeleteElements because of the private destructor of User.
  for (user_manager::UserList::iterator it = users_.begin(); it != users_.end();
       it = users_.erase(it)) {
    DeleteUser(*it);
  }
  // These are pointers to the same User instances that were in users_ list.
  logged_in_users_.clear();
  lru_logged_in_users_.clear();

  DeleteUser(active_user_);
}

void UserManagerImpl::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  local_accounts_subscription_.reset();
  // Stop the session length limiter.
  session_length_limiter_.reset();

  if (device_local_account_policy_service_)
    device_local_account_policy_service_->RemoveObserver(this);

  for (UserImageManagerMap::iterator it = user_image_managers_.begin(),
                                     ie = user_image_managers_.end();
       it != ie; ++it) {
    it->second->Shutdown();
  }
  multi_profile_user_controller_.reset();
  avatar_policy_observer_.reset();
  wallpaper_policy_observer_.reset();
  registrar_.RemoveAll();
}

MultiProfileUserController* UserManagerImpl::GetMultiProfileUserController() {
  return multi_profile_user_controller_.get();
}

UserImageManager* UserManagerImpl::GetUserImageManager(
    const std::string& user_id) {
  UserImageManagerMap::iterator ui = user_image_managers_.find(user_id);
  if (ui != user_image_managers_.end())
    return ui->second.get();
  linked_ptr<UserImageManagerImpl> mgr(new UserImageManagerImpl(user_id, this));
  user_image_managers_[user_id] = mgr;
  return mgr.get();
}

SupervisedUserManager* UserManagerImpl::GetSupervisedUserManager() {
  return supervised_user_manager_.get();
}

const user_manager::UserList& UserManagerImpl::GetUsers() const {
  const_cast<UserManagerImpl*>(this)->EnsureUsersLoaded();
  return users_;
}

user_manager::UserList UserManagerImpl::GetUsersAdmittedForMultiProfile()
    const {
  // Supervised users are not allowed to use multi-profiles.
  if (logged_in_users_.size() == 1 &&
      GetPrimaryUser()->GetType() != user_manager::USER_TYPE_REGULAR) {
    return user_manager::UserList();
  }

  user_manager::UserList result;
  const user_manager::UserList& users = GetUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it) {
    if ((*it)->GetType() == user_manager::USER_TYPE_REGULAR &&
        !(*it)->is_logged_in()) {
      MultiProfileUserController::UserAllowedInSessionResult check =
          multi_profile_user_controller_->
              IsUserAllowedInSession((*it)->email());
      if (check == MultiProfileUserController::
              NOT_ALLOWED_PRIMARY_USER_POLICY_FORBIDS) {
        return user_manager::UserList();
      }

      // Users with a policy that prevents them being added to a session will be
      // shown in login UI but will be grayed out.
      // Same applies to owner account (see http://crbug.com/385034).
      if (check == MultiProfileUserController::ALLOWED ||
          check == MultiProfileUserController::NOT_ALLOWED_POLICY_FORBIDS ||
          check == MultiProfileUserController::NOT_ALLOWED_OWNER_AS_SECONDARY) {
        result.push_back(*it);
      }
    }
  }

  return result;
}

const user_manager::UserList& UserManagerImpl::GetLoggedInUsers() const {
  return logged_in_users_;
}

const user_manager::UserList& UserManagerImpl::GetLRULoggedInUsers() {
  // If there is no user logged in, we return the active user as the only one.
  if (lru_logged_in_users_.empty() && active_user_) {
    temp_single_logged_in_users_.clear();
    temp_single_logged_in_users_.insert(temp_single_logged_in_users_.begin(),
                                        active_user_);
    return temp_single_logged_in_users_;
  }
  return lru_logged_in_users_;
}

user_manager::UserList UserManagerImpl::GetUnlockUsers() const {
  const user_manager::UserList& logged_in_users = GetLoggedInUsers();
  if (logged_in_users.empty())
    return user_manager::UserList();

  user_manager::UserList unlock_users;
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(primary_user_);
  std::string primary_behavior =
      profile->GetPrefs()->GetString(prefs::kMultiProfileUserBehavior);

  // Specific case: only one logged in user or
  // primary user has primary-only multi-profile policy.
  if (logged_in_users.size() == 1 ||
      primary_behavior == MultiProfileUserController::kBehaviorPrimaryOnly) {
    if (primary_user_->can_lock())
      unlock_users.push_back(primary_user_);
  } else {
    // Fill list of potential unlock users based on multi-profile policy state.
    for (user_manager::UserList::const_iterator it = logged_in_users.begin();
         it != logged_in_users.end();
         ++it) {
      user_manager::User* user = (*it);
      Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
      const std::string behavior =
          profile->GetPrefs()->GetString(prefs::kMultiProfileUserBehavior);
      if (behavior == MultiProfileUserController::kBehaviorUnrestricted &&
          user->can_lock()) {
        unlock_users.push_back(user);
      } else if (behavior == MultiProfileUserController::kBehaviorPrimaryOnly) {
        NOTREACHED()
            << "Spotted primary-only multi-profile policy for non-primary user";
      }
    }
  }

  return unlock_users;
}

const std::string& UserManagerImpl::GetOwnerEmail() {
  return owner_email_;
}

void UserManagerImpl::UserLoggedIn(const std::string& user_id,
                                   const std::string& username_hash,
                                   bool browser_restart) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  user_manager::User* user = FindUserInListAndModify(user_id);
  if (active_user_ && user) {
    user->set_is_logged_in(true);
    user->set_username_hash(username_hash);
    logged_in_users_.push_back(user);
    lru_logged_in_users_.push_back(user);
    // Reset the new user flag if the user already exists.
    is_current_user_new_ = false;
    NotifyUserAddedToSession(user);
    // Remember that we need to switch to this user as soon as profile ready.
    pending_user_switch_ = user_id;
    return;
  }

  policy::DeviceLocalAccount::Type device_local_account_type;
  if (user_id == chromeos::login::kGuestUserName) {
    GuestUserLoggedIn();
  } else if (user_id == chromeos::login::kRetailModeUserName) {
    RetailModeUserLoggedIn();
  } else if (policy::IsDeviceLocalAccountUser(user_id,
                                              &device_local_account_type) &&
             device_local_account_type ==
                 policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    KioskAppLoggedIn(user_id);
  } else if (DemoAppLauncher::IsDemoAppSession(user_id)) {
    DemoAccountLoggedIn();
  } else {
    EnsureUsersLoaded();

    if (user && user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
      PublicAccountUserLoggedIn(user);
    } else if ((user &&
                user->GetType() == user_manager::USER_TYPE_SUPERVISED) ||
               (!user &&
                gaia::ExtractDomainName(user_id) ==
                    chromeos::login::kSupervisedUserDomain)) {
      SupervisedUserLoggedIn(user_id);
    } else if (browser_restart && user_id == g_browser_process->local_state()->
                   GetString(kPublicAccountPendingDataRemoval)) {
      PublicAccountUserLoggedIn(
          user_manager::User::CreatePublicAccountUser(user_id));
    } else if (user_id != owner_email_ && !user &&
               (AreEphemeralUsersEnabled() || browser_restart)) {
      RegularUserLoggedInAsEphemeral(user_id);
    } else {
      RegularUserLoggedIn(user_id);
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

  if (!primary_user_) {
    // Register CRLSet now that the home dir is mounted.
    if (!username_hash.empty()) {
      base::FilePath path;
      path =
          chromeos::ProfileHelper::GetProfilePathByUserIdHash(username_hash);
      component_updater::ComponentUpdateService* cus =
          g_browser_process->component_updater();
      CRLSetFetcher* crl_set = g_browser_process->crl_set_fetcher();
      if (crl_set && cus)
        crl_set->StartInitialLoad(cus, path);
    }
    primary_user_ = active_user_;
    if (primary_user_->GetType() == user_manager::USER_TYPE_REGULAR)
      SendRegularUserLoginMetrics(user_id);
  }

  UMA_HISTOGRAM_ENUMERATION("UserManager.LoginUserType",
                            active_user_->GetType(),
                            user_manager::NUM_USER_TYPES);

  g_browser_process->local_state()->SetString(
      kLastLoggedInRegularUser,
      (active_user_->GetType() == user_manager::USER_TYPE_REGULAR) ? user_id
                                                                   : "");

  NotifyOnLogin();
}

void UserManagerImpl::SwitchActiveUser(const std::string& user_id) {
  user_manager::User* user = FindUserAndModify(user_id);
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
  if (user->GetType() != user_manager::USER_TYPE_REGULAR) {
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

void UserManagerImpl::SessionStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  session_started_ = true;

  UpdateLoginState();
  g_browser_process->platform_part()->SessionManager()->SetSessionState(
      session_manager::SESSION_STATE_ACTIVE);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::Source<UserManager>(this),
      content::Details<const user_manager::User>(active_user_));
  if (is_current_user_new_) {
    // Make sure that the new user's data is persisted to Local State.
    g_browser_process->local_state()->CommitPendingWrite();
  }
}

void UserManagerImpl::RemoveUser(const std::string& user_id,
                                 RemoveUserDelegate* delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const user_manager::User* user = FindUser(user_id);
  if (!user || (user->GetType() != user_manager::USER_TYPE_REGULAR &&
                user->GetType() != user_manager::USER_TYPE_SUPERVISED))
    return;

  // Sanity check: we must not remove single user unless it's an enterprise
  // device. This check may seem redundant at a first sight because
  // this single user must be an owner and we perform special check later
  // in order not to remove an owner. However due to non-instant nature of
  // ownership assignment this later check may sometimes fail.
  // See http://crosbug.com/12723
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos();
  if (users_.size() < 2 && !connector->IsEnterpriseManaged())
    return;

  // Sanity check: do not allow any of the the logged in users to be removed.
  for (user_manager::UserList::const_iterator it = logged_in_users_.begin();
       it != logged_in_users_.end();
       ++it) {
    if ((*it)->email() == user_id)
      return;
  }

  RemoveUserInternal(user_id, delegate);
}

void UserManagerImpl::RemoveUserInternal(const std::string& user_email,
                                         RemoveUserDelegate* delegate) {
  CrosSettings* cros_settings = CrosSettings::Get();

  // Ensure the value of owner email has been fetched.
  if (CrosSettingsProvider::TRUSTED != cros_settings->PrepareTrustedValues(
          base::Bind(&UserManagerImpl::RemoveUserInternal,
                     base::Unretained(this),
                     user_email, delegate))) {
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
  RemoveNonOwnerUserInternal(user_email, delegate);
}

void UserManagerImpl::RemoveNonOwnerUserInternal(const std::string& user_email,
                                                 RemoveUserDelegate* delegate) {
  if (delegate)
    delegate->OnBeforeUserRemoved(user_email);
  RemoveUserFromList(user_email);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      user_email, base::Bind(&OnRemoveUserComplete, user_email));

  if (delegate)
    delegate->OnUserRemoved(user_email);
}

void UserManagerImpl::RemoveUserFromList(const std::string& user_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RemoveNonCryptohomeData(user_id);
  if (user_loading_stage_ == STAGE_LOADED) {
    DeleteUser(RemoveRegularOrSupervisedUserFromList(user_id));
  } else if (user_loading_stage_ == STAGE_LOADING) {
    DCHECK(gaia::ExtractDomainName(user_id) ==
           chromeos::login::kSupervisedUserDomain);
    // Special case, removing partially-constructed supervised user during user
    // list loading.
    ListPrefUpdate users_update(g_browser_process->local_state(),
                                kRegularUsers);
    users_update->Remove(base::StringValue(user_id), NULL);
  } else {
    NOTREACHED() << "Users are not loaded yet.";
    return;
  }
  // Make sure that new data is persisted to Local State.
  g_browser_process->local_state()->CommitPendingWrite();
}

bool UserManagerImpl::IsKnownUser(const std::string& user_id) const {
  return FindUser(user_id) != NULL;
}

const user_manager::User* UserManagerImpl::FindUser(
    const std::string& user_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (active_user_ && active_user_->email() == user_id)
    return active_user_;
  return FindUserInList(user_id);
}

user_manager::User* UserManagerImpl::FindUserAndModify(
    const std::string& user_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (active_user_ && active_user_->email() == user_id)
    return active_user_;
  return FindUserInListAndModify(user_id);
}

const user_manager::User* UserManagerImpl::GetLoggedInUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

user_manager::User* UserManagerImpl::GetLoggedInUser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

const user_manager::User* UserManagerImpl::GetActiveUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

user_manager::User* UserManagerImpl::GetActiveUser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

const user_manager::User* UserManagerImpl::GetPrimaryUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return primary_user_;
}

void UserManagerImpl::SaveUserOAuthStatus(
    const std::string& user_id,
    user_manager::User::OAuthTokenStatus oauth_token_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Saving user OAuth token status in Local State";
  user_manager::User* user = FindUserAndModify(user_id);
  if (user)
    user->set_oauth_token_status(oauth_token_status);

  GetUserFlow(user_id)->HandleOAuthTokenStatusChange(oauth_token_status);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(user_id))
    return;

  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate oauth_status_update(local_state, kUserOAuthTokenStatus);
  oauth_status_update->SetWithoutPathExpansion(user_id,
      new base::FundamentalValue(static_cast<int>(oauth_token_status)));
}

void UserManagerImpl::SaveForceOnlineSignin(const std::string& user_id,
                                            bool force_online_signin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(user_id))
    return;

  DictionaryPrefUpdate force_online_update(g_browser_process->local_state(),
                                           kUserForceOnlineSignin);
  force_online_update->SetBooleanWithoutPathExpansion(user_id,
                                                      force_online_signin);
}

void UserManagerImpl::SaveUserDisplayName(const std::string& user_id,
                                          const base::string16& display_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (user_manager::User* user = FindUserAndModify(user_id)) {
    user->set_display_name(display_name);

    // Do not update local state if data stored or cached outside the user's
    // cryptohome is to be treated as ephemeral.
    if (!IsUserNonCryptohomeDataEphemeral(user_id)) {
      PrefService* local_state = g_browser_process->local_state();

      DictionaryPrefUpdate display_name_update(local_state, kUserDisplayName);
      display_name_update->SetWithoutPathExpansion(
          user_id,
          new base::StringValue(display_name));

      supervised_user_manager_->UpdateManagerName(user_id, display_name);
    }
  }
}

base::string16 UserManagerImpl::GetUserDisplayName(
    const std::string& user_id) const {
  const user_manager::User* user = FindUser(user_id);
  return user ? user->display_name() : base::string16();
}

void UserManagerImpl::SaveUserDisplayEmail(const std::string& user_id,
                                           const std::string& display_email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  user_manager::User* user = FindUserAndModify(user_id);
  if (!user)
    return;  // Ignore if there is no such user.

  user->set_display_email(display_email);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(user_id))
    return;

  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate display_email_update(local_state, kUserDisplayEmail);
  display_email_update->SetWithoutPathExpansion(
      user_id,
      new base::StringValue(display_email));
}

std::string UserManagerImpl::GetUserDisplayEmail(
    const std::string& user_id) const {
  const user_manager::User* user = FindUser(user_id);
  return user ? user->display_email() : user_id;
}

void UserManagerImpl::UpdateUserAccountData(
    const std::string& user_id,
    const UserAccountData& account_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SaveUserDisplayName(user_id, account_data.display_name());

  if (user_manager::User* user = FindUserAndModify(user_id)) {
    base::string16 given_name = account_data.given_name();
    user->set_given_name(given_name);
    if (!IsUserNonCryptohomeDataEphemeral(user_id)) {
      PrefService* local_state = g_browser_process->local_state();

      DictionaryPrefUpdate given_name_update(local_state, kUserGivenName);
      given_name_update->SetWithoutPathExpansion(
          user_id,
          new base::StringValue(given_name));
    }
  }

  UpdateUserAccountLocale(user_id, account_data.locale());
}

void UserManagerImpl::StopPolicyObserverForTesting() {
  avatar_policy_observer_.reset();
  wallpaper_policy_observer_.reset();
}

void UserManagerImpl::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED:
      if (!device_local_account_policy_service_) {
        policy::BrowserPolicyConnectorChromeOS* connector =
            g_browser_process->platform_part()
                ->browser_policy_connector_chromeos();
        device_local_account_policy_service_ =
            connector->GetDeviceLocalAccountPolicyService();
        if (device_local_account_policy_service_)
          device_local_account_policy_service_->AddObserver(this);
      }
      RetrieveTrustedDevicePolicies();
      UpdateOwnership();
      break;
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED: {
      Profile* profile = content::Details<Profile>(details).ptr();
      if (IsUserLoggedIn() &&
          !IsLoggedInAsGuest() &&
          !IsLoggedInAsKioskApp()) {
        if (IsLoggedInAsSupervisedUser())
          SupervisedUserPasswordServiceFactory::GetForProfile(profile);
        if (IsLoggedInAsRegularUser())
          ManagerPasswordServiceFactory::GetForProfile(profile);

        if (!profile->IsOffTheRecord()) {
          AuthSyncObserver* sync_observer =
              AuthSyncObserverFactory::GetInstance()->GetForProfile(profile);
          sync_observer->StartObserving();
          multi_profile_user_controller_->StartObserving(profile);
        }
      }
      break;
    }
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      user_manager::User* user =
          ProfileHelper::Get()->GetUserByProfile(profile);
      if (user != NULL)
        user->set_profile_is_created();
      // If there is pending user switch, do it now.
      if (!pending_user_switch_.empty()) {
        // Call SwitchActiveUser async because otherwise it may cause
        // ProfileManager::GetProfile before the profile gets registered
        // in ProfileManager. It happens in case of sync profile load when
        // NOTIFICATION_PROFILE_CREATED is called synchronously.
        base::MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&UserManagerImpl::SwitchActiveUser,
                     base::Unretained(this),
                     pending_user_switch_));
        pending_user_switch_.clear();
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void UserManagerImpl::OnExternalDataSet(const std::string& policy,
                                        const std::string& user_id) {
  if (policy == policy::key::kUserAvatarImage)
    GetUserImageManager(user_id)->OnExternalDataSet(policy);
  else if (policy == policy::key::kWallpaperImage)
    WallpaperManager::Get()->OnPolicySet(policy, user_id);
  else
    NOTREACHED();
}

void UserManagerImpl::OnExternalDataCleared(const std::string& policy,
                                            const std::string& user_id) {
  if (policy == policy::key::kUserAvatarImage)
    GetUserImageManager(user_id)->OnExternalDataCleared(policy);
  else if (policy == policy::key::kWallpaperImage)
    WallpaperManager::Get()->OnPolicyCleared(policy, user_id);
  else
    NOTREACHED();
}

void UserManagerImpl::OnExternalDataFetched(const std::string& policy,
                                            const std::string& user_id,
                                            scoped_ptr<std::string> data) {
  if (policy == policy::key::kUserAvatarImage)
    GetUserImageManager(user_id)->OnExternalDataFetched(policy, data.Pass());
  else if (policy == policy::key::kWallpaperImage)
    WallpaperManager::Get()->OnPolicyFetched(policy, user_id, data.Pass());
  else
    NOTREACHED();
}

void UserManagerImpl::OnPolicyUpdated(const std::string& user_id) {
  const user_manager::User* user = FindUserInList(user_id);
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return;
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
  return IsUserLoggedIn() && active_user_->can_lock() &&
      GetCurrentUserFlow()->CanLockScreen();
}

bool UserManagerImpl::IsUserLoggedIn() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return active_user_;
}

bool UserManagerImpl::IsLoggedInAsRegularUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         active_user_->GetType() == user_manager::USER_TYPE_REGULAR;
}

bool UserManagerImpl::IsLoggedInAsDemoUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         active_user_->GetType() == user_manager::USER_TYPE_RETAIL_MODE;
}

bool UserManagerImpl::IsLoggedInAsPublicAccount() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         active_user_->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
}

bool UserManagerImpl::IsLoggedInAsGuest() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         active_user_->GetType() == user_manager::USER_TYPE_GUEST;
}

bool UserManagerImpl::IsLoggedInAsSupervisedUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         active_user_->GetType() == user_manager::USER_TYPE_SUPERVISED;
}

bool UserManagerImpl::IsLoggedInAsKioskApp() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         active_user_->GetType() == user_manager::USER_TYPE_KIOSK_APP;
}

bool UserManagerImpl::IsLoggedInAsStub() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() && active_user_->email() == login::kStubUser;
}

bool UserManagerImpl::IsSessionStarted() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return session_started_;
}

bool UserManagerImpl::IsUserNonCryptohomeDataEphemeral(
    const std::string& user_id) const {
  // Data belonging to the guest, retail mode and stub users is always
  // ephemeral.
  if (user_id == login::kGuestUserName ||
      user_id == login::kRetailModeUserName || user_id == login::kStubUser) {
    return true;
  }

  // Data belonging to the owner, anyone found on the user list and obsolete
  // public accounts whose data has not been removed yet is not ephemeral.
  if (user_id == owner_email_  || UserExistsInList(user_id) ||
      user_id == g_browser_process->local_state()->
          GetString(kPublicAccountPendingDataRemoval)) {
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
      UserSessionManager::GetInstance()->HasBrowserRestarted();
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

void UserManagerImpl::EnsureUsersLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_browser_process || !g_browser_process->local_state())
    return;

  if (user_loading_stage_ != STAGE_NOT_LOADED)
    return;
  user_loading_stage_ = STAGE_LOADING;
  // Clean up user list first. All code down the path should be synchronous,
  // so that local state after transaction rollback is in consistent state.
  // This process also should not trigger EnsureUsersLoaded again.
  if (supervised_user_manager_->HasFailedUserCreationTransaction())
    supervised_user_manager_->RollbackUserCreationTransaction();

  PrefService* local_state = g_browser_process->local_state();
  const base::ListValue* prefs_regular_users =
      local_state->GetList(kRegularUsers);
  const base::ListValue* prefs_public_sessions =
      local_state->GetList(kPublicAccounts);
  const base::DictionaryValue* prefs_display_names =
      local_state->GetDictionary(kUserDisplayName);
  const base::DictionaryValue* prefs_given_names =
      local_state->GetDictionary(kUserGivenName);
  const base::DictionaryValue* prefs_display_emails =
      local_state->GetDictionary(kUserDisplayEmail);

  // Load public sessions first.
  std::vector<std::string> public_sessions;
  std::set<std::string> public_sessions_set;
  ParseUserList(*prefs_public_sessions, std::set<std::string>(),
                &public_sessions, &public_sessions_set);
  for (std::vector<std::string>::const_iterator it = public_sessions.begin();
       it != public_sessions.end(); ++it) {
    users_.push_back(user_manager::User::CreatePublicAccountUser(*it));
    UpdatePublicAccountDisplayName(*it);
  }

  // Load regular users and supervised users.
  std::vector<std::string> regular_users;
  std::set<std::string> regular_users_set;
  ParseUserList(*prefs_regular_users, public_sessions_set,
                &regular_users, &regular_users_set);
  for (std::vector<std::string>::const_iterator it = regular_users.begin();
       it != regular_users.end(); ++it) {
    user_manager::User* user = NULL;
    const std::string domain = gaia::ExtractDomainName(*it);
    if (domain == chromeos::login::kSupervisedUserDomain)
      user = user_manager::User::CreateSupervisedUser(*it);
    else
      user = user_manager::User::CreateRegularUser(*it);
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

  for (user_manager::UserList::iterator ui = users_.begin(), ue = users_.end();
       ui != ue;
       ++ui) {
    GetUserImageManager((*ui)->email())->LoadUserImage();
  }
}

void UserManagerImpl::RetrieveTrustedDevicePolicies() {
  ephemeral_users_enabled_ = false;
  owner_email_.clear();

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
    for (user_manager::UserList::iterator it = users_.begin();
         it != users_.end();) {
      const std::string user_email = (*it)->email();
      if ((*it)->GetType() == user_manager::USER_TYPE_REGULAR &&
          user_email != owner_email_) {
        RemoveNonCryptohomeData(user_email);
        DeleteUser(*it);
        it = users_.erase(it);
        changed = true;
      } else {
        if ((*it)->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
          prefs_users_update->Append(new base::StringValue(user_email));
        ++it;
      }
    }
  }

  if (changed)
    NotifyUserListChanged();
}

bool UserManagerImpl::AreEphemeralUsersEnabled() const {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return ephemeral_users_enabled_ &&
         (connector->IsEnterpriseManaged() || !owner_email_.empty());
}

user_manager::UserList& UserManagerImpl::GetUsersAndModify() {
  EnsureUsersLoaded();
  return users_;
}

const user_manager::User* UserManagerImpl::FindUserInList(
    const std::string& user_id) const {
  const user_manager::UserList& users = GetUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it) {
    if ((*it)->email() == user_id)
      return *it;
  }
  return NULL;
}

const bool UserManagerImpl::UserExistsInList(const std::string& user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::ListValue* user_list = local_state->GetList(kRegularUsers);
  for (size_t i = 0; i < user_list->GetSize(); ++i) {
    std::string email;
    if (user_list->GetString(i, &email) && (user_id == email))
      return true;
  }
  return false;
}

user_manager::User* UserManagerImpl::FindUserInListAndModify(
    const std::string& user_id) {
  user_manager::UserList& users = GetUsersAndModify();
  for (user_manager::UserList::iterator it = users.begin(); it != users.end();
       ++it) {
    if ((*it)->email() == user_id)
      return *it;
  }
  return NULL;
}

void UserManagerImpl::GuestUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  active_user_ = user_manager::User::CreateGuestUser();
  // TODO(nkostylev): Add support for passing guest session cryptohome
  // mount point. Legacy (--login-profile) value will be used for now.
  // http://crosbug.com/230859
  active_user_->SetStubImage(
      user_manager::UserImage(
          *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_PROFILE_PICTURE_LOADING)),
      user_manager::User::USER_IMAGE_INVALID,
      false);
  // Initializes wallpaper after active_user_ is set.
  WallpaperManager::Get()->SetUserWallpaperNow(chromeos::login::kGuestUserName);
}

void UserManagerImpl::AddUserRecord(user_manager::User* user) {
  // Add the user to the front of the user list.
  ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                    kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(user->email()));
  users_.insert(users_.begin(), user);
}

void UserManagerImpl::RegularUserLoggedIn(const std::string& user_id) {
  // Remove the user from the user list.
  active_user_ = RemoveRegularOrSupervisedUserFromList(user_id);

  // If the user was not found on the user list, create a new user.
  is_current_user_new_ = !active_user_;
  if (!active_user_) {
    active_user_ = user_manager::User::CreateRegularUser(user_id);
    active_user_->set_oauth_token_status(LoadUserOAuthStatus(user_id));
    SaveUserDisplayName(active_user_->email(),
                        base::UTF8ToUTF16(active_user_->GetAccountName(true)));
    WallpaperManager::Get()->SetUserWallpaperNow(user_id);
  }

  AddUserRecord(active_user_);

  GetUserImageManager(user_id)->UserLoggedIn(is_current_user_new_, false);

  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();

  // Make sure that new data is persisted to Local State.
  g_browser_process->local_state()->CommitPendingWrite();
}

void UserManagerImpl::RegularUserLoggedInAsEphemeral(
    const std::string& user_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  is_current_user_ephemeral_regular_user_ = true;
  active_user_ = user_manager::User::CreateRegularUser(user_id);
  GetUserImageManager(user_id)->UserLoggedIn(is_current_user_new_, false);
  WallpaperManager::Get()->SetUserWallpaperNow(user_id);
}

void UserManagerImpl::SupervisedUserLoggedIn(
    const std::string& user_id) {
  // TODO(nkostylev): Refactor, share code with RegularUserLoggedIn().

  // Remove the user from the user list.
  active_user_ = RemoveRegularOrSupervisedUserFromList(user_id);
  // If the user was not found on the user list, create a new user.
  if (!active_user_) {
    is_current_user_new_ = true;
    active_user_ = user_manager::User::CreateSupervisedUser(user_id);
    // Leaving OAuth token status at the default state = unknown.
    WallpaperManager::Get()->SetUserWallpaperNow(user_id);
  } else {
    if (supervised_user_manager_->CheckForFirstRun(user_id)) {
      is_current_user_new_ = true;
      WallpaperManager::Get()->SetUserWallpaperNow(user_id);
    } else {
      is_current_user_new_ = false;
    }
  }

  // Add the user to the front of the user list.
  ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                    kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(user_id));
  users_.insert(users_.begin(), active_user_);

  // Now that user is in the list, save display name.
  if (is_current_user_new_) {
    SaveUserDisplayName(active_user_->email(),
                        active_user_->GetDisplayName());
  }

  GetUserImageManager(user_id)->UserLoggedIn(is_current_user_new_, true);
  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();

  // Make sure that new data is persisted to Local State.
  g_browser_process->local_state()->CommitPendingWrite();
}

void UserManagerImpl::PublicAccountUserLoggedIn(user_manager::User* user) {
  is_current_user_new_ = true;
  active_user_ = user;
  // The UserImageManager chooses a random avatar picture when a user logs in
  // for the first time. Tell the UserImageManager that this user is not new to
  // prevent the avatar from getting changed.
  GetUserImageManager(user->email())->UserLoggedIn(false, true);
  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();
}

void UserManagerImpl::KioskAppLoggedIn(const std::string& app_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  policy::DeviceLocalAccount::Type device_local_account_type;
  DCHECK(policy::IsDeviceLocalAccountUser(app_id,
                                          &device_local_account_type));
  DCHECK_EQ(policy::DeviceLocalAccount::TYPE_KIOSK_APP,
            device_local_account_type);

  active_user_ = user_manager::User::CreateKioskAppUser(app_id);
  active_user_->SetStubImage(
      user_manager::UserImage(
          *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_PROFILE_PICTURE_LOADING)),
      user_manager::User::USER_IMAGE_INVALID,
      false);

  WallpaperManager::Get()->SetUserWallpaperNow(app_id);

  // TODO(bartfab): Add KioskAppUsers to the users_ list and keep metadata like
  // the kiosk_app_id in these objects, removing the need to re-parse the
  // device-local account list here to extract the kiosk_app_id.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);
  const policy::DeviceLocalAccount* account = NULL;
  for (std::vector<policy::DeviceLocalAccount>::const_iterator
           it = device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->user_id == app_id) {
      account = &*it;
      break;
    }
  }
  std::string kiosk_app_id;
  if (account) {
    kiosk_app_id = account->kiosk_app_id;
  } else {
    LOG(ERROR) << "Logged into nonexistent kiosk-app account: " << app_id;
    NOTREACHED();
  }

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  command_line->AppendSwitchASCII(::switches::kAppId, kiosk_app_id);

  // Disable window animation since kiosk app runs in a single full screen
  // window and window animation causes start-up janks.
  command_line->AppendSwitch(
      wm::switches::kWindowAnimationsDisabled);
}

void UserManagerImpl::DemoAccountLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  active_user_ =
      user_manager::User::CreateKioskAppUser(DemoAppLauncher::kDemoUserName);
  active_user_->SetStubImage(
      user_manager::UserImage(
          *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_PROFILE_PICTURE_LOADING)),
      user_manager::User::USER_IMAGE_INVALID,
      false);
  WallpaperManager::Get()->SetUserWallpaperNow(DemoAppLauncher::kDemoUserName);

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  command_line->AppendSwitchASCII(::switches::kAppId,
                                  DemoAppLauncher::kDemoAppId);

  // Disable window animation since the demo app runs in a single full screen
  // window and window animation causes start-up janks.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      wm::switches::kWindowAnimationsDisabled);
}

void UserManagerImpl::RetailModeUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  active_user_ = user_manager::User::CreateRetailModeUser();
  GetUserImageManager(chromeos::login::kRetailModeUserName)
      ->UserLoggedIn(is_current_user_new_, true);
  WallpaperManager::Get()->SetUserWallpaperNow(
      chromeos::login::kRetailModeUserName);
}

void UserManagerImpl::NotifyOnLogin() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UserSessionManager::OverrideHomedir();

  UpdateNumberOfUsers();
  NotifyActiveUserHashChanged(active_user_->username_hash());
  NotifyActiveUserChanged(active_user_);
  UpdateLoginState();

  // TODO(nkostylev): Deprecate this notification in favor of
  // ActiveUserChanged() observer call.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<UserManager>(this),
      content::Details<const user_manager::User>(active_user_));

  UserSessionManager::GetInstance()->PerformPostUserLoggedInActions();
}

user_manager::User::OAuthTokenStatus UserManagerImpl::LoadUserOAuthStatus(
    const std::string& user_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* prefs_oauth_status =
      local_state->GetDictionary(kUserOAuthTokenStatus);
  int oauth_token_status = user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN;
  if (prefs_oauth_status &&
      prefs_oauth_status->GetIntegerWithoutPathExpansion(
          user_id, &oauth_token_status)) {
    user_manager::User::OAuthTokenStatus result =
        static_cast<user_manager::User::OAuthTokenStatus>(oauth_token_status);
    if (result == user_manager::User::OAUTH2_TOKEN_STATUS_INVALID)
      GetUserFlow(user_id)->HandleOAuthTokenStatusChange(result);
    return result;
  }
  return user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN;
}

bool UserManagerImpl::LoadForceOnlineSignin(const std::string& user_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* prefs_force_online =
      local_state->GetDictionary(kUserForceOnlineSignin);
  bool force_online_signin = false;
  if (prefs_force_online) {
    prefs_force_online->GetBooleanWithoutPathExpansion(user_id,
                                                       &force_online_signin);
  }
  return force_online_signin;
}

void UserManagerImpl::UpdateOwnership() {
  bool is_owner = DeviceSettingsService::Get()->HasPrivateOwnerKey();
  VLOG(1) << "Current user " << (is_owner ? "is owner" : "is not owner");

  SetCurrentUserIsOwner(is_owner);
}

void UserManagerImpl::RemoveNonCryptohomeData(const std::string& user_id) {
  WallpaperManager::Get()->RemoveUserWallpaperInfo(user_id);
  GetUserImageManager(user_id)->DeleteUserImage();

  PrefService* prefs = g_browser_process->local_state();
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

  supervised_user_manager_->RemoveNonCryptohomeData(user_id);

  multi_profile_user_controller_->RemoveCachedValues(user_id);
}

user_manager::User* UserManagerImpl::RemoveRegularOrSupervisedUserFromList(
    const std::string& user_id) {
  ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                    kRegularUsers);
  prefs_users_update->Clear();
  user_manager::User* user = NULL;
  for (user_manager::UserList::iterator it = users_.begin();
       it != users_.end();) {
    const std::string user_email = (*it)->email();
    if (user_email == user_id) {
      user = *it;
      it = users_.erase(it);
    } else {
      if ((*it)->GetType() == user_manager::USER_TYPE_REGULAR ||
          (*it)->GetType() == user_manager::USER_TYPE_SUPERVISED) {
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
  for (user_manager::UserList::const_iterator it = users_.begin();
       it != users_.end();
       ++it)
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
  for (user_manager::UserList::const_iterator it = users_.begin();
       it != users_.end();
       ++it) {
    if ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
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
  for (std::vector<std::string>::const_iterator it =
           new_public_accounts.begin();
       it != new_public_accounts.end(); ++it) {
    prefs_public_accounts_update->AppendString(*it);
  }

  // Remove the old public accounts from the user list.
  for (user_manager::UserList::iterator it = users_.begin();
       it != users_.end();) {
    if ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
      if (*it != GetLoggedInUser())
        DeleteUser(*it);
      it = users_.erase(it);
    } else {
      ++it;
    }
  }

  // Add the new public accounts to the front of the user list.
  for (std::vector<std::string>::const_reverse_iterator it =
           new_public_accounts.rbegin();
       it != new_public_accounts.rend(); ++it) {
    if (IsLoggedInAsPublicAccount() && *it == GetActiveUser()->email())
      users_.insert(users_.begin(), GetLoggedInUser());
    else
      users_.insert(users_.begin(),
                    user_manager::User::CreatePublicAccountUser(*it));
    UpdatePublicAccountDisplayName(*it);
  }

  for (user_manager::UserList::iterator
           ui = users_.begin(),
           ue = users_.begin() + new_public_accounts.size();
       ui != ue;
       ++ui) {
    GetUserImageManager((*ui)->email())->LoadUserImage();
  }

  // Remove data belonging to public accounts that are no longer found on the
  // user list.
  CleanUpPublicAccountNonCryptohomeData(old_public_accounts);

  return true;
}

void UserManagerImpl::UpdatePublicAccountDisplayName(
    const std::string& user_id) {
  std::string display_name;

  if (device_local_account_policy_service_) {
    policy::DeviceLocalAccountPolicyBroker* broker =
        device_local_account_policy_service_->GetBrokerForUser(user_id);
    if (broker)
      display_name = broker->GetDisplayName();
  }

  // Set or clear the display name.
  SaveUserDisplayName(user_id, base::UTF8ToUTF16(display_name));
}

UserFlow* UserManagerImpl::GetCurrentUserFlow() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!IsUserLoggedIn())
    return GetDefaultUserFlow();
  return GetUserFlow(GetLoggedInUser()->email());
}

UserFlow* UserManagerImpl::GetUserFlow(const std::string& user_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FlowMap::const_iterator it = specific_flows_.find(user_id);
  if (it != specific_flows_.end())
    return it->second;
  return GetDefaultUserFlow();
}

void UserManagerImpl::SetUserFlow(const std::string& user_id, UserFlow* flow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ResetUserFlow(user_id);
  specific_flows_[user_id] = flow;
}

void UserManagerImpl::ResetUserFlow(const std::string& user_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FlowMap::iterator it = specific_flows_.find(user_id);
  if (it != specific_flows_.end()) {
    delete it->second;
    specific_flows_.erase(it);
  }
}

bool UserManagerImpl::AreSupervisedUsersAllowed() const {
  bool supervised_users_allowed = false;
  cros_settings_->GetBoolean(kAccountsPrefSupervisedUsersEnabled,
                             &supervised_users_allowed);
  return supervised_users_allowed;
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

void UserManagerImpl::NotifyActiveUserChanged(
    const user_manager::User* active_user) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    ActiveUserChanged(active_user));
}

void UserManagerImpl::NotifyUserAddedToSession(
    const user_manager::User* added_user) {
  UpdateNumberOfUsers();
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    UserAddedToSession(added_user));
}

void UserManagerImpl::NotifyActiveUserHashChanged(const std::string& hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(UserManager::UserSessionStateObserver,
                    session_state_observer_list_,
                    ActiveUserHashChanged(hash));
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
  else if (active_user_->GetType() == user_manager::USER_TYPE_GUEST)
    login_user_type = LoginState::LOGGED_IN_USER_GUEST;
  else if (active_user_->GetType() == user_manager::USER_TYPE_RETAIL_MODE)
    login_user_type = LoginState::LOGGED_IN_USER_RETAIL_MODE;
  else if (active_user_->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    login_user_type = LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT;
  else if (active_user_->GetType() == user_manager::USER_TYPE_SUPERVISED)
    login_user_type = LoginState::LOGGED_IN_USER_SUPERVISED;
  else if (active_user_->GetType() == user_manager::USER_TYPE_KIOSK_APP)
    login_user_type = LoginState::LOGGED_IN_USER_KIOSK_APP;
  else
    login_user_type = LoginState::LOGGED_IN_USER_REGULAR;

  if (primary_user_) {
    LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        logged_in_state, login_user_type, primary_user_->username_hash());
  } else {
    LoginState::Get()->SetLoggedInState(logged_in_state, login_user_type);
  }
}

void UserManagerImpl::SetLRUUser(user_manager::User* user) {
  user_manager::UserList::iterator it =
      std::find(lru_logged_in_users_.begin(), lru_logged_in_users_.end(), user);
  if (it != lru_logged_in_users_.end())
    lru_logged_in_users_.erase(it);
  lru_logged_in_users_.insert(lru_logged_in_users_.begin(), user);
}

void UserManagerImpl::SendRegularUserLoginMetrics(const std::string& user_id) {
  // If this isn't the first time Chrome was run after the system booted,
  // assume that Chrome was restarted because a previous session ended.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFirstExecAfterBoot)) {
    const std::string last_email =
        g_browser_process->local_state()->GetString(kLastLoggedInRegularUser);
    const base::TimeDelta time_to_login =
        base::TimeTicks::Now() - manager_creation_time_;
    if (!last_email.empty() && user_id != last_email &&
        time_to_login.InSeconds() <= kLogoutToLoginDelayMaxSec) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("UserManager.LogoutToLoginDelay",
          time_to_login.InSeconds(), 0, kLogoutToLoginDelayMaxSec, 50);
    }
  }
}

void UserManagerImpl::OnUserNotAllowed(const std::string& user_email) {
  LOG(ERROR) << "Shutdown session because a user is not allowed to be in the "
                "current session";
  chromeos::ShowMultiprofilesSessionAbortedDialog(user_email);
}

void UserManagerImpl::UpdateUserAccountLocale(const std::string& user_id,
                                              const std::string& locale) {
  if (!locale.empty() &&
      locale != g_browser_process->GetApplicationLocale()) {
    BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(ResolveLocale, locale,
            base::Bind(&UserManagerImpl::DoUpdateAccountLocale,
                       base::Unretained(this),
                       user_id)));
  } else {
    DoUpdateAccountLocale(user_id, locale);
  }
}

void UserManagerImpl::DoUpdateAccountLocale(
    const std::string& user_id,
    const std::string& resolved_locale) {
  if (user_manager::User* user = FindUserAndModify(user_id))
    user->SetAccountLocale(resolved_locale);
}

void UserManagerImpl::UpdateNumberOfUsers() {
  size_t users = GetLoggedInUsers().size();
  if (users) {
    // Write the user number as UMA stat when a multi user session is possible.
    if ((users + GetUsersAdmittedForMultiProfile().size()) > 1)
      ash::MultiProfileUMA::RecordUserCount(users);
  }

  base::debug::SetCrashKeyValue(crash_keys::kNumberOfUsers,
      base::StringPrintf("%" PRIuS, GetLoggedInUsers().size()));
}

void UserManagerImpl::DeleteUser(user_manager::User* user) {
  const bool is_active_user = (user == active_user_);
  delete user;
  if (is_active_user)
    active_user_ = NULL;
}

}  // namespace chromeos
