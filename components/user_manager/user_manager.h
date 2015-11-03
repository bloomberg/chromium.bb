// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_MANAGER_H_
#define COMPONENTS_USER_MANAGER_USER_MANAGER_H_

#include <string>

#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_export.h"
#include "components/user_manager/user_type.h"

class AccountId;

namespace base {
class DictionaryValue;
}

namespace chromeos {
class ScopedUserManagerEnabler;
}

namespace user_manager {

class RemoveUserDelegate;

// Interface for UserManagerBase - that provides base implementation for
// Chrome OS user management. Typical features:
// * Get list of all know users (who have logged into this Chrome OS device)
// * Keep track for logged in/LRU users, active user in multi-user session.
// * Find/modify users, store user meta-data such as display name/email.
class USER_MANAGER_EXPORT UserManager {
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
    virtual void ActiveUserChanged(const User* active_user);

    // Called when another user got added to the existing session.
    virtual void UserAddedToSession(const User* added_user);

    // Called right before notifying on user change so that those who rely
    // on account_id hash would be accessing up-to-date value.
    virtual void ActiveUserHashChanged(const std::string& hash);

    // Called when child status has changed.
    virtual void UserChangedChildStatus(User* user);

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

  // Initializes UserManager instance to this. Normally should be called right
  // after creation so that user_manager::UserManager::Get() doesn't fail.
  // Tests could call this method if they are replacing existing UserManager
  // instance with their own test instance.
  void Initialize();

  // Checks whether the UserManager instance has been created already.
  // This method is not thread-safe and must be called from the main UI thread.
  static bool IsInitialized();

  // Shuts down the UserManager. After this method has been called, the
  // singleton has unregistered itself as an observer but remains available so
  // that other classes can access it during their shutdown. This method is not
  // thread-safe and must be called from the main UI thread.
  virtual void Shutdown() = 0;

  // Sets UserManager instance to NULL. Always call Shutdown() first.
  // This method is not thread-safe and must be called from the main UI thread.
  void Destroy();

  // Returns UserManager instance or will crash if it is |NULL| (has either not
  // been created yet or is already destroyed). This method is not thread-safe
  // and must be called from the main UI thread.
  static UserManager* Get();

  virtual ~UserManager();

  // Returns a list of users who have logged into this device previously. This
  // is sorted by last login date with the most recent user at the beginning.
  virtual const UserList& GetUsers() const = 0;

  // Returns list of users allowed for logging in into multi-profile session.
  // Users that have a policy that prevents them from being added to the
  // multi-profile session will still be part of this list as long as they
  // are regular users (i.e. not a public session/supervised etc.).
  // Returns an empty list in case when primary user is not a regular one or
  // has a policy that prohibits it to be part of multi-profile session.
  virtual UserList GetUsersAllowedForMultiProfile() const = 0;

  // Returns a list of users who are currently logged in.
  virtual const UserList& GetLoggedInUsers() const = 0;

  // Returns a list of users who are currently logged in in the LRU order -
  // so the active user is the first one in the list. If there is no user logged
  // in, the current user will be returned.
  virtual const UserList& GetLRULoggedInUsers() const = 0;

  // Returns a list of users who can unlock the device.
  // This list is based on policy and whether user is able to do unlock.
  // Policy:
  // * If user has primary-only policy then it is the only user in unlock users.
  // * Otherwise all users with unrestricted policy are added to this list.
  // All users that are unable to perform unlock are excluded from this list.
  virtual UserList GetUnlockUsers() const = 0;

  // Returns account Id of the owner user. Returns an empty Id if there is
  // no owner for the device.
  virtual const AccountId& GetOwnerAccountId() const = 0;

  // Indicates that a user with the given |account_id| has just logged in. The
  // persistent list is updated accordingly if the user is not ephemeral.
  // |browser_restart| is true when reloading Chrome after crash to distinguish
  // from normal sign in flow.
  // |username_hash| is used to identify homedir mount point.
  virtual void UserLoggedIn(const AccountId& account_id,
                            const std::string& username_hash,
                            bool browser_restart) = 0;

  // Switches to active user identified by |account_id|. User has to be logged
  // in.
  virtual void SwitchActiveUser(const AccountId& account_id) = 0;

  // Switches to the last active user (called after crash happens and session
  // restore has completed).
  virtual void SwitchToLastActiveUser() = 0;

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
  virtual void RemoveUser(const AccountId& account_id,
                          RemoveUserDelegate* delegate) = 0;

  // Removes the user from the persistent list only. Also removes the user's
  // picture.
  virtual void RemoveUserFromList(const AccountId& account_id) = 0;

