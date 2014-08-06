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
#include "chrome/browser/signin/screenlock_bridge.h"
#include "components/user_manager/user.h"
#include "ui/wm/core/user_activity_observer.h"

namespace chromeos {

class LoginDisplayWebUIHandler;

// This class represents User Selection screen: user pod-based login screen.
class UserSelectionScreen : public wm::UserActivityObserver {
 public:
  UserSelectionScreen();
  virtual ~UserSelectionScreen();

  static const user_manager::UserList PrepareUserListForSending(
      const user_manager::UserList& users,
      std::string owner,
      bool is_signin_to_add);

  void SetHandler(LoginDisplayWebUIHandler* handler);

  virtual void Init(const user_manager::UserList& users, bool show_guest);
  const user_manager::UserList& GetUsers() const;
  void OnUserImageChanged(const user_manager::User& user);
  void OnBeforeUserRemoved(const std::string& username);
  void OnUserRemoved(const std::string& username);

  void OnPasswordClearTimerExpired();
  virtual void SendUserList();
  void HandleGetUsers();
  void SetAuthType(const std::string& username,
                   ScreenlockBridge::LockHandler::AuthType auth_type);
  ScreenlockBridge::LockHandler::AuthType GetAuthType(
      const std::string& username) const;

  // wm::UserActivityDetector implementation:
  virtual void OnUserActivity(const ui::Event* event) OVERRIDE;

  // Fills |user_dict| with information about |user|.
  static void FillUserDictionary(
      user_manager::User* user,
      bool is_owner,
      bool is_signin_to_add,
      ScreenlockBridge::LockHandler::AuthType auth_type,
      const std::vector<std::string>* public_session_recommended_locales,
      base::DictionaryValue* user_dict);

  // Determines if user auth status requires online sign in.
  static bool ShouldForceOnlineSignIn(const user_manager::User* user);

 protected:
  LoginDisplayWebUIHandler* handler_;

  // Set of Users that are visible.
  user_manager::UserList users_;

  // Map from public session user IDs to recommended locales set by policy.
  typedef std::map<std::string, std::vector<std::string> >
      PublicSessionRecommendedLocaleMap;
  PublicSessionRecommendedLocaleMap public_session_recommended_locales_;

 private:
  // Whether to show guest login.
  bool show_guest_;

  // Map of usernames to their current authentication type. If a user is not
  // contained in the map, it is using the default authentication type.
  std::map<std::string, ScreenlockBridge::LockHandler::AuthType>
      user_auth_type_map_;

  // Timer for measuring idle state duration before password clear.
  base::OneShotTimer<UserSelectionScreen> password_clear_timer_;

  DISALLOW_COPY_AND_ASSIGN(UserSelectionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
