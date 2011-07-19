// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_SCREEN_H_
#pragma once

namespace chromeos {

class ScreenObserver;

// Base class for the OOBE screens.
class WizardScreen {
 public:
  explicit WizardScreen(ScreenObserver* screen_observer);
  virtual ~WizardScreen() {}

  // Called before showing the screen. It is the right moment for the
  // screen's actor to pass the information to the corresponding OobeDisplay.
  virtual void PrepareToShow() = 0;
  // Makes wizard screen visible.
  virtual void Show() = 0;
  // Makes wizard screen invisible.
  virtual void Hide() = 0;

 protected:
  ScreenObserver* get_screen_observer() const {
    return screen_observer_;
  }

 private:
  friend class NetworkScreenTest;
  friend class UpdateScreenTest;

  ScreenObserver* screen_observer_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_SCREEN_H_
