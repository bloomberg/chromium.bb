// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_

#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_flow.h"

class PrefRegistrySimple;

namespace chromeos {

class RemoveUserDelegate;
class UserImageManager;

// Base class for UserManagerImpl - provides a mechanism for discovering users
// who have logged into this Chrome OS device before and updating that list.
class UserManager {
 public:
  // Status of merge sessions process which is responsible for exchanging
  // user OAuth2 refresh token for GAIA cookies.
  enum MergeSessionState {
    // Session merge hasn't started yet.
    MERGE_STATUS_NOT_STARTED,
    // Session merge is in process.
    MERGE_STATUS_IN_PROCESS,
    // Session merge is completed.
    MERGE_STATUS_DONE,
  };

  // Interface that observers of UserManager must implement in order
  // to receive notification when local state preferences is changed
  class Observer {
   public:
    // Called when the local state preferences is changed.
    virtual void LocalStateChanged(UserManager* user_manager) = 0;

    // Called when merge session state is changed.
    virtual void MergeSessionStateChanged(MergeSessionState state) {}

   protected:
    virtual ~Observer() {}
  };

  // Username for stub login when not running on ChromeOS.
  static const char kStubUser[];

  // Domain that is used for all locally managed users.
  static const char kLocallyManagedUserDomain[];

  // Domain that is used for kiosk app robot.
  static const char kKioskAppUserDomain[];

  // Returns a shared instance of a UserManager. Not thread-safe, should only be
  // called from the main UI thread.
  static UserManager* Get();

  // Set UserManager singleton object for test purpose only! Returns the
  // previous singleton object and releases it from the singleton memory
  // management. It is the responsibility of the test writer to restore the
  // original object or delete it if needed.
  //
  // The intended usage is meant to be something like this:
  //   virtual void SetUp() {
  //     mock_user_manager_.reset(new MockUserManager());
  //     old_user_manager_ = UserManager::Set(mock_user_manager_.get());
  //     EXPECT_CALL...
  //     ...
  //   }
  //   virtual void TearDown() {
  //     ...
  //     UserManager::Set(old_user_manager_);
  //   }
  //   scoped_ptr<MockUserManager> mock_user_manager_;
  //   UserManager* old_user_manager_;
  static UserManager* Set(UserManager* mock);

  // Registers user manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Indicates imminent shutdown, allowing the UserManager to remove any
  // observers it has registered.
  virtual void Shutdown() = 0;

  virtual ~UserManager();

  virtual UserImageManager* GetUserImageManager() = 0;

  // Returns a list of users who have logged into this device previously. This
  // is sorted by last login date with the most recent user at the beginning.
  virtual const UserList& GetUsers() const = 0;

  // Indicates that a user with the given |email| has just logged in. The
  // persistent list is updated accordingly if the user is not ephemeral.
  // |browser_restart| is true when reloading Chrome after crash to distinguish
  // from normal sign in flow.
  // |username_hash| is used to identify homedir mount point.
  virtual void UserLoggedIn(const std::string& email,
                            const std::string& username_hash,
                            bool browser_restart) = 0;

  // Indicates that user just logged on as the retail mode user.
  virtual void RetailModeUserLoggedIn() = 0;

  // Indicates that user just started incognito session.
  virtual void GuestUserLoggedIn() = 0;

  // Indicates that a kiosk app robot just logged in.
  virtual void KioskAppLoggedIn(const std::string& app_id) = 0;

  // Indicates that a locally managed user just logged in.
  virtual void LocallyManagedUserLoggedIn(const std::string& username) = 0;

  // Indicates that a user just logged into a public account.
  virtual void PublicAccountUserLoggedIn(User* user) = 0;

  // Indicates that a regular user just logged in.
  virtual void RegularUserLoggedIn(const std::string& email,
                                   bool browser_restart) = 0;

  // Indicates that a regular user just logged in as ephemeral.
  virtual void RegularUserLoggedInAsEphemeral(const std::string& email) = 0;

  // Called when browser session is started i.e. after
  // browser_creator.LaunchBrowser(...) was called after user sign in.
  // When user is at the image screen IsUserLoggedIn() will return true
  // but SessionStarted() will return false.
  // Fires NOTIFICATION_SESSION_STARTED.
  virtual void SessionStarted() = 0;

  // Creates locally managed user with given display name, and id (e-mail), and
  // sets |display_name| for created user and stores it to
  // persistent list. Returns created user, or existing user if there already
  // was locally managed user with such display name.
  virtual const User* CreateLocallyManagedUserRecord(
      const std::string& e_mail,
      const string16& display_name) = 0;

  // Generates unique username for locally managed user.
  virtual std::string GenerateUniqueLocallyManagedUserId() = 0;

  // Removes the user from the device. Note, it will verify that the given user
  // isn't the owner, so calling this method for the owner will take no effect.
  // Note, |delegate| can be NULL.
  virtual void RemoveUser(const std::string& email,
                          RemoveUserDelegate* delegate) = 0;

  // Removes the user from the persistent list only. Also removes the user's
  // picture.
  virtual void RemoveUserFromList(const std::string& email) = 0;

