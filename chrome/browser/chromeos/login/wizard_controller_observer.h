// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_OBSERVER_H_
#pragma once

namespace chromeos {

// Observer of WizardController screen changes.
class WizardControllerObserver {
 public:
  // Called before a screen change happens.
  void OnScreenChanged(const std::string& old_screen_name,
                       const std::string& new_screen_name) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_OBSERVER_H_
