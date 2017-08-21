// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"

class PrefService;
class ProfileAttributesEntry;

class ProfileAttributesStorage {
 public:
  using Observer = ProfileInfoCacheObserver;

  ProfileAttributesStorage(PrefService* prefs,
                           const base::FilePath& user_data_dir);
  virtual ~ProfileAttributesStorage();

  // If the |supervised_user_id| is non-empty, the profile will be marked to be
  // omitted from the avatar-menu list on desktop versions. This is used while a
  // supervised user is in the process of being registered with the server. Use
  // ProfileAttributesEntry::SetIsOmitted() to clear the flag when the profile
  // is ready to be shown in the menu.
  virtual void AddProfile(const base::FilePath& profile_path,
                          const base::string16& name,
                          const std::string& gaia_id,
                          const base::string16& user_name,
                          size_t icon_index,
                          const std::string& supervised_user_id) = 0;
  // Removes the profile at |profile_path| from this storage. Does not delete or
  // affect the actual profile's data.
  virtual void RemoveProfile(const base::FilePath& profile_path) = 0;

  // Returns a vector containing one attributes entry per known profile. They
  // are not sorted in any particular order.
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributes();
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributesSortedByName();

  // Populates |entry| with the data for the profile at |path| and returns true
  // if the operation is successful and |entry| can be used. Returns false
  // otherwise.
  // |entry| should not be cached as it may not reflect subsequent changes to
  // the profile's metadata.
  virtual bool GetProfileAttributesWithPath(
      const base::FilePath& path, ProfileAttributesEntry** entry) = 0;

  // Returns the count of known profiles.
  virtual size_t GetNumberOfProfiles() const = 0;

  // Returns a unique name that can be assigned to a newly created profile.
  base::string16 ChooseNameForNewProfile(size_t icon_index) const;

  // Determines whether |name| is one of the default assigned names.
  bool IsDefaultProfileName(const base::string16& name) const;

  // Returns an avatar icon index that can be assigned to a newly created
  // profile. Note that the icon may not be unique since there are a limited
  // set of default icons.
  size_t ChooseAvatarIconIndexForNewProfile() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest, EntriesInAttributesStorage);

  PrefService* prefs_;
  base::FilePath user_data_dir_;
  mutable std::unordered_map<base::FilePath::StringType,
                             std::unique_ptr<ProfileAttributesEntry>>
      profile_attributes_entries_;

  mutable base::ObserverList<Observer> observer_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileAttributesStorage);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_
