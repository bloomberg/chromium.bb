// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  // ProfileInfoCacheObserver:
  virtual void OnProfileAdded(
      const string16& profile_name,
      const string16& profile_base_dir,
      const FilePath& profile_path,
      const gfx::Image* avatar_image) OVERRIDE;
  virtual void OnProfileRemoved(
      const string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(
      const string16& old_profile_name,
      const string16& new_profile_name) OVERRIDE;
  virtual void OnProfileAvatarChanged(
      const string16& profile_name,
      const string16& profile_base_dir,
      const FilePath& profile_path,
      const gfx::Image* avatar_image) OVERRIDE;

  // Takes a vector of profile names (for example: "Sparky") and generates a
  // vector of shortcut link names (for example: "Chromium (Sparky).lnk").
  static std::vector<string16> GenerateShortcutsFromProfiles(
      const std::vector<string16>& profile_names);

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileShortcutManagerWin);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
