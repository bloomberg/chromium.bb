// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/signin/token_handle_util.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/models/user_board_model.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/user_manager/user.h"
#include "ui/base/user_activity/user_activity_observer.h"

class AccountId;
class EasyUnlockService;

namespace chromeos {

class LoginDisplayWebUIHandler;
class UserBoardView;

// This class represents User Selection screen: user pod-based login screen.
class UserSelectionScreen
    : public ui::UserActivityObserver,
      public proximity_auth::ScreenlockBridge::LockHandler,
      public UserBoardModel {
 public:
  explicit UserSelectionScreen(const std::string& display_type);
  ~UserSelectionScreen() override;

  void SetLoginDisplayDelegate(LoginDisplay::Delegate* login_display_delegate);
  void SetHandler(LoginDisplayWebUIHandler* handler);
  void SetView(UserBoardView* view);

  static const user_manager::UserList PrepareUserListForSending(
      const user_manager::UserList& users,
      std::string owner,
      bool is_signin_to_add);


  virtual void Init(const user_manager::UserList& users, bool show_guest);
  void OnUserImageChanged(const user_manager::User& user);
  void OnBeforeUserRemoved(const std::string& username);
  void OnUserRemoved(const std::string& username);

  void OnPasswordClearTimerExpired();

  void HandleGetUsers();
  void CheckUserStatus(const std::string& user_id);

  // ui::UserActivityDetector implementation:
  void OnUserActivity(const ui::Event* event) override;

  void InitEasyUnlock();

  // proximity_auth::ScreenlockBridge::LockHandler implementation:
  void ShowBannerMessage(const base::string16& message) override;
  void ShowUserPodCustomIcon(
      const std::string& user_email,
      const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions& icon)
      override;
  void HideUserPodCustomIcon(const std::string& user_email) override;

  void EnableInput() override;
  void SetAuthType(const std::string& user_email,
                   AuthType auth_type,
                   const base::string16& auth_value) override;
  AuthType GetAuthType(const std::string& user_email) const override;
  ScreenType GetScreenType() const override;

  void Unlock(const std::string& user_email) override;
  void AttemptEasySignin(const std::string& user_email,
                         const std::string& secret,
                         const std::string& key_label) override;

  // UserBoardModel implementation.
  void SendUserList() override;
  void HardLockPod(const std::string& user_id) override;
  void AttemptEasyUnlock(const std::string& user_id) override;
  void RecordClickOnLockIcon(const std::string& user_id) override;

  // Fills |user_dict| with information about |user|.
  static void FillUserDictionary(
      user_manager::User* user,
      bool is_owner,
      bool is_signin_to_add,
      AuthType auth_type,
      const std::vector<std::string>* public_session_recommended_locales,
      base::DictionaryValue* user_dict);

  // Fills |user_dict| with |user| known preferences.
  static void FillKnownUserPrefs(user_manager::User* user,
                                 base::DictionaryValue* user_dict);

  // Fills |user_dict| with |user| multi-profile related preferences.
  static void FillMultiProfileUserPrefs(user_manager::User* user,
                                        base::DictionaryValue* user_dict,
                                        bool is_signin_to_add);

  // Determines if user auth status requires online sign in.
  static bool ShouldForceOnlineSignIn(const user_manager::User* user);

 protected:
  LoginDisplayWebUIHandler* handler_;
  LoginDisplay::Delegate* login_display_delegate_;
  UserBoardView* view_;

  // Map from public session user IDs to recommended locales set by policy.
  typedef std::map<std::string, std::vector<std::string> >
      PublicSessionRecommendedLocaleMap;
  PublicSessionRecommendedLocaleMap public_session_recommended_locales_;

 private:
  EasyUnlockService* GetEasyUnlockServiceForUser(
      const std::string& user_id) const;

  void OnUserStatusChecked(const AccountId& account_id,
                           TokenHandleUtil::TokenHandleStatus status);

  // Whether to show guest login.
  bool show_guest_;

  // Purpose of the screen (see constants in OobeUI).
  const std::string display_type_;

  // Set of Users that are visible.
  user_manager::UserList users_;

  // Map of usernames to their current authentication type. If a user is not
  // contained in the map, it is using the default authentication type.
  std::map<std::string, proximity_auth::ScreenlockBridge::LockHandler::AuthType>
      user_auth_type_map_;

  // Timer for measuring idle state duration before password clear.
  base::OneShotTimer password_clear_timer_;

  // Token handler util for checking user OAuth token status.
  scoped_ptr<TokenHandleUtil> token_handle_util_;

  base::WeakPtrFactory<UserSelectionScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserSelectionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
