// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/wizard_screen.h"

#include "chrome/browser/chromeos/login/screens/screen_observer.h"

namespace chromeos {

WizardScreen::WizardScreen(ScreenObserver* screen_observer)
  : screen_observer_(screen_observer) {
}

}  // namespace chromeos
