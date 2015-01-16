// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_MODEL_H_

#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace base {
class ListValue;
}

namespace chromeos {

class BaseScreenDelegate;
class NetworkView;

class NetworkModel : public BaseScreen {
 public:
  static const char kUserActionContinueButtonClicked[];
  static const char kUserActionConnectDebuggingFeaturesClicked[];
  static const char kContextKeyLocale[];
  static const char kContextKeyInputMethod[];
  static const char kContextKeyTimezone[];
  static const char kContextKeyContinueButtonEnabled[];

  explicit NetworkModel(BaseScreenDelegate* base_screen_delegate);
  ~NetworkModel() override;

  // BaseScreen implementation:
  std::string GetName() const override;

  // This method is called, when view is being destroyed. Note, if model
  // is destroyed earlier then it has to call Unbind().
  virtual void OnViewDestroyed(NetworkView* view) = 0;

  virtual std::string GetLanguageListLocale() const = 0;

  virtual const base::ListValue* GetLanguageList() const = 0;

  virtual void UpdateLanguageList() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_MODEL_H_