  // Returns true if a user with the given email address is found in the
  // persistent list or currently logged in as ephemeral.
  virtual bool IsKnownUser(const std::string& email) const = 0;

  // Returns the user with the given email address if found in the persistent
  // list or currently logged in as ephemeral. Returns |NULL| otherwise.
  virtual const User* FindUser(const std::string& email) const = 0;

  // Returns the locally managed user with the given |display_name| if found in
  // the persistent list. Returns |NULL| otherwise.
  virtual const User* FindLocallyManagedUser(
      const string16& display_name) const = 0;

  // Returns the logged-in user.
  virtual const User* GetLoggedInUser() const = 0;
  virtual User* GetLoggedInUser() = 0;

  // Saves user's oauth token status in local state preferences.
  virtual void SaveUserOAuthStatus(
      const std::string& username,
      User::OAuthTokenStatus oauth_token_status) = 0;

  // Saves user's displayed name in local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserDisplayName(const std::string& username,
                                   const string16& display_name) = 0;

  // Returns the display name for user |username| if it is known (was
  // previously set by a |SaveUserDisplayName| call).
  // Otherwise, returns an empty string.
  virtual string16 GetUserDisplayName(
      const std::string& username) const = 0;

  // Saves user's displayed (non-canonical) email in local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserDisplayEmail(const std::string& username,
                                    const std::string& display_email) = 0;

  // Returns the display email for user |username| if it is known (was
  // previously set by a |SaveUserDisplayEmail| call).
  // Otherwise, returns |username| itself.
  virtual std::string GetUserDisplayEmail(
      const std::string& username) const = 0;

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

  // Returns true if user is signed in.
  virtual bool IsUserLoggedIn() const = 0;

  // Returns true if we're logged in as a regular user.
  virtual bool IsLoggedInAsRegularUser() const = 0;

  // Returns true if we're logged in as a demo user.
  virtual bool IsLoggedInAsDemoUser() const = 0;

  // Returns true if we're logged in as a public account.
  virtual bool IsLoggedInAsPublicAccount() const = 0;

  // Returns true if we're logged in as a Guest.
  virtual bool IsLoggedInAsGuest() const = 0;

  // Returns true if we're logged in as a locally managed user.
  virtual bool IsLoggedInAsLocallyManagedUser() const = 0;

  // Returns true if we're logged in as a kiosk app.
  virtual bool IsLoggedInAsKioskApp() const = 0;

  // Returns true if we're logged in as the stub user used for testing on Linux.
  virtual bool IsLoggedInAsStub() const = 0;

  // Returns true if we're logged in and browser has been started i.e.
  // browser_creator.LaunchBrowser(...) was called after sign in
  // or restart after crash.
  virtual bool IsSessionStarted() const = 0;

  // Returns merge session status.
  virtual MergeSessionState GetMergeSessionState() const = 0;

  // Changes merge session status.
  virtual void SetMergeSessionState(MergeSessionState status) = 0;

  // Returns true when the browser has crashed and restarted during the current
  // user's session.
  virtual bool HasBrowserRestarted() const = 0;

  // Returns true if data stored or cached for the user with the given email
  // address outside that user's cryptohome (wallpaper, avatar, OAuth token
  // status, display name, display email) is to be treated as ephemeral.
  virtual bool IsUserNonCryptohomeDataEphemeral(
      const std::string& email) const = 0;

  // Create a record about starting locally managed user creation transaction.
  virtual void StartLocallyManagedUserCreationTransaction(
      const string16& display_name) = 0;

  // Add user id to locally managed user creation transaction record.
  virtual void SetLocallyManagedUserCreationTransactionUserId(
      const std::string& email) = 0;

  // Remove locally managed user creation transaction record.
  virtual void CommitLocallyManagedUserCreationTransaction() = 0;

  // Method that allows to set |flow| for user identified by |email|.
  // Flow should be set before login attempt.
  // Takes ownership of the |flow|, |flow| will be deleted in case of login
  // failure.
  virtual void SetUserFlow(const std::string& email, UserFlow* flow) = 0;

  // Return user flow for current user. Returns instance of DefaultUserFlow if
  // no flow was defined for current user, or user is not logged in.
  // Returned value should not be cached.
  virtual UserFlow* GetCurrentUserFlow() const = 0;

  // Return user flow for user identified by |email|. Returns instance of
  // DefaultUserFlow if no flow was defined for user.
  // Returned value should not be cached.
  virtual UserFlow* GetUserFlow(const std::string& email) const = 0;

  // Resets user flow fo user idenitified by |email|.
  virtual void ResetUserFlow(const std::string& email) = 0;

  // Gets/sets chrome oauth client id and secret for kiosk app mode. The default
  // values can be overriden with kiosk auth file.
  virtual bool GetAppModeChromeClientOAuthInfo(
      std::string* chrome_client_id,
      std::string* chrome_client_secret) = 0;
  virtual void SetAppModeChromeClientOAuthInfo(
      const std::string& chrome_client_id,
      const std::string& chrome_client_secret) = 0;

  virtual void AddObserver(Observer* obs) = 0;
  virtual void RemoveObserver(Observer* obs) = 0;

  virtual void NotifyLocalStateChanged() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