  // Returns true if a user with the given account id is found in the persistent
  // list or currently logged in as ephemeral.
  virtual bool IsKnownUser(const AccountId& account_id) const = 0;

  // Returns the user with the given account id if found in the persistent
  // list or currently logged in as ephemeral. Returns |NULL| otherwise.
  virtual const User* FindUser(const AccountId& account_id) const = 0;

  // Returns the user with the given account id if found in the persistent
  // list or currently logged in as ephemeral. Returns |NULL| otherwise.
  // Same as FindUser but returns non-const pointer to User object.
  virtual User* FindUserAndModify(const AccountId& account_id) = 0;

  // Returns the logged-in user.
  // TODO(nkostylev): Deprecate this call, move clients to GetActiveUser().
  // http://crbug.com/230852
  virtual const User* GetLoggedInUser() const = 0;
  virtual User* GetLoggedInUser() = 0;

  // Returns the logged-in user that is currently active within this session.
  // There could be multiple users logged in at the the same but for now
  // we support only one of them being active.
  virtual const User* GetActiveUser() const = 0;
  virtual User* GetActiveUser() = 0;

  // Returns the primary user of the current session. It is recorded for the
  // first signed-in user and does not change thereafter.
  virtual const User* GetPrimaryUser() const = 0;

  // Saves user's oauth token status in local state preferences.
  virtual void SaveUserOAuthStatus(
      const AccountId& account_id,
      User::OAuthTokenStatus oauth_token_status) = 0;

  // Saves a flag indicating whether online authentication against GAIA should
  // be enforced during the user's next sign-in.
  virtual void SaveForceOnlineSignin(const AccountId& account_id,
                                     bool force_online_signin) = 0;

  // Saves user's displayed name in local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserDisplayName(const AccountId& account_id,
                                   const base::string16& display_name) = 0;

  // Updates data upon User Account download.
  virtual void UpdateUserAccountData(const AccountId& account_id,
                                     const UserAccountData& account_data) = 0;

  // Returns the display name for user |account_id| if it is known (was
  // previously set by a |SaveUserDisplayName| call).
  // Otherwise, returns an empty string.
  virtual base::string16 GetUserDisplayName(
      const AccountId& account_id) const = 0;

