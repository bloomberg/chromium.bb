// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager_impl.h"

#include <cstddef>
#include <set>
#include <vector>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/remove_user_delegate.h"
#include "chrome/browser/chromeos/login/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/session_length_limiter.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "policy/policy_constants.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// A vector pref of the the regular users known on this device, arranged in LRU
// order.
const char kRegularUsers[] = "LoggedInUsers";

// A vector pref of the public accounts defined on this device.
const char kPublicAccounts[] = "PublicAccounts";

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
// skipped. The |logged_in_user| is also skipped and the return value
// indicates whether that user was found in |users_list|.
bool ParseUserList(const ListValue& users_list,
                   const std::set<std::string>& existing_users,
                   const std::string& logged_in_user,
                   std::vector<std::string>* users_vector,
                   std::set<std::string>* users_set) {
  users_vector->clear();
  users_set->clear();
  bool logged_in_user_on_list = false;
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
    if (email == logged_in_user) {
      logged_in_user_on_list = true;
      continue;
    }
    users_vector->push_back(email);
  }
  users_set->erase(logged_in_user);
  return logged_in_user_on_list;
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
  registry->RegisterDictionaryPref(kUserOAuthTokenStatus);
  registry->RegisterDictionaryPref(kUserDisplayName);
  registry->RegisterDictionaryPref(kUserDisplayEmail);
  SessionLengthLimiter::RegisterPrefs(registry);
}

UserManagerImpl::UserManagerImpl()
    : cros_settings_(CrosSettings::Get()),
      device_local_account_policy_service_(NULL),
      users_loaded_(false),
      logged_in_user_(NULL),
      session_started_(false),
      is_current_user_owner_(false),
      is_current_user_new_(false),
      is_current_user_ephemeral_regular_user_(false),
      ephemeral_users_enabled_(false),
      merge_session_state_(MERGE_STATUS_NOT_STARTED),
      observed_sync_service_(NULL),
      user_image_manager_(new UserImageManagerImpl) {
  // UserManager instance should be used only on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this, chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
      content::NotificationService::AllSources());
  RetrieveTrustedDevicePolicies();
}

UserManagerImpl::~UserManagerImpl() {
  // Can't use STLDeleteElements because of the private destructor of User.
  for (UserList::iterator it = users_.begin(); it != users_.end();
       it = users_.erase(it)) {
    if (logged_in_user_ == *it)
      logged_in_user_ = NULL;
    delete *it;
  }
  delete logged_in_user_;
}

void UserManagerImpl::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cros_settings_->RemoveSettingsObserver(kAccountsPrefDeviceLocalAccounts,
                                         this);
  // Stop the session length limiter.
  session_length_limiter_.reset();

  if (device_local_account_policy_service_)
    device_local_account_policy_service_->RemoveObserver(this);
}

UserImageManager* UserManagerImpl::GetUserImageManager() {
  return user_image_manager_.get();
}

const UserList& UserManagerImpl::GetUsers() const {
  const_cast<UserManagerImpl*>(this)->EnsureUsersLoaded();
  return users_;
}

void UserManagerImpl::UserLoggedIn(const std::string& email,
                                   bool browser_restart) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!IsUserLoggedIn());

  if (email == kGuestUserEMail) {
    GuestUserLoggedIn();
  } else if (email == kRetailModeUserEMail) {
    RetailModeUserLoggedIn();
  } else if (gaia::ExtractDomainName(email) == kKioskAppUserDomain) {
    KioskAppLoggedIn(email);
  } else {
    EnsureUsersLoaded();

    User* user = const_cast<User*>(FindUserInList(email));
    if (user && user->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT) {
      PublicAccountUserLoggedIn(user);
    } else if ((user &&
                user->GetType() == User::USER_TYPE_LOCALLY_MANAGED) ||
               (!user &&
                gaia::ExtractDomainName(email) ==
                    UserManager::kLocallyManagedUserDomain)) {
      LocallyManagedUserLoggedIn(email);
    } else if (browser_restart && email == g_browser_process->local_state()->
                   GetString(kPublicAccountPendingDataRemoval)) {
      PublicAccountUserLoggedIn(User::CreatePublicAccountUser(email));
    } else if (email != owner_email_ && !user &&
               (AreEphemeralUsersEnabled() || browser_restart)) {
      RegularUserLoggedInAsEphemeral(email);
    } else {
      RegularUserLoggedIn(email, browser_restart);
    }

    // Initialize the session length limiter and start it only if
    // session limit is defined by the policy.
    session_length_limiter_.reset(new SessionLengthLimiter(NULL,
                                                           browser_restart));
  }

  NotifyOnLogin();
}

