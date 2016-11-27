// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_MODEL_H_

#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class BaseScreenDelegate;
class UpdateView;

class UpdateModel : public BaseScreen {
 public:
  static const char kUserActionCancelUpdateShortcut[];
  static const char kContextKeyEstimatedTimeLeftSec[];
  static const char kContextKeyShowEstimatedTimeLeft[];
  static const char kContextKeyUpdateMessage[];
  static const char kContextKeyShowCurtain[];
  static const char kContextKeyShowProgressMessage[];
  static const char kContextKeyProgress[];
  static const char kContextKeyProgressMessage[];
  static const char kContextKeyCancelUpdateShortcutEnabled[];

  explicit UpdateModel(BaseScreenDelegate* base_screen_delegate);
  ~UpdateModel() override;

  // BaseScreen implementation:
  std::string GetName() const override;

  // This method is called, when view is being destroyed. Note, if model
  // is destroyed earlier then it has to call Unbind().
  virtual void OnViewDestroyed(UpdateView* view) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_MODEL_H_
