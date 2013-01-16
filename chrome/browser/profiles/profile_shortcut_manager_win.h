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

// Name of the badged icon file generated for a given profile.
extern const char kProfileIconFileName[];

// Returns the default shortcut filename for the given profile name,
// given |distribution|. Returns a filename appropriate for a
// single-user installation if |profile_name| is empty.
string16 GetShortcutFilenameForProfile(const string16& profile_name,
                                       BrowserDistribution* distribution);

// Returns the command-line flags to launch Chrome with the given profile.
string16 CreateProfileShortcutFlags(const FilePath& profile_path);

}  // namespace internal
}  // namespace profiles

class ProfileShortcutManagerWin : public ProfileShortcutManager,
                                  public ProfileInfoCacheObserver {
 public:
  // Specifies whether a new shortcut should be created if none exist.
  enum CreateOrUpdateMode {
    UPDATE_EXISTING_ONLY,
    CREATE_WHEN_NONE_FOUND,
  };
  // Specifies whether non-profile shortcuts should be updated.
  enum NonProfileShortcutAction {
    IGNORE_NON_PROFILE_SHORTCUTS,
    UPDATE_NON_PROFILE_SHORTCUTS,
  };

  explicit ProfileShortcutManagerWin(ProfileManager* manager);
  virtual ~ProfileShortcutManagerWin();

  // ProfileShortcutManager implementation:
  virtual void CreateProfileShortcut(const FilePath& profile_path) OVERRIDE;
  virtual void RemoveProfileShortcuts(const FilePath& profile_path) OVERRIDE;
  virtual void HasProfileShortcuts(
      const FilePath& profile_path,
      const base::Callback<void(bool)>& callback) OVERRIDE;

  // ProfileInfoCacheObserver implementation:
  virtual void OnProfileAdded(const FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWillBeRemoved(const FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(const FilePath& profile_path,
                                   const string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(const FilePath& profile_path,
                                    const string16& old_profile_name) OVERRIDE;
  virtual void OnProfileAvatarChanged(const FilePath& profile_path) OVERRIDE;

 private:
  // Gives the profile path of an alternate profile than |profile_path|.
  // Must only be called when the number profiles is 2.
  FilePath GetOtherProfilePath(const FilePath& profile_path);

  void CreateOrUpdateShortcutsForProfileAtPath(const FilePath& profile_path,
                                               CreateOrUpdateMode create_mode,
                                               NonProfileShortcutAction action);

  ProfileManager* profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(ProfileShortcutManagerWin);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_WIN_H_
