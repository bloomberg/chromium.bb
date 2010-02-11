// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_SCREEN_H_

#include "views/view.h"

// Interface that defines login wizard screens.
class WizardScreen : public views::View {
 public:
  // Initialization of controls and screen itself.
  virtual void Init() = 0;

  // Updates all localized strings on all controls on the view.
  virtual void UpdateLocalizedStrings() = 0;

 protected:
  virtual ~WizardScreen() {}
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_SCREEN_H_