void UserManagerImpl::RetailModeUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  logged_in_user_ = User::CreateRetailModeUser();
  user_image_manager_->UserLoggedIn(kRetailModeUserEMail,
                                    is_current_user_new_,
                                    true);
  WallpaperManager::Get()->SetInitialUserWallpaper(kRetailModeUserEMail, false);
}

void UserManagerImpl::GuestUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WallpaperManager::Get()->SetInitialUserWallpaper(kGuestUserEMail, false);
  logged_in_user_ = User::CreateGuestUser();
  logged_in_user_->SetStubImage(User::kInvalidImageIndex, false);
}

void UserManagerImpl::KioskAppLoggedIn(const std::string& username) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(gaia::ExtractDomainName(username), kKioskAppUserDomain);

  WallpaperManager::Get()->SetInitialUserWallpaper(username, false);
  logged_in_user_ = User::CreateKioskAppUser(username);
  logged_in_user_->SetStubImage(User::kInvalidImageIndex, false);

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  command_line->AppendSwitchASCII(::switches::kAppId,
                                  logged_in_user_->GetAccountName(false));
}

void UserManagerImpl::LocallyManagedUserLoggedIn(
    const std::string& username) {
  // TODO(nkostylev): Refactor, share code with RegularUserLoggedIn().

  // Remove the user from the user list.
  logged_in_user_ = RemoveRegularOrLocallyManagedUserFromList(username);
  // If the user was not found on the user list, create a new user.
  if (!logged_in_user_) {
    is_current_user_new_ = true;
    logged_in_user_ = User::CreateLocallyManagedUser(username);
    // Leaving OAuth token status at the default state = unknown.
    WallpaperManager::Get()->SetInitialUserWallpaper(username, true);
  } else {
    ListPrefUpdate prefs_new_users_update(g_browser_process->local_state(),
                                          kLocallyManagedUsersFirstRun);
    if (prefs_new_users_update->Remove(base::StringValue(username), NULL)) {
      is_current_user_new_ = true;
      WallpaperManager::Get()->SetInitialUserWallpaper(username, true);
    }
  }

  // Add the user to the front of the user list.
  ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                    kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(username));
  users_.insert(users_.begin(), logged_in_user_);

  // Now that user is in the list, save display name.
  if (is_current_user_new_) {
    SaveUserDisplayName(logged_in_user_->email(),
                        logged_in_user_->GetDisplayName());
  }

  user_image_manager_->UserLoggedIn(username, is_current_user_new_, true);
  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();

  // Make sure that new data is persisted to Local State.
  g_browser_process->local_state()->CommitPendingWrite();
}

void UserManagerImpl::PublicAccountUserLoggedIn(User* user) {
  is_current_user_new_ = true;
  logged_in_user_ = user;
  // The UserImageManager chooses a random avatar picture when a user logs in
  // for the first time. Tell the UserImageManager that this user is not new to
  // prevent the avatar from getting changed.
  user_image_manager_->UserLoggedIn(user->email(), false, true);
  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();
}

