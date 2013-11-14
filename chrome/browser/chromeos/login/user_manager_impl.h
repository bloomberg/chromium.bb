// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/multi_profile_user_controller_delegate.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;
class ProfileSyncService;

namespace policy {
struct DeviceLocalAccount;
}

namespace chromeos {

class MultiProfileFirstRunNotification;
class MultiProfileUserController;
class RemoveUserDelegate;
class SupervisedUserManagerImpl;
class SessionLengthLimiter;

struct UpdateUserAccountDataCallbackData;

// Implementation of the UserManager.
class UserManagerImpl
    : public UserManager,
      public LoginUtils::Delegate,
      public content::NotificationObserver,
      public policy::DeviceLocalAccountPolicyService::Observer,
      public MultiProfileUserControllerDelegate {
 public:
  virtual ~UserManagerImpl();

  // UserManager implementation:
  virtual void Shutdown() OVERRIDE;
  virtual UserImageManager* GetUserImageManager() OVERRIDE;
  virtual SupervisedUserManager* GetSupervisedUserManager() OVERRIDE;
  virtual const UserList& GetUsers() const OVERRIDE;
  virtual UserList GetUsersAdmittedForMultiProfile() const OVERRIDE;
  virtual const UserList& GetLoggedInUsers() const OVERRIDE;
  virtual const UserList& GetLRULoggedInUsers() OVERRIDE;
  virtual UserList GetUnlockUsers() const OVERRIDE;
  virtual const std::string& GetOwnerEmail() OVERRIDE;
  virtual void UserLoggedIn(const std::string& user_id,
                            const std::string& user_id_hash,
                            bool browser_restart) OVERRIDE;
  virtual void SwitchActiveUser(const std::string& user_id) OVERRIDE;
  virtual void RestoreActiveSessions() OVERRIDE;
  virtual void SessionStarted() OVERRIDE;
  virtual void RemoveUser(const std::string& user_id,
                          RemoveUserDelegate* delegate) OVERRIDE;
  virtual void RemoveUserFromList(const std::string& user_id) OVERRIDE;
  virtual bool IsKnownUser(const std::string& user_id) const OVERRIDE;
  virtual const User* FindUser(const std::string& user_id) const OVERRIDE;
  virtual const User* GetLoggedInUser() const OVERRIDE;
  virtual User* GetLoggedInUser() OVERRIDE;
  virtual const User* GetActiveUser() const OVERRIDE;
  virtual User* GetActiveUser() OVERRIDE;
  virtual const User* GetPrimaryUser() const OVERRIDE;
  virtual User* GetUserByProfile(Profile* profile) const OVERRIDE;
  virtual void SaveUserOAuthStatus(
      const std::string& user_id,
      User::OAuthTokenStatus oauth_token_status) OVERRIDE;
  virtual void SaveUserDisplayName(const std::string& user_id,
                                   const string16& display_name) OVERRIDE;
  virtual void UpdateUserAccountData(const std::string& user_id,
                                     const string16& display_name,
                                     const std::string& locale) OVERRIDE;
  virtual string16 GetUserDisplayName(
      const std::string& user_id) const OVERRIDE;
  virtual void SaveUserDisplayEmail(const std::string& user_id,
                                    const std::string& display_email) OVERRIDE;
  virtual std::string GetUserDisplayEmail(
      const std::string& user_id) const OVERRIDE;
  virtual bool IsCurrentUserOwner() const OVERRIDE;
  virtual bool IsCurrentUserNew() const OVERRIDE;
  virtual bool IsCurrentUserNonCryptohomeDataEphemeral() const OVERRIDE;
  virtual bool CanCurrentUserLock() const OVERRIDE;
  virtual bool IsUserLoggedIn() const OVERRIDE;
  virtual bool IsLoggedInAsRegularUser() const OVERRIDE;
  virtual bool IsLoggedInAsDemoUser() const OVERRIDE;
  virtual bool IsLoggedInAsPublicAccount() const OVERRIDE;
  virtual bool IsLoggedInAsGuest() const OVERRIDE;
  virtual bool IsLoggedInAsLocallyManagedUser() const OVERRIDE;
  virtual bool IsLoggedInAsKioskApp() const OVERRIDE;
  virtual bool IsLoggedInAsStub() const OVERRIDE;
  virtual bool IsSessionStarted() const OVERRIDE;
  virtual bool UserSessionsRestored() const OVERRIDE;
  virtual bool HasBrowserRestarted() const OVERRIDE;
  virtual bool IsUserNonCryptohomeDataEphemeral(
      const std::string& user_id) const OVERRIDE;
  virtual void AddObserver(UserManager::Observer* obs) OVERRIDE;
  virtual void RemoveObserver(UserManager::Observer* obs) OVERRIDE;
  virtual void AddSessionStateObserver(
      UserManager::UserSessionStateObserver* obs) OVERRIDE;
  virtual void RemoveSessionStateObserver(
      UserManager::UserSessionStateObserver* obs) OVERRIDE;
  virtual void NotifyLocalStateChanged() OVERRIDE;

  virtual UserFlow* GetCurrentUserFlow() const OVERRIDE;
  virtual UserFlow* GetUserFlow(const std::string& user_id) const OVERRIDE;
  virtual void SetUserFlow(const std::string& user_id, UserFlow* flow) OVERRIDE;
  virtual void ResetUserFlow(const std::string& user_id) OVERRIDE;
  virtual bool GetAppModeChromeClientOAuthInfo(
      std::string* chrome_client_id,
      std::string* chrome_client_secret) OVERRIDE;
  virtual void SetAppModeChromeClientOAuthInfo(
      const std::string& chrome_client_id,
      const std::string& chrome_client_secret) OVERRIDE;
  virtual bool AreLocallyManagedUsersAllowed() const OVERRIDE;
  virtual base::FilePath GetUserProfileDir(
      const std::string& user_id) const OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // policy::DeviceLocalAccountPolicyService::Observer implementation.
  virtual void OnPolicyUpdated(const std::string& user_id) OVERRIDE;
  virtual void OnDeviceLocalAccountsChanged() OVERRIDE;

  virtual bool RespectLocalePreference(
      Profile* profile,
      const User* user,
      scoped_ptr<locale_util::SwitchLanguageCallback> callback) const OVERRIDE;

 private:
  friend class SupervisedUserManagerImpl;
  friend class UserManager;
  friend class WallpaperManager;
  friend class UserManagerTest;
  friend class WallpaperManagerTest;

  UserManagerImpl();

  // LoginUtils::Delegate implementation:
  // Used when restoring user sessions after crash.
  virtual void OnProfilePrepared(Profile* profile) OVERRIDE;

  // Loads |users_| from Local State if the list has not been loaded yet.
  // Subsequent calls have no effect. Must be called on the UI thread.
  void EnsureUsersLoaded();

  // Retrieves trusted device policies and removes users from the persistent
  // list if ephemeral users are enabled. Schedules a callback to itself if
  // trusted device policies are not yet available.
  void RetrieveTrustedDevicePolicies();

  // Returns true if trusted device policies have successfully been retrieved
  // and ephemeral users are enabled.
  bool AreEphemeralUsersEnabled() const;

  // Returns a list of users who have logged into this device previously.
  // Same as GetUsers but used if you need to modify User from that list.
  UserList& GetUsersAndModify();

  // Returns the user with the given email address if found in the persistent
  // list or currently logged in as ephemeral. Returns |NULL| otherwise.
  // Same as FindUser but returns non-const pointer to User object.
  User* FindUserAndModify(const std::string& user_id);

  // Returns the user with the given email address if found in the persistent
  // list. Returns |NULL| otherwise.
  const User* FindUserInList(const std::string& user_id) const;

  // Returns |true| if user with the given id is found in the persistent list.
  // Returns |false| otherwise. Does not trigger user loading.
  const bool UserExistsInList(const std::string& user_id) const;

  // Same as FindUserInList but returns non-const pointer to User object.
  User* FindUserInListAndModify(const std::string& user_id);

  // Indicates that a user just logged in as guest.
  void GuestUserLoggedIn();

  // Indicates that a regular user just logged in.
  void RegularUserLoggedIn(const std::string& user_id);

  // Indicates that a regular user just logged in as ephemeral.
  void RegularUserLoggedInAsEphemeral(const std::string& user_id);

  // Indicates that a locally managed user just logged in.
  void LocallyManagedUserLoggedIn(const std::string& user_id);

  // Indicates that a user just logged into a public session.
  void PublicAccountUserLoggedIn(User* user);

  // Indicates that a kiosk app robot just logged in.
  void KioskAppLoggedIn(const std::string& app_id);

  // Indicates that a user just logged into a retail mode session.
  void RetailModeUserLoggedIn();

  // Notifies that user has logged in.
  // Sends NOTIFICATION_LOGIN_USER_CHANGED notification.
  void NotifyOnLogin();

  // Reads user's oauth token status from local state preferences.
  User::OAuthTokenStatus LoadUserOAuthStatus(const std::string& user_id) const;

  void SetCurrentUserIsOwner(bool is_current_user_owner);

  // Updates current user ownership on UI thread.
  void UpdateOwnership();

  // Removes data stored or cached outside the user's cryptohome (wallpaper,
  // avatar, OAuth token status, display name, display email).
  void RemoveNonCryptohomeData(const std::string& user_id);

  // Removes a regular or locally managed user from the user list.
  // Returns the user if found or NULL otherwise.
  // Also removes the user from the persistent user list.
  User* RemoveRegularOrLocallyManagedUserFromList(const std::string& user_id);

  // If data for a public account is marked as pending removal and the user is
  // no longer logged into that account, removes the data.
  void CleanUpPublicAccountNonCryptohomeDataPendingRemoval();

  // Removes data belonging to public accounts that are no longer found on the
  // user list. If the user is currently logged into one of these accounts, the
  // data for that account is not removed immediately but marked as pending
  // removal after logout.
  void CleanUpPublicAccountNonCryptohomeData(
      const std::vector<std::string>& old_public_accounts);

  // Replaces the list of public accounts with those found in
  // |device_local_accounts|. Ensures that data belonging to accounts no longer
  // on the list is removed. Returns |true| if the list has changed.
  // Public accounts are defined by policy. This method is called whenever an
  // updated list of public accounts is received from policy.
  bool UpdateAndCleanUpPublicAccounts(
      const std::vector<policy::DeviceLocalAccount>& device_local_accounts);

  // Updates the display name for public account |username| from policy settings
  // associated with that username.
  void UpdatePublicAccountDisplayName(const std::string& user_id);

  // Notifies the UI about a change to the user list.
  void NotifyUserListChanged();

  // Notifies observers that merge session state had changed.
  void NotifyMergeSessionStateChanged();

  // Notifies observers that active user has changed.
  void NotifyActiveUserChanged(const User* active_user);

  // Notifies observers that another user was added to the session.
  void NotifyUserAddedToSession(const User* added_user);

  // Notifies observers that active user_id hash has changed.
  void NotifyActiveUserHashChanged(const std::string& hash);

  // Notifies observers that user pending sessions restore has finished.
  void NotifyPendingUserSessionsRestoreFinished();

  // Lazily creates default user flow.
  UserFlow* GetDefaultUserFlow() const;

  // Update the global LoginState.
  void UpdateLoginState();

  // Insert |user| at the front of the LRU user list..
  void SetLRUUser(User* user);

  // Adds |user| to users list, and adds it to front of LRU list. It is assumed
  // that there is no user with same id.
  void AddUserRecord(User* user);

  // Callback to process RetrieveActiveSessions() request results.
  void OnRestoreActiveSessions(
      const SessionManagerClient::ActiveSessionsMap& sessions,
      bool success);

  // Called by OnRestoreActiveSessions() when there're user sessions in
  // |pending_user_sessions_| that has to be restored one by one.
  // Also called after first user session from that list is restored and so on.
  // Process continues till |pending_user_sessions_| map is not empty.
  void RestorePendingUserSessions();

  // Sends metrics in response to a regular user logging in.
  void SendRegularUserLoginMetrics(const std::string& user_id);

  // UpdateUserAccountData() + SaveUserDisplayName() .
  void UpdateUserAccountDataImpl(const std::string& user_id,
                                 const string16& display_name,
                                 const std::string* locale);

  // Account locale needs to be translated to device locale.
  // This might be called as callback after FILE thread translates locale.
  void UpdateUserAccountDataImplCallback(
      const std::string& user_id,
      const string16& display_name,
      const std::string* resolved_account_locale);

  // Decorator to the previous function.
  void UpdateUserAccountDataImplCallbackDecorator(
      const scoped_ptr<UpdateUserAccountDataCallbackData>& data);

  Profile* GetProfileByUser(const User* user) const;

  // Implementation for RemoveUser method. This is an asynchronous part of the
  // method, that verifies that owner will not get deleted, and calls
  // |RemoveNonOwnerUserInternal|.
  void RemoveUserInternal(const std::string& user_email,
                          RemoveUserDelegate* delegate);

  // Implementation for RemoveUser method. It is synchronous. It is called from
  // RemoveUserInternal after owner check.
  void RemoveNonOwnerUserInternal(const std::string& user_email,
                                  RemoveUserDelegate* delegate);

  // MultiProfileUserControllerDelegate implementation:
  virtual void OnUserNotAllowed() OVERRIDE;

  // Interface to the signed settings store.
  CrosSettings* cros_settings_;

  // Interface to device-local account definitions and associated policy.
  policy::DeviceLocalAccountPolicyService* device_local_account_policy_service_;

  // True if users have been loaded from prefs already.
  bool users_loaded_;

  // List of all known users. User instances are owned by |this|. Regular users
  // are removed by |RemoveUserFromList|, public accounts by
  // |UpdateAndCleanUpPublicAccounts|.
  UserList users_;

  // List of all users that are logged in current session. These point to User
  // instances in |users_|. Only one of them could be marked as active.
  UserList logged_in_users_;

  // A list of all users that are logged in the current session. In contrast to
  // |logged_in_users|, the order of this list is least recently used so that
  // the active user should always be the first one in the list.
  UserList lru_logged_in_users_;

  // The list which gets reported when the |lru_logged_in_users_| list is empty.
  UserList temp_single_logged_in_users_;

  // The logged-in user that is currently active in current session.
  // NULL until a user has logged in, then points to one
  // of the User instances in |users_|, the |guest_user_| instance or an
  // ephemeral user instance.
  User* active_user_;

  // The primary user of the current session. It is recorded for the first
  // signed-in user and does not change thereafter.
  User* primary_user_;

  // True if SessionStarted() has been called.
  bool session_started_;

  // True is user sessions has been restored after crash.
  // On a normal boot then login into user sessions this will be false.
  bool user_sessions_restored_;

  // Cached flag of whether currently logged-in user is owner or not.
  // May be accessed on different threads, requires locking.
  bool is_current_user_owner_;
  mutable base::Lock is_current_user_owner_lock_;

  // Cached flag of whether the currently logged-in user existed before this
  // login.
  bool is_current_user_new_;

  // Cached flag of whether the currently logged-in user is a regular user who
  // logged in as ephemeral. Storage of persistent information is avoided for
  // such users by not adding them to the persistent user list, not downloading
  // their custom avatars and mounting their cryptohomes using tmpfs. Defaults
  // to |false|.
  bool is_current_user_ephemeral_regular_user_;

  // Cached flag indicating whether the ephemeral user policy is enabled.
  // Defaults to |false| if the value has not been read from trusted device
  // policy yet.
  bool ephemeral_users_enabled_;

  // Cached name of device owner. Defaults to empty string if the value has not
  // been read from trusted device policy yet.
  std::string owner_email_;

  // Chrome oauth client id and secret - override values for kiosk mode.
  std::string chrome_client_id_;
  std::string chrome_client_secret_;

  content::NotificationRegistrar registrar_;

  ObserverList<UserManager::Observer> observer_list_;

  // TODO(nkostylev): Merge with session state refactoring CL.
  ObserverList<UserManager::UserSessionStateObserver>
      session_state_observer_list_;

  // User avatar manager.
  scoped_ptr<UserImageManagerImpl> user_image_manager_;

  // Supervised user manager.
  scoped_ptr<SupervisedUserManagerImpl> supervised_user_manager_;

  // Session length limiter.
  scoped_ptr<SessionLengthLimiter> session_length_limiter_;

  typedef std::map<std::string, UserFlow*> FlowMap;

  // Lazy-initialized default flow.
  mutable scoped_ptr<UserFlow> default_flow_;

  // Specific flows by user e-mail. Keys should be canonicalized before
  // access.
  FlowMap specific_flows_;

  // User sessions that have to be restored after browser crash.
  // [user_id] > [user_id_hash]
  SessionManagerClient::ActiveSessionsMap pending_user_sessions_;

  // Time at which this object was created.
  base::TimeTicks manager_creation_time_;

  scoped_ptr<CrosSettings::ObserverSubscription>
      local_accounts_subscription_;
  scoped_ptr<CrosSettings::ObserverSubscription>
      supervised_users_subscription_;

  scoped_ptr<MultiProfileUserController> multi_profile_user_controller_;
  scoped_ptr<MultiProfileFirstRunNotification>
      multi_profile_first_run_notification_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_IMPL_H_
