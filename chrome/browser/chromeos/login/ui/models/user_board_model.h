// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_MODELS_USER_BOARD_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_MODELS_USER_BOARD_MODEL_H_

#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class UserBoardModel : public BaseScreen {
 public:
  UserBoardModel();
  virtual ~UserBoardModel();

  // Build list of users and send it to the webui.
  virtual void SendUserList() = 0;

  // Methods for easy unlock support.
  virtual void HardLockPod(const std::string& user_id) = 0;
  virtual void AttemptEasyUnlock(const std::string& user_id) = 0;

  // BaseScreen implementation:
  virtual std::string GetName() const override;

  // Temorary unused methods:
  virtual void PrepareToShow() override{};
  virtual void Show() override{};
  virtual void Hide() override{};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_MODELS_USER_BOARD_MODEL_H_