void UserManagerImpl::RegularUserLoggedIn(const std::string& email,
                                          bool browser_restart) {
  // Remove the user from the user list.
  logged_in_user_ = RemoveRegularOrLocallyManagedUserFromList(email);

  // If the user was not found on the user list, create a new user.
  if (!logged_in_user_) {
    is_current_user_new_ = true;
    logged_in_user_ = User::CreateRegularUser(email);
    logged_in_user_->set_oauth_token_status(LoadUserOAuthStatus(email));
    SaveUserDisplayName(logged_in_user_->email(),
                        UTF8ToUTF16(logged_in_user_->GetAccountName(true)));
    WallpaperManager::Get()->SetInitialUserWallpaper(email, true);
  }

  // Add the user to the front of the user list.
  ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                    kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(email));
  users_.insert(users_.begin(), logged_in_user_);

  user_image_manager_->UserLoggedIn(email, is_current_user_new_, false);

  if (!browser_restart) {
    // For GAIA login flow, logged in user wallpaper may not be loaded.
    WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();
  }

  // Make sure that new data is persisted to Local State.
  g_browser_process->local_state()->CommitPendingWrite();
}

void UserManagerImpl::RegularUserLoggedInAsEphemeral(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  is_current_user_ephemeral_regular_user_ = true;
  logged_in_user_ = User::CreateRegularUser(email);
  user_image_manager_->UserLoggedIn(email, is_current_user_new_, false);
  WallpaperManager::Get()->SetInitialUserWallpaper(email, false);
}

void UserManagerImpl::SessionStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  session_started_ = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
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
      const std::string& e_mail,
      const string16& display_name) {
  const User* user = FindLocallyManagedUser(display_name);
  DCHECK(!user);
  if (user)
    return user;

  User* new_user = User::CreateLocallyManagedUser(e_mail);
  ListPrefUpdate prefs_users_update(g_browser_process->local_state(),
                                    kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(e_mail));
  ListPrefUpdate prefs_new_users_update(g_browser_process->local_state(),
                                        kLocallyManagedUsersFirstRun);
  prefs_new_users_update->Insert(0, new base::StringValue(e_mail));
  users_.insert(users_.begin(), new_user);
  SaveUserDisplayName(e_mail, display_name);

  g_browser_process->local_state()->CommitPendingWrite();
  return new_user;
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

  // Sanity check: do not allow the logged-in user to remove himself.
  if (logged_in_user_ && logged_in_user_->email() == email)
    return;

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
  if (logged_in_user_ && logged_in_user_->email() == email)
    return logged_in_user_;
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
  return logged_in_user_;
}

User* UserManagerImpl::GetLoggedInUser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return logged_in_user_;
}

void UserManagerImpl::SaveUserOAuthStatus(
    const std::string& username,
    User::OAuthTokenStatus oauth_token_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Saving user OAuth token status in Local State";
  User* user = const_cast<User*>(FindUser(username));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  User* user = const_cast<User*>(FindUser(username));
  if (!user)
    return;  // Ignore if there is no such user.

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
}

string16 UserManagerImpl::GetUserDisplayName(
    const std::string& username) const {
  const User* user = FindUser(username);
  return user ? user->display_name() : string16();
}

void UserManagerImpl::SaveUserDisplayEmail(const std::string& username,
                                           const std::string& display_email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  User* user = const_cast<User*>(FindUser(username));
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
      CheckOwnership();
      RetrieveTrustedDevicePolicies();
      break;
    case chrome::NOTIFICATION_PROFILE_ADDED:
      if (IsUserLoggedIn() && !IsLoggedInAsGuest() &&
          !IsLoggedInAsLocallyManagedUser()) {
        Profile* profile = content::Source<Profile>(source).ptr();
        if (!profile->IsOffTheRecord() &&
            profile == ProfileManager::GetDefaultProfile()) {
          DCHECK(NULL == observed_sync_service_);
          observed_sync_service_ =
              ProfileSyncServiceFactory::GetForProfile(profile);
          if (observed_sync_service_)
            observed_sync_service_->AddObserver(this);
        }
      }
      break;
    case chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED:
      DCHECK_EQ(*content::Details<const std::string>(details).ptr(),
                kAccountsPrefDeviceLocalAccounts);
      RetrieveTrustedDevicePolicies();
      break;
    default:
      NOTREACHED();
  }
}

