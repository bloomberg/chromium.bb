// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_BOARD_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_BOARD_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/ui/views/user_board_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class SigninScreenHandler;

// A class that handles WebUI hooks in Gaia screen.
class UserBoardScreenHandler : public BaseScreenHandler, public UserBoardView {
 public:
  UserBoardScreenHandler();
  ~UserBoardScreenHandler() override;

 private:
  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;
  void Initialize() override;

  // Handlers
  void HandleGetUsers();
  void HandleHardlockPod(const std::string& user_id);
  void HandleAttemptUnlock(const std::string& user_id);
  void HandleRecordClickOnLockIcon(const std::string& user_id);

  // UserBoardView implementation:
  void SetPublicSessionDisplayName(const std::string& user_id,
                                   const std::string& display_name) override;
  void SetPublicSessionLocales(const std::string& user_id,
                               scoped_ptr<base::ListValue> locales,
                               const std::string& default_locale,
                               bool multiple_recommended_locales) override;
  void ShowBannerMessage(const base::string16& message) override;
  void ShowUserPodCustomIcon(const std::string& user_id,
                             const base::DictionaryValue& icon) override;
  void HideUserPodCustomIcon(const std::string& user_id) override;
  void SetAuthType(const std::string& user_id,
                   ScreenlockBridge::LockHandler::AuthType auth_type,
                   const base::string16& initial_value) override;

  void Bind(UserBoardModel& model) override;
  void Unbind() override;

  UserBoardModel* model_;

  DISALLOW_COPY_AND_ASSIGN(UserBoardScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_BOARD_SCREEN_HANDLER_H_
