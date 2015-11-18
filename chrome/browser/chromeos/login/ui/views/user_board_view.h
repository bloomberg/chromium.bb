// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_VIEWS_USER_BOARD_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_VIEWS_USER_BOARD_VIEW_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "components/proximity_auth/screenlock_bridge.h"

namespace chromeos {

class UserBoardModel;

// Interface between user board screen and its representation, either WebUI
// or Views one.
class UserBoardView {
 public:
  virtual ~UserBoardView() {}

  virtual void Bind(UserBoardModel& model) = 0;
  virtual void Unbind() = 0;

  virtual void SetPublicSessionDisplayName(const std::string& user_id,
                                           const std::string& display_name) = 0;
  virtual void SetPublicSessionLocales(const std::string& user_id,
                                       scoped_ptr<base::ListValue> locales,
                                       const std::string& default_locale,
                                       bool multiple_recommended_locales) = 0;
  virtual void ShowBannerMessage(const base::string16& message) = 0;
  virtual void ShowUserPodCustomIcon(const std::string& user_id,
                                     const base::DictionaryValue& icon) = 0;
  virtual void HideUserPodCustomIcon(const std::string& user_id) = 0;
  virtual void SetAuthType(
      const std::string& user_id,
      proximity_auth::ScreenlockBridge::LockHandler::AuthType auth_type,
      const base::string16& initial_value) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_VIEWS_USER_BOARD_VIEW_H_
