// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile_info_cache.h"

class ProfileManager;

class ProfileShortcutManager {
 public:
  virtual ~ProfileShortcutManager();

  // Create a profile shortcut for the profile with path |profile_path|, plus
  // update the original profile shortcut if |profile_path| is the second
  // profile created.
  virtual void CreateProfileShortcut(const base::FilePath& profile_path) = 0;

  // Removes any desktop profile shortcuts for the profile corresponding to
  // |profile_path|.
  virtual void RemoveProfileShortcuts(const base::FilePath& profile_path) = 0;

  // Checks if a profile at |profile_path| has any shortcuts and invokes
  // |callback| with the bool result some time later. Does not consider
  // non-profile specific shortcuts.
  virtual void HasProfileShortcuts(
      const base::FilePath& profile_path,
      const base::Callback<void(bool)>& callback) = 0;

  static bool IsFeatureEnabled();
  static ProfileShortcutManager* Create(ProfileManager* manager);

 protected:
  ProfileShortcutManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileShortcutManager);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_H_