void UserManagerImpl::OnStateChanged() {
  DCHECK(IsLoggedInAsRegularUser());
  GoogleServiceAuthError::State state =
      observed_sync_service_->GetAuthError().state();
  if (state != GoogleServiceAuthError::NONE &&
      state != GoogleServiceAuthError::CONNECTION_FAILED &&
      state != GoogleServiceAuthError::SERVICE_UNAVAILABLE &&
      state != GoogleServiceAuthError::REQUEST_CANCELED) {
    // Invalidate OAuth token to force Gaia sign-in flow. This is needed
    // because sign-out/sign-in solution is suggested to the user.
    // TODO(altimofeev): this code isn't needed after crosbug.com/25978 is
    // implemented.
    DVLOG(1) << "Invalidate OAuth token because of a sync error.";
    SaveUserOAuthStatus(
        logged_in_user_->email(),
        User::OAUTH2_TOKEN_STATUS_INVALID);
  }
}

void UserManagerImpl::OnPolicyUpdated(const std::string& account_id) {
  UpdatePublicAccountDisplayName(account_id);
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
  base::AutoLock lk(is_current_user_owner_lock_);
  is_current_user_owner_ = is_current_user_owner;
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
  return IsUserLoggedIn() && logged_in_user_->can_lock();
}

bool UserManagerImpl::IsUserLoggedIn() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return logged_in_user_;
}

bool UserManagerImpl::IsLoggedInAsRegularUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         logged_in_user_->GetType() == User::USER_TYPE_REGULAR;
}

bool UserManagerImpl::IsLoggedInAsDemoUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         logged_in_user_->GetType() == User::USER_TYPE_RETAIL_MODE;
}

bool UserManagerImpl::IsLoggedInAsPublicAccount() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
      logged_in_user_->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT;
}

bool UserManagerImpl::IsLoggedInAsGuest() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
         logged_in_user_->GetType() == User::USER_TYPE_GUEST;
}

bool UserManagerImpl::IsLoggedInAsLocallyManagedUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
      logged_in_user_->GetType() == User::USER_TYPE_LOCALLY_MANAGED;
}

bool UserManagerImpl::IsLoggedInAsKioskApp() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() &&
      logged_in_user_->GetType() == User::USER_TYPE_KIOSK_APP;
}

bool UserManagerImpl::IsLoggedInAsStub() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() && logged_in_user_->email() == kStubUser;
}

bool UserManagerImpl::IsSessionStarted() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return session_started_;
}

UserManager::MergeSessionState UserManagerImpl::GetMergeSessionState() const {
  return merge_session_state_;
}

void UserManagerImpl::SetMergeSessionState(
    UserManager::MergeSessionState state) {
  if (merge_session_state_ == state)
    return;

  merge_session_state_ = state;
  NotifyMergeSessionStateChanged();
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
  if (email == kGuestUserEMail || email == kRetailModeUserEMail ||
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

void UserManagerImpl::NotifyLocalStateChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(UserManager::Observer, observer_list_,
                    LocalStateChanged(this));
}

void UserManagerImpl::EnsureUsersLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_browser_process)
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
  ParseUserList(*prefs_regular_users, std::set<std::string>(), "",
                &regular_users, &regular_users_set);
  for (std::vector<std::string>::const_iterator it = regular_users.begin();
       it != regular_users.end(); ++it) {
    User* user = NULL;
    if (gaia::ExtractDomainName(*it) ==
            UserManager::kLocallyManagedUserDomain) {
      user = User::CreateLocallyManagedUser(*it);
    } else {
      user = User::CreateRegularUser(*it);
      user->set_oauth_token_status(LoadUserOAuthStatus(*it));
    }
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
  ParseUserList(*prefs_public_accounts, regular_users_set, "",
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
  const base::ListValue* public_accounts;
  cros_settings_->GetList(kAccountsPrefDeviceLocalAccounts, &public_accounts);

  EnsureUsersLoaded();

  bool changed = UpdateAndCleanUpPublicAccounts(*public_accounts);

  // If ephemeral users are enabled and we are on the login screen, take this
  // opportunity to clean up by removing all regular users except the owner.
  if (ephemeral_users_enabled_  && !IsUserLoggedIn()) {
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

  cros_settings_->AddSettingsObserver(kAccountsPrefDeviceLocalAccounts,
                                      this);
}

bool UserManagerImpl::AreEphemeralUsersEnabled() const {
  return ephemeral_users_enabled_ &&
      (g_browser_process->browser_policy_connector()->IsEnterpriseManaged() ||
      !owner_email_.empty());
}

const User* UserManagerImpl::FindUserInList(const std::string& email) const {
  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->email() == email)
      return *it;
  }
  return NULL;
}

