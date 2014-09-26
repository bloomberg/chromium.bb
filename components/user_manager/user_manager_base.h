// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_MANAGER_BASE_H_
#define COMPONENTS_USER_MANAGER_USER_MANAGER_BASE_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_export.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class ListValue;
class TaskRunner;
}

namespace user_manager {

class RemoveUserDelegate;

// Base implementation of the UserManager interface.
class USER_MANAGER_EXPORT UserManagerBase : public UserManager {
 public:
  // Creates UserManagerBase with |task_runner| for UI thread and
  // |blocking_task_runner| for SequencedWorkerPool.
  UserManagerBase(scoped_refptr<base::TaskRunner> task_runner,
                  scoped_refptr<base::TaskRunner> blocking_task_runner);
  virtual ~UserManagerBase();

  // Registers UserManagerBase preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // UserManager implementation:
  virtual void Shutdown() OVERRIDE;
  virtual const UserList& GetUsers() const OVERRIDE;
  virtual const UserList& GetLoggedInUsers() const OVERRIDE;
  virtual const UserList& GetLRULoggedInUsers() const OVERRIDE;
  virtual const std::string& GetOwnerEmail() const OVERRIDE;
  virtual void UserLoggedIn(const std::string& user_id,
                            const std::string& user_id_hash,
                            bool browser_restart) OVERRIDE;
  virtual void SwitchActiveUser(const std::string& user_id) OVERRIDE;
  virtual void SwitchToLastActiveUser() OVERRIDE;
  virtual void SessionStarted() OVERRIDE;
  virtual void RemoveUser(const std::string& user_id,
                          RemoveUserDelegate* delegate) OVERRIDE;
  virtual void RemoveUserFromList(const std::string& user_id) OVERRIDE;
  virtual bool IsKnownUser(const std::string& user_id) const OVERRIDE;
  virtual const User* FindUser(const std::string& user_id) const OVERRIDE;
  virtual User* FindUserAndModify(const std::string& user_id) OVERRIDE;
  virtual const User* GetLoggedInUser() const OVERRIDE;
  virtual User* GetLoggedInUser() OVERRIDE;
  virtual const User* GetActiveUser() const OVERRIDE;
  virtual User* GetActiveUser() OVERRIDE;
  virtual const User* GetPrimaryUser() const OVERRIDE;
  virtual void SaveUserOAuthStatus(
      const std::string& user_id,
      User::OAuthTokenStatus oauth_token_status) OVERRIDE;
  virtual void SaveForceOnlineSignin(const std::string& user_id,
                                     bool force_online_signin) OVERRIDE;
  virtual void SaveUserDisplayName(const std::string& user_id,
                                   const base::string16& display_name) OVERRIDE;
  virtual base::string16 GetUserDisplayName(
      const std::string& user_id) const OVERRIDE;
  virtual void SaveUserDisplayEmail(const std::string& user_id,
                                    const std::string& display_email) OVERRIDE;
  virtual std::string GetUserDisplayEmail(
      const std::string& user_id) const OVERRIDE;
  virtual void UpdateUserAccountData(
      const std::string& user_id,
      const UserAccountData& account_data) OVERRIDE;
  virtual bool IsCurrentUserOwner() const OVERRIDE;
  virtual bool IsCurrentUserNew() const OVERRIDE;
  virtual bool IsCurrentUserNonCryptohomeDataEphemeral() const OVERRIDE;
  virtual bool CanCurrentUserLock() const OVERRIDE;
  virtual bool IsUserLoggedIn() const OVERRIDE;
  virtual bool IsLoggedInAsRegularUser() const OVERRIDE;
  virtual bool IsLoggedInAsDemoUser() const OVERRIDE;
  virtual bool IsLoggedInAsPublicAccount() const OVERRIDE;
  virtual bool IsLoggedInAsGuest() const OVERRIDE;
  virtual bool IsLoggedInAsSupervisedUser() const OVERRIDE;
  virtual bool IsLoggedInAsKioskApp() const OVERRIDE;
  virtual bool IsLoggedInAsStub() const OVERRIDE;
  virtual bool IsSessionStarted() const OVERRIDE;
  virtual bool IsUserNonCryptohomeDataEphemeral(
      const std::string& user_id) const OVERRIDE;
  virtual void AddObserver(UserManager::Observer* obs) OVERRIDE;
  virtual void RemoveObserver(UserManager::Observer* obs) OVERRIDE;
  virtual void AddSessionStateObserver(
      UserManager::UserSessionStateObserver* obs) OVERRIDE;
  virtual void RemoveSessionStateObserver(
      UserManager::UserSessionStateObserver* obs) OVERRIDE;
  virtual void NotifyLocalStateChanged() OVERRIDE;
  virtual void ForceUpdateState() OVERRIDE;

