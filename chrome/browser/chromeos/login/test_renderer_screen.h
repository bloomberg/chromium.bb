// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_RENDERER_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_RENDERER_SCREEN_H_

#include "chrome/browser/chromeos/login/wizard_screen.h"

namespace chromeos {
class ScreenObserver;
}  // namespace chromeos

class TestRendererScreen : public WizardScreen {
 public:
  explicit TestRendererScreen(chromeos::ScreenObserver* observer);
  virtual ~TestRendererScreen();

  // WizardScreen implementation:
  void Init();
  void UpdateLocalizedStrings();

  DISALLOW_COPY_AND_ASSIGN(TestRendererScreen);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_RENDERER_SCREEN_H_
