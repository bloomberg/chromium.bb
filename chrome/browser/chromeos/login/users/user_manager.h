// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_USER_MANAGER_H_

#include <string>

#include "chrome/browser/chromeos/login/user_flow.h"
#include "components/user_manager/user.h"

class PrefRegistrySimple;

namespace chromeos {

class MultiProfileUserController;
class RemoveUserDelegate;
class UserImageManager;
class SupervisedUserManager;

// Base class for UserManagerImpl - provides a mechanism for discovering users
// who have logged into this Chrome OS device before and updating that list.
class UserManager {
 public:
  // Interface that observers of UserManager must implement in order
  // to receive notification when local state preferences is changed
  class Observer {
   public:
    // Called when the local state preferences is changed.
    virtual void LocalStateChanged(UserManager* user_manager);

   protected:
    virtual ~Observer();
  };

  // TODO(nkostylev): Refactor and move this observer out of UserManager.
  // Observer interface that defines methods used to notify on user session /
  // active user state changes. Default implementation is empty.
  class UserSessionStateObserver {
   public:
    // Called when active user has changed.
    virtual void ActiveUserChanged(const user_manager::User* active_user);

    // Called when another user got added to the existing session.
    virtual void UserAddedToSession(const user_manager::User* added_user);

    // Called right before notifying on user change so that those who rely
    // on user_id hash would be accessing up-to-date value.
    virtual void ActiveUserHashChanged(const std::string& hash);

   protected:
    virtual ~UserSessionStateObserver();
  };

  // Data retrieved from user account.
  class UserAccountData {
   public:
    UserAccountData(const base::string16& display_name,
                    const base::string16& given_name,
                    const std::string& locale);
    ~UserAccountData();
    const base::string16& display_name() const { return display_name_; }
    const base::string16& given_name() const { return given_name_; }
    const std::string& locale() const { return locale_; }

   private:
    const base::string16 display_name_;
    const base::string16 given_name_;
    const std::string locale_;

    DISALLOW_COPY_AND_ASSIGN(UserAccountData);
  };

  // Creates the singleton instance. This method is not thread-safe and must be
  // called from the main UI thread.
  static void Initialize();

  // Checks whether the singleton instance has been created already. This method
  // is not thread-safe and must be called from the main UI thread.
  static bool IsInitialized();

  // Shuts down the UserManager. After this method has been called, the
  // singleton has unregistered itself as an observer but remains available so
  // that other classes can access it during their shutdown. This method is not
  // thread-safe and must be called from the main UI thread.
  virtual void Shutdown() = 0;

  // Destroys the singleton instance. Always call Shutdown() first. This method
  // is not thread-safe and must be called from the main UI thread.
  static void Destroy();

  // Returns the singleton instance or |NULL| if the singleton has either not
  // been created yet or is already destroyed. This method is not thread-safe
  // and must be called from the main UI thread.
  static UserManager* Get();

  // Registers user manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual ~UserManager();

  virtual MultiProfileUserController* GetMultiProfileUserController() = 0;
  virtual UserImageManager* GetUserImageManager(const std::string& user_id) = 0;
  virtual SupervisedUserManager* GetSupervisedUserManager() = 0;

  // Returns a list of users who have logged into this device previously. This
  // is sorted by last login date with the most recent user at the beginning.
  virtual const user_manager::UserList& GetUsers() const = 0;

  // Returns list of users admitted for logging in into multi-profile session.
  // Users that have a policy that prevents them from being added to the
  // multi-profile session will still be part of this list as long as they
  // are regular users (i.e. not a public session/supervised etc.).
  // Returns an empty list in case when primary user is not a regular one or
  // has a policy that prohibids it to be part of multi-profile session.
  virtual user_manager::UserList GetUsersAdmittedForMultiProfile() const = 0;

  // Returns a list of users who are currently logged in.
  virtual const user_manager::UserList& GetLoggedInUsers() const = 0;

  // Returns a list of users who are currently logged in in the LRU order -
  // so the active user is the first one in the list. If there is no user logged
  // in, the current user will be returned.
  virtual const user_manager::UserList& GetLRULoggedInUsers() = 0;

  // Returns a list of users who can unlock the device.
  // This list is based on policy and whether user is able to do unlock.
  // Policy:
  // * If user has primary-only policy then it is the only user in unlock users.
  // * Otherwise all users with unrestricted policy are added to this list.
  // All users that are unable to perform unlock are excluded from this list.
  virtual user_manager::UserList GetUnlockUsers() const = 0;

