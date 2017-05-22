// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_CHROME_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_CHROME_USER_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"

namespace chromeos {

class FakeSupervisedUserManager;

// Fake chrome user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class FakeChromeUserManager : public ChromeUserManager {
 public:
  FakeChromeUserManager();
  ~FakeChromeUserManager() override;

  // Create and add various types of users.
  user_manager::User* AddKioskAppUser(const AccountId& account_id);
  user_manager::User* AddArcKioskAppUser(const AccountId& account_id);
  user_manager::User* AddSupervisedUser(const AccountId& account_id);
  const user_manager::User* AddPublicAccountUser(const AccountId& account_id);

  // Calculates the user name hash and calls UserLoggedIn to login a user.
  // Sets the user as having its profile created, but does not create a profile.
  // NOTE: This does not match production, which first logs in the user, then
  // creates the profile and updates the user later.
  void LoginUser(const AccountId& account_id);

  const user_manager::User* AddUser(const AccountId& account_id);
  const user_manager::User* AddUserWithAffiliation(const AccountId& account_id,
                                                   bool is_affiliated);

  // Creates the instance returned by |GetLocalState()| (which returns nullptr
  // by default).
  void CreateLocalState();

  // user_manager::UserManager override.
  void Shutdown() override;
  const user_manager::UserList& GetUsers() const override;
  user_manager::UserList GetUsersAllowedForMultiProfile() const override;
  const user_manager::UserList& GetLoggedInUsers() const override;
  const user_manager::UserList& GetLRULoggedInUsers() const override;
  user_manager::UserList GetUnlockUsers() const override;
  const AccountId& GetOwnerAccountId() const override;
  void UserLoggedIn(const AccountId& account_id,
                    const std::string& user_id_hash,
                    bool browser_restart) override;
  void SwitchActiveUser(const AccountId& account_id) override;
  void SwitchToLastActiveUser() override;
  void OnSessionStarted() override;
  void OnProfileInitialized(user_manager::User* user) override;
  void RemoveUser(const AccountId& account_id,
                  user_manager::RemoveUserDelegate* delegate) override;
  void RemoveUserFromList(const AccountId& account_id) override;
  bool IsKnownUser(const AccountId& account_id) const override;
  const user_manager::User* FindUser(
      const AccountId& account_id) const override;
  user_manager::User* FindUserAndModify(const AccountId& account_id) override;
  const user_manager::User* GetActiveUser() const override;
  user_manager::User* GetActiveUser() override;
  const user_manager::User* GetPrimaryUser() const override;
  void SaveUserOAuthStatus(
      const AccountId& account_id,
      user_manager::User::OAuthTokenStatus oauth_token_status) override;
  void SaveForceOnlineSignin(const AccountId& account_id,
                             bool force_online_signin) override;
  void SaveUserDisplayName(const AccountId& account_id,
                           const base::string16& display_name) override;
  base::string16 GetUserDisplayName(const AccountId& account_id) const override;
  void SaveUserDisplayEmail(const AccountId& account_id,
                            const std::string& display_email) override;
  std::string GetUserDisplayEmail(const AccountId& account_id) const override;
  void SaveUserType(const AccountId& account_id,
                    const user_manager::UserType& user_type) override;
  void UpdateUserAccountData(const AccountId& account_id,
                             const UserAccountData& account_data) override;
  bool IsCurrentUserOwner() const override;
  bool IsCurrentUserNew() const override;
  bool IsCurrentUserNonCryptohomeDataEphemeral() const override;
  bool CanCurrentUserLock() const override;
  bool IsUserLoggedIn() const override;
  bool IsLoggedInAsUserWithGaiaAccount() const override;
  bool IsLoggedInAsChildUser() const override;
  bool IsLoggedInAsPublicAccount() const override;
  bool IsLoggedInAsGuest() const override;
  bool IsLoggedInAsSupervisedUser() const override;
  bool IsLoggedInAsKioskApp() const override;
  bool IsLoggedInAsArcKioskApp() const override;
  bool IsLoggedInAsStub() const override;
  bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  void ChangeUserChildStatus(user_manager::User* user, bool is_child) override;
  bool AreSupervisedUsersAllowed() const override;
  PrefService* GetLocalState() const override;
  bool GetPlatformKnownUserId(const std::string& user_email,
                              const std::string& gaia_id,
                              AccountId* out_account_id) const override;
  const AccountId& GetGuestAccountId() const override;
  bool IsFirstExecAfterBoot() const override;
  void AsyncRemoveCryptohome(const AccountId& account_id) const override;
  bool IsGuestAccountId(const AccountId& account_id) const override;
  bool IsStubAccountId(const AccountId& account_id) const override;
  bool IsSupervisedAccountId(const AccountId& account_id) const override;
  bool HasBrowserRestarted() const override;
  const gfx::ImageSkia& GetResourceImagekiaNamed(int id) const override;
  base::string16 GetResourceStringUTF16(int string_id) const override;
  void ScheduleResolveLocale(const std::string& locale,
                             const base::Closure& on_resolved_callback,
                             std::string* out_resolved_locale) const override;
  bool IsValidDefaultUserImageId(int image_index) const override;
  bool AreEphemeralUsersEnabled() const override;
  void SetIsCurrentUserNew(bool is_new) override;
  void Initialize() override;