  // Helper function that copies users from |users_list| to |users_vector| and
  // |users_set|. Duplicates and users already present in |existing_users| are
  // skipped.
  static void ParseUserList(const base::ListValue& users_list,
                            const std::set<std::string>& existing_users,
                            std::vector<std::string>* users_vector,
                            std::set<std::string>* users_set);

 protected:
  // Adds |user| to users list, and adds it to front of LRU list. It is assumed
  // that there is no user with same id.
  virtual void AddUserRecord(User* user);

  // Returns true if trusted device policies have successfully been retrieved
  // and ephemeral users are enabled.
  virtual bool AreEphemeralUsersEnabled() const = 0;

  // Returns true if user may be removed.
  virtual bool CanUserBeRemoved(const User* user) const;

  // A wrapper around C++ delete operator. Deletes |user|, and when |user|
  // equals to active_user_, active_user_ is reset to NULL.
  virtual void DeleteUser(User* user);

  // Returns the locale used by the application.
  virtual const std::string& GetApplicationLocale() const = 0;

  // Returns "Local State" PrefService instance.
  virtual PrefService* GetLocalState() const = 0;

  // Loads |users_| from Local State if the list has not been loaded yet.
  // Subsequent calls have no effect. Must be called on the UI thread.
  void EnsureUsersLoaded();

  // Handle OAuth token |status| change for |user_id|.
  virtual void HandleUserOAuthTokenStatusChange(
      const std::string& user_id,
      User::OAuthTokenStatus status) const = 0;

  // Returns true if device is enterprise managed.
  virtual bool IsEnterpriseManaged() const = 0;

  // Helper function that copies users from |users_list| to |users_vector| and
  // |users_set|. Duplicates and users already present in |existing_users| are
  // skipped.
  // Loads public accounts from the Local state and fills in
  // |public_sessions_set|.
  virtual void LoadPublicAccounts(
      std::set<std::string>* public_sessions_set) = 0;

  // Notifies that user has logged in.
  virtual void NotifyOnLogin();

  // Notifies observers that another user was added to the session.
  // If |user_switch_pending| is true this means that user has not been fully
  // initialized yet like waiting for profile to be loaded.
  virtual void NotifyUserAddedToSession(const User* added_user,
                                        bool user_switch_pending);

  // Performs any additional actions before user list is loaded.
  virtual void PerformPreUserListLoadingActions() = 0;

  // Performs any additional actions after user list is loaded.
  virtual void PerformPostUserListLoadingActions() = 0;

  // Performs any additional actions after UserLoggedIn() execution has been
  // completed.
  // |browser_restart| is true when reloading Chrome after crash to distinguish
  // from normal sign in flow.
  virtual void PerformPostUserLoggedInActions(bool browser_restart) = 0;

  // Implementation for RemoveUser method. It is synchronous. It is called from
  // RemoveUserInternal after owner check.
  virtual void RemoveNonOwnerUserInternal(const std::string& user_email,
                                          RemoveUserDelegate* delegate);

  // Removes a regular or supervised user from the user list.
  // Returns the user if found or NULL otherwise.
  // Also removes the user from the persistent user list.
  User* RemoveRegularOrSupervisedUserFromList(const std::string& user_id);

  // Implementation for RemoveUser method. This is an asynchronous part of the
  // method, that verifies that owner will not get deleted, and calls
  // |RemoveNonOwnerUserInternal|.
  virtual void RemoveUserInternal(const std::string& user_email,
                                  RemoveUserDelegate* delegate);

  // Removes data stored or cached outside the user's cryptohome (wallpaper,
  // avatar, OAuth token status, display name, display email).
  virtual void RemoveNonCryptohomeData(const std::string& user_id);

  // Check for a particular user type.

  // Returns true if |user_id| represents demo app.
  virtual bool IsDemoApp(const std::string& user_id) const = 0;

  // Returns true if |user_id| represents kiosk app.
  virtual bool IsKioskApp(const std::string& user_id) const = 0;

  // Returns true if |user_id| represents public account that has been marked
  // for deletion.
  virtual bool IsPublicAccountMarkedForRemoval(
      const std::string& user_id) const = 0;

  // These methods are called when corresponding user type has signed in.

  // Indicates that the demo account has just logged in.
  virtual void DemoAccountLoggedIn() = 0;

  // Indicates that a user just logged in as guest.
  virtual void GuestUserLoggedIn();

  // Indicates that a kiosk app robot just logged in.
  virtual void KioskAppLoggedIn(const std::string& app_id) = 0;

  // Indicates that a user just logged into a public session.
  virtual void PublicAccountUserLoggedIn(User* user) = 0;

  // Indicates that a regular user just logged in.
  virtual void RegularUserLoggedIn(const std::string& user_id);