  // Returns the email of the owner user. Returns an empty string if there is
  // no owner for the device.
  virtual const std::string& GetOwnerEmail() = 0;

  // Indicates that a user with the given |user_id| has just logged in. The
  // persistent list is updated accordingly if the user is not ephemeral.
  // |browser_restart| is true when reloading Chrome after crash to distinguish
  // from normal sign in flow.
  // |username_hash| is used to identify homedir mount point.
  virtual void UserLoggedIn(const std::string& user_id,
                            const std::string& username_hash,
                            bool browser_restart) = 0;

  // Switches to active user identified by |user_id|. User has to be logged in.
  virtual void SwitchActiveUser(const std::string& user_id) = 0;

  // Called when browser session is started i.e. after
  // browser_creator.LaunchBrowser(...) was called after user sign in.
  // When user is at the image screen IsUserLoggedIn() will return true
  // but IsSessionStarted() will return false. During the kiosk splash screen,
  // we perform additional initialization after the user is logged in but
  // before the session has been started.
  // Fires NOTIFICATION_SESSION_STARTED.
  virtual void SessionStarted() = 0;

  // Removes the user from the device. Note, it will verify that the given user
  // isn't the owner, so calling this method for the owner will take no effect.
  // Note, |delegate| can be NULL.
  virtual void RemoveUser(const std::string& user_id,
                          RemoveUserDelegate* delegate) = 0;

  // Removes the user from the persistent list only. Also removes the user's
  // picture.
  virtual void RemoveUserFromList(const std::string& user_id) = 0;

  // Returns true if a user with the given user id is found in the persistent
  // list or currently logged in as ephemeral.
  virtual bool IsKnownUser(const std::string& user_id) const = 0;

  // Returns the user with the given user id if found in the persistent
  // list or currently logged in as ephemeral. Returns |NULL| otherwise.
  virtual const user_manager::User* FindUser(
      const std::string& user_id) const = 0;

  // Returns the user with the given user id if found in the persistent
  // list or currently logged in as ephemeral. Returns |NULL| otherwise.
  // Same as FindUser but returns non-const pointer to User object.
  virtual user_manager::User* FindUserAndModify(const std::string& user_id) = 0;

  // Returns the logged-in user.
  // TODO(nkostylev): Deprecate this call, move clients to GetActiveUser().
  // http://crbug.com/230852
  virtual const user_manager::User* GetLoggedInUser() const = 0;
  virtual user_manager::User* GetLoggedInUser() = 0;

  // Returns the logged-in user that is currently active within this session.
  // There could be multiple users logged in at the the same but for now
  // we support only one of them being active.
  virtual const user_manager::User* GetActiveUser() const = 0;
  virtual user_manager::User* GetActiveUser() = 0;

  // Returns the primary user of the current session. It is recorded for the
  // first signed-in user and does not change thereafter.
  virtual const user_manager::User* GetPrimaryUser() const = 0;

  // Saves user's oauth token status in local state preferences.
  virtual void SaveUserOAuthStatus(
      const std::string& user_id,
      user_manager::User::OAuthTokenStatus oauth_token_status) = 0;

  // Saves a flag indicating whether online authentication against GAIA should
  // be enforced during the user's next sign-in.
  virtual void SaveForceOnlineSignin(const std::string& user_id,
                                     bool force_online_signin) = 0;

  // Saves user's displayed name in local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserDisplayName(const std::string& user_id,
                                   const base::string16& display_name) = 0;

  // Updates data upon User Account download.
  virtual void UpdateUserAccountData(const std::string& user_id,
                                     const UserAccountData& account_data) = 0;

  // Returns the display name for user |user_id| if it is known (was
  // previously set by a |SaveUserDisplayName| call).
  // Otherwise, returns an empty string.
  virtual base::string16 GetUserDisplayName(
      const std::string& user_id) const = 0;

