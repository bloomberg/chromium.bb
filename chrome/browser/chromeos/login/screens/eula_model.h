// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_MODEL_H_

#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "url/gurl.h"

namespace chromeos {

class BaseScreenDelegate;
class EulaView;

// Allows us to get info from eula screen that we need.
class EulaModel : public BaseScreen {
 public:
  static const char kUserActionAcceptButtonClicked[];
  static const char kUserActionBackButtonClicked[];
  static const char kContextKeyUsageStatsEnabled[];

  explicit EulaModel(BaseScreenDelegate* base_screen_delegate);
  ~EulaModel() override;

  // BaseScreen implementation:
  std::string GetName() const override;

  // Returns URL of the OEM EULA page that should be displayed using current
  // locale and manifest. Returns empty URL otherwise.
  virtual GURL GetOemEulaUrl() const = 0;

  // Initiate TPM password fetch. Will call actor's OnPasswordFetched() when
  // done.
  virtual void InitiatePasswordFetch() = 0;

  // Returns true if usage statistics reporting is enabled.
  virtual bool IsUsageStatsEnabled() const = 0;

  // This method is called, when view is being destroyed. Note, if model
  // is destroyed earlier then it has to call SetModel(NULL).
  virtual void OnViewDestroyed(EulaView* view) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_MODEL_H_
