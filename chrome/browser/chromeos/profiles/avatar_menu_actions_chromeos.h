// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROFILES_AVATAR_MENU_ACTIONS_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_PROFILES_AVATAR_MENU_ACTIONS_CHROMEOS_H_

#include "chrome/browser/profiles/avatar_menu_actions.h"

#include <string>

class Browser;

namespace chromeos {

// This interface controls the behavior of avatar menu actions on ChromeOS.
class AvatarMenuActionsChromeOS : public AvatarMenuActions {
 public:
  AvatarMenuActionsChromeOS();
  ~AvatarMenuActionsChromeOS() override;

  // AvatarMenuActions overrides:
  void AddNewProfile(ProfileMetrics::ProfileAdd type) override;
  void EditProfile(Profile* profile, size_t index) override;
  bool ShouldShowAddNewProfileLink() const override;
  bool ShouldShowEditProfileLink() const override;
  void ActiveBrowserChanged(Browser* browser) override;

 private:
  // Browser in which this avatar menu resides. Weak.
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuActionsChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROFILES_AVATAR_MENU_ACTIONS_CHROMEOS_H_
