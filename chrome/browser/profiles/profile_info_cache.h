// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_info_interface.h"

namespace gfx {
class Image;
}

namespace base {
class DictionaryValue;
}

class PrefService;

// This class saves various information about profiles to local preferences.
// This cache can be used to display a list of profiles without having to
// actually load the profiles from disk.
class ProfileInfoCache : public ProfileInfoInterface,
                         public base::SupportsWeakPtr<ProfileInfoCache> {
 public:
  ProfileInfoCache(PrefService* prefs, const FilePath& user_data_dir);
  virtual ~ProfileInfoCache();

  void AddProfileToCache(const FilePath& profile_path,
                         const string16& name,
                         const string16& username,
                         size_t icon_index);
  void DeleteProfileFromCache(const FilePath& profile_path);

  // ProfileInfoInterface:
  virtual size_t GetNumberOfProfiles() const OVERRIDE;
  // Don't cache this value and reuse, because resorting the menu could cause
  // the item being referred to to change out from under you.
  virtual size_t GetIndexOfProfileWithPath(
      const FilePath& profile_path) const OVERRIDE;
  virtual string16 GetNameOfProfileAtIndex(size_t index) const OVERRIDE;
  virtual FilePath GetPathOfProfileAtIndex(size_t index) const OVERRIDE;
  virtual string16 GetUserNameOfProfileAtIndex(size_t index) const OVERRIDE;
  virtual const gfx::Image& GetAvatarIconOfProfileAtIndex(
      size_t index) const OVERRIDE;
  virtual bool GetBackgroundStatusOfProfileAtIndex(
      size_t index) const OVERRIDE;
  virtual string16 GetGAIANameOfProfileAtIndex(size_t index) const OVERRIDE;
  virtual bool IsUsingGAIANameOfProfileAtIndex(size_t index) const OVERRIDE;
  // Returns the GAIA picture for the given profile. This may return NULL
  // if the profile does not have a GAIA picture or if the picture must be
  // loaded from disk.
  virtual const gfx::Image* GetGAIAPictureOfProfileAtIndex(
      size_t index) const OVERRIDE;
  virtual bool IsUsingGAIAPictureOfProfileAtIndex(
      size_t index) const OVERRIDE;

  size_t GetAvatarIconIndexOfProfileAtIndex(size_t index) const;

  void SetNameOfProfileAtIndex(size_t index, const string16& name);
  void SetUserNameOfProfileAtIndex(size_t index, const string16& user_name);
  void SetAvatarIconOfProfileAtIndex(size_t index, size_t icon_index);
  void SetBackgroundStatusOfProfileAtIndex(size_t index,
                                           bool running_background_apps);
  void SetGAIANameOfProfileAtIndex(size_t index, const string16& name);
  void SetIsUsingGAIANameOfProfileAtIndex(size_t index, bool value);
  void SetGAIAPictureOfProfileAtIndex(size_t index, const gfx::Image* image);
  void SetIsUsingGAIAPictureOfProfileAtIndex(size_t index, bool value);

  // Returns unique name that can be assigned to a newly created profile.
  string16 ChooseNameForNewProfile(size_t icon_index);

  // Checks if the given profile has switched to using GAIA information
  // for the profile name and picture. This pref is used to switch over
  // to GAIA info the first time it is available. Afterwards this pref is
  // checked to prevent clobbering the user's custom settings.
  bool GetHasMigratedToGAIAInfoOfProfileAtIndex(size_t index) const;

  // Marks the given profile as having switched to using GAIA information
  // for the profile name and picture.
  void SetHasMigratedToGAIAInfoOfProfileAtIndex(size_t index, bool value);

  // Returns an avatar icon index that can be assigned to a newly created
  // profile. Note that the icon may not be unique since there are a limited
  // set of default icons.
  size_t ChooseAvatarIconIndexForNewProfile() const;

  const FilePath& GetUserDataDir() const;

  // Gets the number of default avatar icons that exist.
  static size_t GetDefaultAvatarIconCount();
  // Gets the resource ID of the default avatar icon at |index|.
  static int GetDefaultAvatarIconResourceIDAtIndex(size_t index);
  // Returns a URL for the default avatar icon with specified index.
  static std::string GetDefaultAvatarIconUrl(size_t index);
  // Checks if |index| is a valid avatar icon index
  static bool IsDefaultAvatarIconIndex(size_t index);
  // Checks if the given URL points to one of the default avatar icons. If it
  // is, returns true and its index through |icon_index|. If not, returns false.
  static bool IsDefaultAvatarIconUrl(const std::string& icon_url,
                                     size_t *icon_index);

  // Gets all names of profiles associated with this instance of Chrome.
  // Because this method will be called during uninstall, before the creation
  // of the ProfileManager, it reads directly from the local state preferences,
  // rather than going through the ProfileInfoCache object.
  static std::vector<string16> GetProfileNames();

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefService* prefs);

  void AddObserver(ProfileInfoCacheObserver* obs);
  void RemoveObserver(ProfileInfoCacheObserver* obs);

 private:
  const base::DictionaryValue* GetInfoForProfileAtIndex(size_t index) const;
  // Saves the profile info to a cache and takes ownership of |info|.
  // Currently the only information that is cached is the profile's name,
  // user name, and avatar icon.
  void SetInfoForProfileAtIndex(size_t index, base::DictionaryValue* info);
  std::string CacheKeyFromProfilePath(const FilePath& profile_path) const;
  std::vector<std::string>::iterator FindPositionForProfile(
      std::string search_key,
      const string16& search_name);

  // Returns true if the given icon index is not in use by another profie.
  bool IconIndexIsUnique(size_t icon_index) const;

  // Tries to find an icon index that satisfies all the given conditions.
  // Returns true if an icon was found, false otherwise.
  bool ChooseAvatarIconIndexForNewProfile(bool allow_generic_icon,
                                          bool must_be_unique,
                                          size_t* out_icon_index) const;

  // Updates the position of the profile at the given index so that the list
  // of profiles is still sorted.
  void UpdateSortForProfileIndex(size_t index);

  void OnGAIAPictureLoaded(FilePath path, gfx::Image** image) const;
  void OnGAIAPictureSaved(FilePath path, bool* success) const;

  PrefService* prefs_;
  std::vector<std::string> sorted_keys_;
  FilePath user_data_dir_;

  ObserverList<ProfileInfoCacheObserver> observer_list_;

  // A cache of gaia profile pictures. This cache is updated lazily so it needs
  // to be mutable.
  mutable std::map<std::string, gfx::Image*> gaia_pictures_;
  // Marks a gaia profile picture as loading. This prevents a picture from
  // loading multiple times.
  mutable std::map<std::string, bool> gaia_pictures_loading_;

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoCache);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
