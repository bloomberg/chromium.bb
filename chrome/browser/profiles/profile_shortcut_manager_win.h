// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
#pragma once

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
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
      const string16& profile_base_dir) OVERRIDE;
  virtual void OnProfileRemoved(
      const string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(
      const string16& old_profile_name,
      const string16& new_profile_name) OVERRIDE;

  // Takes a vector of profile names (for example: "Sparky") and generates a
  // vector of shortcut link names (for example: "Chromium (Sparky).lnk").
  static std::vector<string16> GenerateShortcutsFromProfiles(
      const std::vector<string16>& profile_names);

 private:
  // Creates a desktop shortcut to open Chrome with the given profile name and
  // directory. Iff |create|, create shortcut if it doesn't already exist. Must
  // be called on the FILE thread.
  static void CreateChromeDesktopShortcutForProfile(
      const string16& profile_name,
      const string16& directory,
      bool create);

  // Renames an existing Chrome desktop profile shortcut. Must be called on the
  // FILE thread.
  static void RenameChromeDesktopShortcutForProfile(
      const string16& old_profile_name,
      const string16& new_profile_name);

  // Updates the arguments to a Chrome desktop shortcut for a profile. Must be
  // called on the FILE thread.
  static void UpdateChromeDesktopShortcutForProfile(
      const string16& shortcut,
      const string16& arguments);

  DISALLOW_COPY_AND_ASSIGN(ProfileShortcutManagerWin);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
