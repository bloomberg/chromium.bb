// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_MODEL_H_

#include <string>

#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class BaseScreenDelegate;
class ResetView;

class ResetModel : public BaseScreen {
 public:
  static const char kUserActionCancelReset[];
  static const char kUserActionResetRestartPressed[];
  static const char kUserActionResetPowerwashPressed[];
  static const char kUserActionResetLearnMorePressed[];
  static const char kUserActionResetRollbackToggled[];
  static const char kUserActionResetShowConfirmationPressed[];
  static const char kUserActionResetResetConfirmationDismissed[];
  static const char kContextKeyIsRestartRequired[];
  static const char kContextKeyIsRollbackAvailable[];
  static const char kContextKeyIsRollbackChecked[];
  static const char kContextKeyIsConfirmational[];
  static const char kContextKeyIsOfficialBuild[];
  static const char kContextKeyScreenState[];

  explicit ResetModel(BaseScreenDelegate* base_screen_delegate);
  ~ResetModel() override;

  // BaseScreen implementation:
  std::string GetName() const override;

  // Called when actor is destroyed so there's no dead reference to it.
  virtual void OnViewDestroyed(ResetView* view) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_MODEL_H_