  // Saves user's displayed (non-canonical) email in local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserDisplayEmail(const std::string& user_id,
                                    const std::string& display_email) = 0;

  // Returns the display email for user |user_id| if it is known (was
  // previously set by a |SaveUserDisplayEmail| call).
  // Otherwise, returns |user_id| itself.
  virtual std::string GetUserDisplayEmail(
      const std::string& user_id) const = 0;

  // Returns true if current user is an owner.
  virtual bool IsCurrentUserOwner() const = 0;

  // Returns true if current user is not existing one (hasn't signed in before).
  virtual bool IsCurrentUserNew() const = 0;

  // Returns true if data stored or cached for the current user outside that
  // user's cryptohome (wallpaper, avatar, OAuth token status, display name,
  // display email) is ephemeral.
  virtual bool IsCurrentUserNonCryptohomeDataEphemeral() const = 0;

  // Returns true if the current user's session can be locked (i.e. the user has
  // a password with which to unlock the session).
  virtual bool CanCurrentUserLock() const = 0;

  // Returns true if at least one user has signed in.
  virtual bool IsUserLoggedIn() const = 0;

  // Returns true if we're logged in as a regular user.
  virtual bool IsLoggedInAsRegularUser() const = 0;

  // Returns true if we're logged in as a demo user.
  virtual bool IsLoggedInAsDemoUser() const = 0;

  // Returns true if we're logged in as a public account.
  virtual bool IsLoggedInAsPublicAccount() const = 0;

  // Returns true if we're logged in as a Guest.
  virtual bool IsLoggedInAsGuest() const = 0;

  // Returns true if we're logged in as a supervised user.
  virtual bool IsLoggedInAsSupervisedUser() const = 0;

  // Returns true if we're logged in as a kiosk app.
  virtual bool IsLoggedInAsKioskApp() const = 0;

  // Returns true if we're logged in as the stub user used for testing on Linux.
  virtual bool IsLoggedInAsStub() const = 0;

  // Returns true if we're logged in and browser has been started i.e.
  // browser_creator.LaunchBrowser(...) was called after sign in
  // or restart after crash.
  virtual bool IsSessionStarted() const = 0;

  // Returns true if data stored or cached for the user with the given user id
  // address outside that user's cryptohome (wallpaper, avatar, OAuth token
  // status, display name, display email) is to be treated as ephemeral.
  virtual bool IsUserNonCryptohomeDataEphemeral(
      const std::string& user_id) const = 0;

  // Method that allows to set |flow| for user identified by |user_id|.
  // Flow should be set before login attempt.
  // Takes ownership of the |flow|, |flow| will be deleted in case of login
  // failure.
  virtual void SetUserFlow(const std::string& user_id, UserFlow* flow) = 0;

  // Return user flow for current user. Returns instance of DefaultUserFlow if
  // no flow was defined for current user, or user is not logged in.
  // Returned value should not be cached.
  virtual UserFlow* GetCurrentUserFlow() const = 0;

  // Return user flow for user identified by |user_id|. Returns instance of
  // DefaultUserFlow if no flow was defined for user.
  // Returned value should not be cached.
  virtual UserFlow* GetUserFlow(const std::string& user_id) const = 0;

  // Resets user flow for user identified by |user_id|.
  virtual void ResetUserFlow(const std::string& user_id) = 0;

  virtual void AddObserver(Observer* obs) = 0;
  virtual void RemoveObserver(Observer* obs) = 0;

  virtual void AddSessionStateObserver(UserSessionStateObserver* obs) = 0;
  virtual void RemoveSessionStateObserver(UserSessionStateObserver* obs) = 0;

  virtual void NotifyLocalStateChanged() = 0;

  // Returns true if supervised users allowed.
  virtual bool AreSupervisedUsersAllowed() const = 0;

 private:
  friend class ScopedUserManagerEnabler;

  // Sets the singleton to the given |user_manager|, taking ownership. Returns
  // the previous value of the singleton, passing ownership.
  static UserManager* SetForTesting(UserManager* user_manager);
};

// Helper class for unit tests. Initializes the UserManager singleton to the
// given |user_manager| and tears it down again on destruction. If the singleton
// had already been initialized, its previous value is restored after tearing
// down |user_manager|.
class ScopedUserManagerEnabler {
 public:
  // Takes ownership of |user_manager|.
  explicit ScopedUserManagerEnabler(UserManager* user_manager);
  ~ScopedUserManagerEnabler();

 private:
  UserManager* previous_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUserManagerEnabler);
};

// Helper class for unit tests. Initializes the UserManager singleton on
// construction and tears it down again on destruction.
class ScopedTestUserManager {
 public:
  ScopedTestUserManager();
  ~ScopedTestUserManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedTestUserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_USER_MANAGER_H_
