// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"

// This class observes the ProfileInfoCache, and makes corresponding changes
// to shortcuts on the user's desktop in Windows systems.
class ProfileShortcutManagerWin : public ProfileInfoCacheObserver {
 public:
  ProfileShortcutManagerWin();
  virtual ~ProfileShortcutManagerWin();

  // Create a profile shortcut for the profile with path |profile_path|, plus
  // update the original profile shortcut if |profile_path| is the second
  // profile created.
  virtual void AddProfileShortcut(const FilePath& profile_path);

  // ProfileInfoCacheObserver:
  virtual void OnProfileAdded(const FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWillBeRemoved(
      const FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(
      const FilePath& profile_path,
      const string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(
      const FilePath& profile_path,
      const string16& old_profile_name) OVERRIDE;
  virtual void OnProfileAvatarChanged(const FilePath& profile_path) OVERRIDE;

  // Takes a vector of profile names (for example: "Sparky") and generates a
  // vector of shortcut link names (for example: "Chromium (Sparky).lnk").
  static std::vector<string16> GenerateShortcutsFromProfiles(
      const std::vector<string16>& profile_names);

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileShortcutManagerWin);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
