// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_LIST_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_LIST_CHROMEOS_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/profile_list.h"

class ProfileAttributesStorage;

namespace chromeos {

// This model represents profiles corresponding to logged-in ChromeOS users.
class ProfileListChromeOS : public ProfileList {
 public:
  explicit ProfileListChromeOS(ProfileAttributesStorage* profile_storage);
  ~ProfileListChromeOS() override;

 private:
  // ProfileList overrides:
  size_t GetNumberOfItems() const override;
  const AvatarMenu::Item& GetItemAt(size_t index) const override;
  void RebuildMenu() override;
  size_t MenuIndexFromProfilePath(const base::FilePath& path) const override;
  void ActiveProfilePathChanged(
      const base::FilePath& active_profile_path) override;

  // The storage that provides the profile attributes. Not owned.
  ProfileAttributesStorage* profile_storage_;

  // The path of the currently active profile.
  base::FilePath active_profile_path_;

  // List of built "menu items."
  std::vector<std::unique_ptr<AvatarMenu::Item>> items_;

  DISALLOW_COPY_AND_ASSIGN(ProfileListChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_LIST_CHROMEOS_H_