  // Saves user's displayed (non-canonical) email in local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserDisplayEmail(const AccountId& account_id,
                                    const std::string& display_email) = 0;

  // Returns the display email for user |account_id| if it is known (was
  // previously set by a |SaveUserDisplayEmail| call).
  // Otherwise, returns |account_id| itself.
  virtual std::string GetUserDisplayEmail(
      const AccountId& account_id) const = 0;

  // Saves user's type for user |account_id| into local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserType(const AccountId& account_id,
                            const UserType& user_type) = 0;

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

  // Returns true if we're logged in as a user with gaia account.
  virtual bool IsLoggedInAsUserWithGaiaAccount() const = 0;

  // Returns true if we're logged in as a child user.
  virtual bool IsLoggedInAsChildUser() const = 0;

  // Returns true if we're logged in as a public account.
  virtual bool IsLoggedInAsPublicAccount() const = 0;

  // Returns true if we're logged in as a Guest.
  virtual bool IsLoggedInAsGuest() const = 0;

  // Returns true if we're logged in as a legacy supervised user.
  virtual bool IsLoggedInAsSupervisedUser() const = 0;

  // Returns true if we're logged in as a kiosk app.
  virtual bool IsLoggedInAsKioskApp() const = 0;

  // Returns true if we're logged in as the stub user used for testing on Linux.
  virtual bool IsLoggedInAsStub() const = 0;

  // Returns true if we're logged in and browser has been started i.e.
  // browser_creator.LaunchBrowser(...) was called after sign in
  // or restart after crash.
  virtual bool IsSessionStarted() const = 0;

  // Returns true if data stored or cached for the user with the given
  // |account_id|
  // address outside that user's cryptohome (wallpaper, avatar, OAuth token
  // status, display name, display email) is to be treated as ephemeral.
  virtual bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const = 0;

  virtual void AddObserver(Observer* obs) = 0;
  virtual void RemoveObserver(Observer* obs) = 0;

  virtual void AddSessionStateObserver(UserSessionStateObserver* obs) = 0;
  virtual void RemoveSessionStateObserver(UserSessionStateObserver* obs) = 0;

  virtual void NotifyLocalStateChanged() = 0;

  // Changes the child status and notifies observers.
  virtual void ChangeUserChildStatus(User* user, bool is_child) = 0;


  // Returns true if supervised users allowed.
  virtual bool AreSupervisedUsersAllowed() const = 0;

  // Methods for storage/retrieval of per-user properties in Local State.

  // Performs a lookup of properties associated with |account_id|. If found,
  // returns |true| and fills |out_value|. |out_value| can be NULL, if
  // only existence check is required.
  virtual bool FindKnownUserPrefs(const AccountId& account_id,
                                  const base::DictionaryValue** out_value) = 0;

  // Updates (or creates) properties associated with |account_id| based
  // on |values|. |clear| defines if existing properties are cleared (|true|)
  // or if it is just a incremental update (|false|).
  virtual void UpdateKnownUserPrefs(const AccountId& account_id,
                                    const base::DictionaryValue& values,
                                    bool clear) = 0;

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  virtual bool GetKnownUserStringPref(const AccountId& account_id,
                                      const std::string& path,
                                      std::string* out_value) = 0;

  // Updates user's identified by |account_id| string preference |path|.
  virtual void SetKnownUserStringPref(const AccountId& account_id,
                                      const std::string& path,
                                      const std::string& in_value) = 0;

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  virtual bool GetKnownUserBooleanPref(const AccountId& account_id,
                                       const std::string& path,
                                       bool* out_value) = 0;

  // Updates user's identified by |account_id| boolean preference |path|.
  virtual void SetKnownUserBooleanPref(const AccountId& account_id,
                                       const std::string& path,
                                       const bool in_value) = 0;

  // Returns true if |account_id| preference by |path| does exist,
  // fills in |out_value|. Otherwise returns false.
  virtual bool GetKnownUserIntegerPref(const AccountId& account_id,
                                       const std::string& path,
                                       int* out_value) = 0;

  // Updates user's identified by |account_id| integer preference |path|.
  virtual void SetKnownUserIntegerPref(const AccountId& account_id,
                                       const std::string& path,
                                       const int in_value) = 0;

  // Returns true if user's AccountId was found.
  // Returns it in |out_account_id|.
  virtual bool GetKnownUserAccountId(const AccountId& authenticated_account_id,
                                     AccountId* out_account_id) = 0;

  // Updates |gaia_id| for user with |account_id|.
  // TODO(alemate): Update this once AccountId contains GAIA ID
  // (crbug.com/548926).
  virtual void UpdateGaiaID(const AccountId& account_id,
                            const std::string& gaia_id) = 0;

  // Find GAIA ID for user with |account_id|, fill in |out_value| and return
  // true
  // if GAIA ID was found or false otherwise.
  // TODO(antrim): Update this once AccountId contains GAIA ID
  // (crbug.com/548926).
  virtual bool FindGaiaID(const AccountId& account_id,
                          std::string* out_value) = 0;

  // Saves whether the user authenticates using SAML.
  virtual void UpdateUsingSAML(const AccountId& account_id,
                               const bool using_saml) = 0;

  // Returns if SAML needs to be used for authentication of the user with
  // |account_id|, if it is known (was set by a |UpdateUsingSaml| call).
  // Otherwise
  // returns false.
  virtual bool FindUsingSAML(const AccountId& account_id) = 0;

  // Setter and getter for DeviceId known user string preference.
  virtual void SetKnownUserDeviceId(const AccountId& account_id,
                                    const std::string& device_id) = 0;
  virtual std::string GetKnownUserDeviceId(const AccountId& account_id) = 0;

  // Setter and getter for GAPSCookie known user string preference.
  virtual void SetKnownUserGAPSCookie(const AccountId& account_id,
                                      const std::string& gaps_cookie) = 0;

  virtual std::string GetKnownUserGAPSCookie(const AccountId& account_id) = 0;

  // Saves why the user has to go through re-auth flow.
  virtual void UpdateReauthReason(const AccountId& account_id,
                                  const int reauth_reason) = 0;

  // Returns the reason why the user with |account_id| has to go through the
  // re-auth flow. Returns true if such a reason was recorded or false
  // otherwise.
  virtual bool FindReauthReason(const AccountId& account_id,
                                int* out_value) = 0;

 protected:
  // Sets UserManager instance.
  static void SetInstance(UserManager* user_manager);

  // Pointer to the existing UserManager instance (if any).
  // Usually is set by calling Initialize(), reset by calling Destroy().
  // Not owned since specific implementation of UserManager should decide on its
  // own appropriate owner. For src/chrome implementation such place is
  // g_browser_process->platform_part().
  static UserManager* instance;

 private:
  friend class chromeos::ScopedUserManagerEnabler;

  // Same as Get() but doesn't won't crash is current instance is NULL.
  static UserManager* GetForTesting();

  // Sets UserManager instance to the given |user_manager|.
  // Returns the previous value of the instance.
  static UserManager* SetForTesting(UserManager* user_manager);
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_MANAGER_H_
