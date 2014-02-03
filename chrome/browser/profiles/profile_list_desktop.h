// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_LIST_DESKTOP_H_
#define CHROME_BROWSER_PROFILES_PROFILE_LIST_DESKTOP_H_

#include "chrome/browser/profiles/profile_list.h"

#include <vector>

class Browser;
class ProfileInfoInterface;

// This model represents the profiles added to desktop Chrome (as opposed to
// Chrome OS). Profiles marked not to appear in the list will be omitted
// throughout.
class ProfileListDesktop : public ProfileList {
 public:
  explicit ProfileListDesktop(ProfileInfoInterface* profile_cache);
  virtual ~ProfileListDesktop();

  // ProfileList overrides:
  virtual size_t GetNumberOfItems() const OVERRIDE;
  virtual const AvatarMenu::Item& GetItemAt(size_t index) const OVERRIDE;
  virtual void RebuildMenu() OVERRIDE;
  // Returns the menu index of the profile at |index| in the ProfileInfoCache.
  // The profile index must exist, and it may not be marked as omitted from the
  // menu.
  virtual size_t MenuIndexFromProfileIndex(size_t index) OVERRIDE;
  virtual void ActiveProfilePathChanged(base::FilePath& path) OVERRIDE;

 private:
  void ClearMenu();

  // The cache that provides the profile information. Weak.
  ProfileInfoInterface* profile_info_;

  // The path of the currently active profile.
  base::FilePath active_profile_path_;

  // List of built "menu items."
  std::vector<AvatarMenu::Item*> items_;

  // The number of profiles that were omitted from the list when it was built.
  size_t omitted_item_count_;

  DISALLOW_COPY_AND_ASSIGN(ProfileListDesktop);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_LIST_DESKTOP_H_