  // user_manager::UserManagerBase override.
  const std::string& GetApplicationLocale() const override;
  void HandleUserOAuthTokenStatusChange(
      const AccountId& account_id,
      user_manager::User::OAuthTokenStatus status) const override;
  void LoadDeviceLocalAccounts(std::set<AccountId>* users_set) override;
  bool IsEnterpriseManaged() const override;
  void PerformPreUserListLoadingActions() override;
  void PerformPostUserListLoadingActions() override;
  void PerformPostUserLoggedInActions(bool browser_restart) override;
  bool IsDemoApp(const AccountId& account_id) const override;
  bool IsDeviceLocalAccountMarkedForRemoval(
      const AccountId& account_id) const override;
  void DemoAccountLoggedIn() override;
  void KioskAppLoggedIn(user_manager::User* user) override;
  void ArcKioskAppLoggedIn(user_manager::User* user) override;
  void PublicAccountUserLoggedIn(user_manager::User* user) override;
  void SupervisedUserLoggedIn(const AccountId& account_id) override;
  void OnUserRemoved(const AccountId& account_id) override;
  void UpdateLoginState(const user_manager::User* active_user,
                        const user_manager::User* primary_user,
                        bool is_current_user_owner) const override;

  // UserManagerInterface override.
  BootstrapManager* GetBootstrapManager() override;
  MultiProfileUserController* GetMultiProfileUserController() override;
  UserImageManager* GetUserImageManager(const AccountId& account_id) override;
  SupervisedUserManager* GetSupervisedUserManager() override;
  void SetUserFlow(const AccountId& account_id, UserFlow* flow) override;
  UserFlow* GetCurrentUserFlow() const override;
  UserFlow* GetUserFlow(const AccountId& account_id) const override;
  void ResetUserFlow(const AccountId& account_id) override;
  user_manager::UserList GetUsersAllowedForSupervisedUsersCreation()
      const override;

  // ChromeUserManager override.
  void SetUserAffiliation(
      const std::string& user_email,
      const AffiliationIDSet& user_affiliation_ids) override;
  bool ShouldReportUser(const std::string& user_id) const override;

  void set_ephemeral_users_enabled(bool ephemeral_users_enabled) {
    fake_ephemeral_users_enabled_ = ephemeral_users_enabled;
  }

  void set_owner_id(const AccountId& owner_account_id) {
    owner_account_id_ = owner_account_id;
  }

  void set_bootstrap_manager(BootstrapManager* bootstrap_manager) {
    bootstrap_manager_ = bootstrap_manager;
  }

  void set_multi_profile_user_controller(
      MultiProfileUserController* controller) {
    multi_profile_user_controller_ = controller;
  }

  void set_current_user_new(bool new_user) { current_user_new_ = new_user; }

 private:
  // Lazily creates default user flow.
  UserFlow* GetDefaultUserFlow() const;

  // Returns the active user.
  user_manager::User* GetActiveUserInternal() const;

  std::unique_ptr<FakeSupervisedUserManager> supervised_user_manager_;
  AccountId owner_account_id_ = EmptyAccountId();
  bool fake_ephemeral_users_enabled_ = false;
  bool current_user_new_ = false;

  BootstrapManager* bootstrap_manager_ = nullptr;
  MultiProfileUserController* multi_profile_user_controller_ = nullptr;

  // If set this is the active user. If empty, the first created user is the
  // active user.
  AccountId active_account_id_ = EmptyAccountId();

  // Lazy-initialized default flow.
  mutable std::unique_ptr<UserFlow> default_flow_;

  using FlowMap = std::map<AccountId, UserFlow*>;

  std::unique_ptr<TestingPrefServiceSimple> local_state_;

  // Specific flows by user e-mail.
  // Keys should be canonicalized before access.
  FlowMap specific_flows_;

  DISALLOW_COPY_AND_ASSIGN(FakeChromeUserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_CHROME_USER_MANAGER_H_