  // Indicates that a regular user just logged in as ephemeral.
  virtual void RegularUserLoggedInAsEphemeral(const std::string& user_id);

  // Indicates that a user just logged into a retail mode session.
  virtual void RetailModeUserLoggedIn() = 0;

  // Indicates that a supervised user just logged in.
  virtual void SupervisedUserLoggedIn(const std::string& user_id) = 0;

  // Getters/setters for private members.

  virtual void SetCurrentUserIsOwner(bool is_current_user_owner);

  virtual bool GetEphemeralUsersEnabled() const;
  virtual void SetEphemeralUsersEnabled(bool enabled);

  virtual void SetIsCurrentUserNew(bool is_new);

  virtual void SetOwnerEmail(std::string owner_user_id);

  virtual const std::string& GetPendingUserSwitchID() const;
  virtual void SetPendingUserSwitchID(std::string user_id);

  // The logged-in user that is currently active in current session.
  // NULL until a user has logged in, then points to one
  // of the User instances in |users_|, the |guest_user_| instance or an
  // ephemeral user instance.
  User* active_user_;

  // The primary user of the current session. It is recorded for the first
  // signed-in user and does not change thereafter.
  User* primary_user_;

  // List of all known users. User instances are owned by |this|. Regular users
  // are removed by |RemoveUserFromList|, public accounts by
  // |UpdateAndCleanUpPublicAccounts|.
  UserList users_;

 private:
  // Stages of loading user list from preferences. Some methods can have
  // different behavior depending on stage.
  enum UserLoadStage { STAGE_NOT_LOADED = 0, STAGE_LOADING, STAGE_LOADED };

  // Returns a list of users who have logged into this device previously.
  // Same as GetUsers but used if you need to modify User from that list.
  UserList& GetUsersAndModify();

  // Returns the user with the given email address if found in the persistent
  // list. Returns |NULL| otherwise.
  const User* FindUserInList(const std::string& user_id) const;

  // Returns |true| if user with the given id is found in the persistent list.
  // Returns |false| otherwise. Does not trigger user loading.
  const bool UserExistsInList(const std::string& user_id) const;

  // Same as FindUserInList but returns non-const pointer to User object.
  User* FindUserInListAndModify(const std::string& user_id);

  // Reads user's oauth token status from local state preferences.
  User::OAuthTokenStatus LoadUserOAuthStatus(const std::string& user_id) const;

  // Read a flag indicating whether online authentication against GAIA should
  // be enforced during the user's next sign-in from local state preferences.
  bool LoadForceOnlineSignin(const std::string& user_id) const;

  // Notifies observers that merge session state had changed.
  void NotifyMergeSessionStateChanged();

  // Notifies observers that active user has changed.
  void NotifyActiveUserChanged(const User* active_user);

  // Notifies observers that active user_id hash has changed.
  void NotifyActiveUserHashChanged(const std::string& hash);

  // Update the global LoginState.
  void UpdateLoginState();

  // Insert |user| at the front of the LRU user list.
  void SetLRUUser(User* user);

  // Sends metrics in response to a regular user logging in.
  void SendRegularUserLoginMetrics(const std::string& user_id);

  // Sets account locale for user with id |user_id|.
  virtual void UpdateUserAccountLocale(const std::string& user_id,
                                       const std::string& locale);

  // Updates user account after locale was resolved.
  void DoUpdateAccountLocale(const std::string& user_id,
                             scoped_ptr<std::string> resolved_locale);

  // Indicates stage of loading user from prefs.
  UserLoadStage user_loading_stage_;

  // List of all users that are logged in current session. These point to User
  // instances in |users_|. Only one of them could be marked as active.
  UserList logged_in_users_;

  // A list of all users that are logged in the current session. In contrast to
  // |logged_in_users|, the order of this list is least recently used so that
  // the active user should always be the first one in the list.
  UserList lru_logged_in_users_;

  // True if SessionStarted() has been called.
  bool session_started_;

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

  ObserverList<UserManager::Observer> observer_list_;

  // TODO(nkostylev): Merge with session state refactoring CL.
  ObserverList<UserManager::UserSessionStateObserver>
      session_state_observer_list_;

  // Time at which this object was created.
  base::TimeTicks manager_creation_time_;

  // ID of the user just added to the session that needs to be activated
  // as soon as user's profile is loaded.
  std::string pending_user_switch_;

  // ID of the user that was active in the previous session.
  // Preference value is stored here before first user signs in
  // because pref will be overidden once session restore starts.
  std::string last_session_active_user_;
  bool last_session_active_user_initialized_;

  // TaskRunner for UI thread.
  scoped_refptr<base::TaskRunner> task_runner_;

  // TaskRunner for SequencedWorkerPool.
  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  base::WeakPtrFactory<UserManagerBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerBase);
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_MANAGER_BASE_H_
