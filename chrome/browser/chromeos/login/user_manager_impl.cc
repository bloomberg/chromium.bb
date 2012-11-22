// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager_impl.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/rand_util.h"
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
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
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
#include "google_apis/gaia/google_service_auth_error.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// A vector pref of the users who have logged into the device.
const char kLoggedInUsers[] = "LoggedInUsers";

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

}  // namespace

// static
void UserManager::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterListPref(kLoggedInUsers, PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserOAuthTokenStatus,
                                      PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserDisplayName,
                                      PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserDisplayEmail,
                                      PrefService::UNSYNCABLE_PREF);
}

UserManagerImpl::UserManagerImpl()
    : logged_in_user_(NULL),
      session_started_(false),
      is_current_user_owner_(false),
      is_current_user_new_(false),
      is_current_user_ephemeral_(false),
      ephemeral_users_enabled_(false),
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
  for (size_t i = 0; i < users_.size(); ++i)
    delete users_[i];
  users_.clear();
  if (is_current_user_ephemeral_)
    delete logged_in_user_;
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
    return;
  }

  if (email == kRetailModeUserEMail) {
    RetailModeUserLoggedIn();
    return;
  }

  if (IsEphemeralUser(email)) {
    EphemeralUserLoggedIn(email);
    return;
  }

  EnsureUsersLoaded();

  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_users_update(prefs, kLoggedInUsers);
  prefs_users_update->Clear();

  // Make sure this user is first.
  prefs_users_update->Append(new base::StringValue(email));
  UserList::iterator logged_in_user = users_.end();
  for (UserList::iterator it = users_.begin(); it != users_.end(); ++it) {
    std::string user_email = (*it)->email();
    // Skip the most recent user.
    if (email != user_email)
      prefs_users_update->Append(new base::StringValue(user_email));
    else
      logged_in_user = it;
  }

  if (logged_in_user == users_.end()) {
    is_current_user_new_ = true;
    logged_in_user_ = User::CreateRegularUser(email);
    logged_in_user_->set_oauth_token_status(LoadUserOAuthStatus(email));
  } else {
    logged_in_user_ = *logged_in_user;
    users_.erase(logged_in_user);
  }
  // This user must be in the front of the user list.
  users_.insert(users_.begin(), logged_in_user_);

  if (is_current_user_new_) {
    SaveUserDisplayName(logged_in_user_->email(),
                        UTF8ToUTF16(logged_in_user_->GetAccountName(true)));
    WallpaperManager::Get()->SetInitialUserWallpaper(email, true);
  }

  user_image_manager_->UserLoggedIn(email, is_current_user_new_);

  if (!browser_restart) {
    // For GAIA login flow, logged in user wallpaper may not be loaded.
    WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();
  }

  // Make sure we persist new user data to Local State.
  prefs->CommitPendingWrite();

  NotifyOnLogin();
}

void UserManagerImpl::RetailModeUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  is_current_user_ephemeral_ = true;
  logged_in_user_ = User::CreateRetailModeUser();
  user_image_manager_->UserLoggedIn(kRetailModeUserEMail,
                                    /* user_is_new= */ true);
  WallpaperManager::Get()->SetInitialUserWallpaper(kRetailModeUserEMail, false);
  NotifyOnLogin();
}

void UserManagerImpl::GuestUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_ephemeral_ = true;
  WallpaperManager::Get()->SetInitialUserWallpaper(kGuestUserEMail, false);
  logged_in_user_ = User::CreateGuestUser();
  logged_in_user_->SetStubImage(User::kInvalidImageIndex, false);
  NotifyOnLogin();
}

void UserManagerImpl::EphemeralUserLoggedIn(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  is_current_user_ephemeral_ = true;
  logged_in_user_ = User::CreateRegularUser(email);
  user_image_manager_->UserLoggedIn(email, /* user_is_new= */ true);
  WallpaperManager::Get()->SetInitialUserWallpaper(email, false);
  NotifyOnLogin();
}

void UserManagerImpl::SessionStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  session_started_ = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
  if (is_current_user_new_) {
    // Make sure we persist new user data to Local State.
    g_browser_process->local_state()->CommitPendingWrite();
  }
}

void UserManagerImpl::RemoveUser(const std::string& email,
                                 RemoveUserDelegate* delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!IsKnownUser(email))
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
  RemoveUserFromListInternal(email);
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

  // Do not update local store if the user is ephemeral.
  if (IsEphemeralUser(username))
    return;

  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate oauth_status_update(local_state, kUserOAuthTokenStatus);
  oauth_status_update->SetWithoutPathExpansion(username,
      new base::FundamentalValue(static_cast<int>(oauth_token_status)));
}

User::OAuthTokenStatus UserManagerImpl::LoadUserOAuthStatus(
    const std::string& username) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSkipOAuthLogin)) {
    // Use OAUTH_TOKEN_STATUS_VALID flag if kSkipOAuthLogin is present.
    return User::OAUTH_TOKEN_STATUS_VALID;
  } else {
    PrefService* local_state = g_browser_process->local_state();
    const DictionaryValue* prefs_oauth_status =
        local_state->GetDictionary(kUserOAuthTokenStatus);

    int oauth_token_status = User::OAUTH_TOKEN_STATUS_UNKNOWN;
    if (prefs_oauth_status &&
        prefs_oauth_status->GetIntegerWithoutPathExpansion(username,
            &oauth_token_status)) {
      return static_cast<User::OAuthTokenStatus>(oauth_token_status);
    }
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

  // Do not update local store if the user is ephemeral.
  if (IsEphemeralUser(username))
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

  // Do not update local store if the user is ephemeral.
  if (IsEphemeralUser(username))
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
      CheckOwnership();
      RetrieveTrustedDevicePolicies();
      break;
    case chrome::NOTIFICATION_PROFILE_ADDED:
      if (IsUserLoggedIn() && !IsLoggedInAsGuest()) {
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
    default:
      NOTREACHED();
  }
}

