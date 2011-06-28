// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string16.h"

namespace gfx {
class Image;
}

class DictionaryValue;
class PrefService;


// This class saves various information about profiles to local preferences.
// This cache can be used to display a list of profiles without having to
// actually load the profiles from disk.
class ProfileInfoCache {
 public:
  ProfileInfoCache(PrefService* prefs, const FilePath& user_data_dir);
  ~ProfileInfoCache();

  void AddProfileToCache(const FilePath& profile_path,
                         const string16& name,
                         size_t icon_index);
  void DeleteProfileFromCache(const FilePath& profile_path);

  size_t GetNumberOfProfiles() const;
  string16 GetNameOfProfileAtIndex(size_t index) const;
  FilePath GetPathOfProfileAtIndex(size_t index) const;
  const gfx::Image& GetAvatarIconOfProfileAtIndex(size_t index) const;

  void SetNameOfProfileAtIndex(size_t index, const string16& name);
  void SetAvatarIconOfProfileAtIndex(size_t index, size_t icon_index);

  // Gets the number of default avatar icons that exist.
  static size_t GetDefaultAvatarIconCount();
  // Gets the resource ID of the default avatar icon at |index|.
  static int GetDefaultAvatarIconResourceIDAtIndex(size_t index);

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefService* prefs);

 private:
  const DictionaryValue* GetInfoForProfileAtIndex(size_t index) const;
  // Saves the profile info to a cache and takes ownership of |info|.
  // Currently the only information that is cached is the profiles name and
  // avatar icon.
  void SetInfoForProfileAtIndex(size_t index, DictionaryValue* info);
  std::string CacheKeyFromProfilePath(const FilePath& profile_path) const;
  std::vector<std::string>::iterator FindPositionForProfile(
      std::string search_key,
      const string16& search_name);

  PrefService* prefs_;
  std::vector<std::string> sorted_keys_;
  FilePath user_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoCache);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
