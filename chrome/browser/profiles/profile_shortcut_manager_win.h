// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_

#include "chrome/browser/profiles/profile_shortcut_manager.h"

class BrowserDistribution;

// Internal free-standing functions that are exported here for testing.
namespace profiles {
namespace internal {

// Returns the default shortcut filename for the given profile name,
// given |distribution|. Returns a filename appropriate for a
// single-user installation if |profile_name| is empty.
string16 GetShortcutFilenameForProfile(const string16& profile_name,
                                       BrowserDistribution* distribution);

}  // namespace internal
}  // namespace profiles

class ProfileShortcutManagerWin : public ProfileShortcutManager,
                                  public ProfileInfoCacheObserver {
 public:
  explicit ProfileShortcutManagerWin(ProfileManager* manager);
  virtual ~ProfileShortcutManagerWin();

  // ProfileShortcutManager implementation:
  virtual void CreateProfileShortcut(const FilePath& profile_path) OVERRIDE;

  // ProfileInfoCacheObserver implementation:
  virtual void OnProfileAdded(const FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWillBeRemoved(const FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(const FilePath& profile_path,
                                   const string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(const FilePath& profile_path,
                                    const string16& old_profile_name) OVERRIDE;
  virtual void OnProfileAvatarChanged(const FilePath& profile_path) OVERRIDE;

 private:
  void StartProfileShortcutNameChange(const FilePath& profile_path,
                                      const string16& old_profile_name);
  // Gives the profile path of an alternate profile than |profile_path|.
  // Must only be called when the number profiles is 2.
  FilePath GetOtherProfilePath(const FilePath& profile_path);
  void UpdateShortcutsForProfileAtPath(const FilePath& profile_path,
                                       bool create_always);

  ProfileManager* profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(ProfileShortcutManagerWin);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