void UserManagerImpl::NotifyOnLogin() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<UserManager>(this),
      content::Details<const User>(logged_in_user_));

  CrosLibrary::Get()->GetCertLibrary()->LoadKeyStore();

  // Indicate to DeviceSettingsService that the owner key may have become
  // available.
  DeviceSettingsService::Get()->SetUsername(logged_in_user_->email());
}

void UserManagerImpl::UpdateOwnership(
    DeviceSettingsService::OwnershipStatus status,
    bool is_owner) {
  VLOG(1) << "Current user " << (is_owner ? "is owner" : "is not owner");

  SetCurrentUserIsOwner(is_owner);
}

void UserManagerImpl::CheckOwnership() {
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&UserManagerImpl::UpdateOwnership,
                 base::Unretained(this)));
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

bool UserManagerImpl::UpdateAndCleanUpPublicAccounts(
    const base::ListValue& public_accounts) {
  PrefService* local_state = g_browser_process->local_state();

  // Determine the currently logged-in user's email.
  std::string logged_in_user_email;
  if (IsUserLoggedIn())
    logged_in_user_email = GetLoggedInUser()->email();

  // If there is a public account whose data is pending removal and the user is
  // not currently logged in with that account, take this opportunity to remove
  // the data.
  std::string public_account_pending_data_removal =
      local_state->GetString(kPublicAccountPendingDataRemoval);
  if (!public_account_pending_data_removal.empty() &&
      public_account_pending_data_removal != logged_in_user_email) {
    RemoveNonCryptohomeData(public_account_pending_data_removal);
    local_state->ClearPref(kPublicAccountPendingDataRemoval);
  }

  // Split the current user list public accounts and regular users.
  std::vector<std::string> old_public_accounts;
  std::set<std::string> regular_users;
  for (UserList::const_iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT)
      old_public_accounts.push_back((*it)->email());
    else
      regular_users.insert((*it)->email());
  }

  // Get the new list of public accounts from policy.
  std::vector<std::string> new_public_accounts;
  std::set<std::string> new_public_accounts_set;
  if (!ParseUserList(public_accounts, regular_users, logged_in_user_email,
                     &new_public_accounts, &new_public_accounts_set) &&
      IsLoggedInAsPublicAccount()) {
    // If the user is currently logged into a public account that has been
    // removed from the list, mark the account's data as pending removal after
    // logout.
    local_state->SetString(kPublicAccountPendingDataRemoval,
                           logged_in_user_email);
  }

  // Persist the new list of public accounts in a pref.
  ListPrefUpdate prefs_public_accounts_update(local_state, kPublicAccounts);
  scoped_ptr<base::ListValue> prefs_public_accounts(public_accounts.DeepCopy());
  prefs_public_accounts_update->Swap(prefs_public_accounts.get());

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
    if (IsLoggedInAsPublicAccount() && *it == logged_in_user_email)
      users_.insert(users_.begin(), GetLoggedInUser());
    else
      users_.insert(users_.begin(), User::CreatePublicAccountUser(*it));
    UpdatePublicAccountDisplayName(*it);
  }

  user_image_manager_->LoadUserImages(
      UserList(users_.begin(), users_.begin() + new_public_accounts.size()));

  return true;
}

void UserManagerImpl::UpdatePublicAccountDisplayName(
    const std::string& username) {
  std::string display_name;

  if (device_local_account_policy_service_) {
    policy::DeviceLocalAccountPolicyBroker* broker =
        device_local_account_policy_service_->GetBrokerForAccount(username);
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
      user_id, base::Callback<void(bool, cryptohome::MountError)>());

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

void UserManagerImpl::NotifyMergeSessionStateChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(UserManager::Observer, observer_list_,
                    MergeSessionStateChanged(merge_session_state_));
}

}  // namespace chromeos
