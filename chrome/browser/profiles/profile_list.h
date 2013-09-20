// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_LIST_H_
#define CHROME_BROWSER_PROFILES_PROFILE_LIST_H_

#include "chrome/browser/profiles/avatar_menu.h"

class ProfileInfoInterface;

// This model represents the profiles added to Chrome.
class ProfileList {
 public:
  virtual ~ProfileList() {}

  static ProfileList* Create(ProfileInfoInterface* profile_cache);

  // Returns the number of profiles in the model.
  virtual size_t GetNumberOfItems() const = 0;

  // Returns the Item at the specified index.
  virtual const AvatarMenu::Item& GetItemAt(size_t index) const = 0;

  // Rebuilds the menu from the data source.
  virtual void RebuildMenu() = 0;

  // Returns the index in the menu of the specified profile.
  virtual size_t MenuIndexFromProfileIndex(size_t index) = 0;

  // Updates the path of the active browser's profile.
  virtual void ActiveProfilePathChanged(base::FilePath& path) = 0;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_LIST_H_