void UserManagerImpl::OnStateChanged() {
  DCHECK(IsUserLoggedIn() && !IsLoggedInAsGuest());
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
    SaveUserOAuthStatus(logged_in_user_->email(),
                        User::OAUTH_TOKEN_STATUS_INVALID);
  }
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

bool UserManagerImpl::IsCurrentUserEphemeral() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return is_current_user_ephemeral_;
}

bool UserManagerImpl::CanCurrentUserLock() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() && logged_in_user_->can_lock();
}

bool UserManagerImpl::IsUserLoggedIn() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return logged_in_user_;
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

bool UserManagerImpl::IsLoggedInAsStub() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() && logged_in_user_->email() == kStubUser;
}

bool UserManagerImpl::IsSessionStarted() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return session_started_;
}

bool UserManagerImpl::IsEphemeralUser(const std::string& email) const {
  // The guest and stub user always are ephemeral.
  if (email == kGuestUserEMail || email == kStubUser)
    return true;

  // The currently logged-in user is ephemeral iff logged in as ephemeral.
  if (logged_in_user_ && (email == logged_in_user_->email()))
    return is_current_user_ephemeral_;

  // The owner and any users found in the persistent list are never ephemeral.
  if (email == owner_email_  || FindUserInList(email))
    return false;

  // Any other user is ephemeral when:
  // a) Going through the regular login flow and ephemeral users are enabled.
  //    - or -
  // b) The browser is restarting after a crash.
  return AreEphemeralUsersEnabled() ||
         (base::chromeos::IsRunningOnChromeOS() &&
          !CommandLine::ForCurrentProcess()->
              HasSwitch(switches::kLoginManager));
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
  if (!users_.empty())
    return;
  if (!g_browser_process)
    return;

  PrefService* local_state = g_browser_process->local_state();
  const ListValue* prefs_users =
      local_state->GetList(kLoggedInUsers);
  const DictionaryValue* prefs_display_names =
      local_state->GetDictionary(kUserDisplayName);
  const DictionaryValue* prefs_display_emails =
      local_state->GetDictionary(kUserDisplayEmail);

  if (!prefs_users)
    return;

  for (ListValue::const_iterator it = prefs_users->begin();
       it != prefs_users->end(); ++it) {
    std::string email;
    if ((*it)->GetAsString(&email)) {
      User* user = User::CreateRegularUser(email);
      user->set_oauth_token_status(LoadUserOAuthStatus(email));
      users_.push_back(user);

      string16 display_name;
      if (prefs_display_names &&
          prefs_display_names->GetStringWithoutPathExpansion(
              email, &display_name)) {
        user->set_display_name(display_name);
      }

      std::string display_email;
      if (prefs_display_emails &&
          prefs_display_emails->GetStringWithoutPathExpansion(
              email, &display_email)) {
        user->set_display_email(display_email);
      }
    }
  }

  user_image_manager_->LoadUserImages(users_);
}

void UserManagerImpl::RetrieveTrustedDevicePolicies() {
  ephemeral_users_enabled_ = false;
  owner_email_ = "";

  CrosSettings* cros_settings = CrosSettings::Get();
  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED != cros_settings->PrepareTrustedValues(
      base::Bind(&UserManagerImpl::RetrieveTrustedDevicePolicies,
                 base::Unretained(this)))) {
    return;
  }

  cros_settings->GetBoolean(kAccountsPrefEphemeralUsersEnabled,
                            &ephemeral_users_enabled_);
  cros_settings->GetString(kDeviceOwner, &owner_email_);

  // If ephemeral users are enabled and we are on the login screen, take this
  // opportunity to clean up by removing all users except the owner.
  if (ephemeral_users_enabled_  && !IsUserLoggedIn()) {
    scoped_ptr<base::ListValue> users(
        g_browser_process->local_state()->GetList(kLoggedInUsers)->DeepCopy());

    bool changed = false;
    for (base::ListValue::const_iterator user = users->begin();
        user != users->end(); ++user) {
      std::string user_email;
      (*user)->GetAsString(&user_email);
      if (user_email != owner_email_) {
        RemoveUserFromListInternal(user_email);
        changed = true;
      }
    }

    if (changed) {
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_POLICY_USER_LIST_CHANGED,
          content::Source<UserManager>(this),
          content::NotificationService::NoDetails());
    }
  }
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

void UserManagerImpl::RemoveUserFromListInternal(const std::string& email) {
  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_users_update(prefs, kLoggedInUsers);
  prefs_users_update->Clear();

  UserList::iterator user_to_remove = users_.end();
  for (UserList::iterator it = users_.begin(); it != users_.end(); ++it) {
    std::string user_email = (*it)->email();
    // Skip user that we would like to delete.
    if (email != user_email)
      prefs_users_update->Append(new base::StringValue(user_email));
    else
      user_to_remove = it;
  }

  WallpaperManager::Get()->RemoveUserWallpaperInfo(email);

  DictionaryPrefUpdate prefs_oauth_update(prefs, kUserOAuthTokenStatus);
  int oauth_status;
  prefs_oauth_update->GetIntegerWithoutPathExpansion(email, &oauth_status);
  prefs_oauth_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_display_name_update(prefs, kUserDisplayName);
  prefs_display_name_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_display_email_update(prefs, kUserDisplayEmail);
  prefs_display_email_update->RemoveWithoutPathExpansion(email, NULL);

  if (user_to_remove != users_.end()) {
    delete *user_to_remove;
    users_.erase(user_to_remove);
  }
}

}  // namespace chromeos
