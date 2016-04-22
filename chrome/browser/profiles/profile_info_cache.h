// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_info_interface.h"

namespace gfx {
class Image;
}

namespace base {
class DictionaryValue;
}

class PrefService;
class PrefRegistrySimple;
class ProfileAvatarDownloader;

// This class saves various information about profiles to local preferences.
// This cache can be used to display a list of profiles without having to
// actually load the profiles from disk.
// The ProfileInfoInterface is being deprecated. Prefer using the
// ProfileAttributesStorage and avoid using the Get*AtIndex family of functions.
class ProfileInfoCache : public ProfileInfoInterface,
                         public ProfileAttributesStorage,
                         public base::SupportsWeakPtr<ProfileInfoCache> {
 public:
  ProfileInfoCache(PrefService* prefs, const base::FilePath& user_data_dir);
  ~ProfileInfoCache() override;

  // If the |supervised_user_id| is non-empty, the profile will be marked to be
  // omitted from the avatar-menu list on desktop versions. This is used while a
  // supervised user is in the process of being registered with the server. Use
  // SetIsOmittedProfileAtIndex() to clear the flag when the profile is ready to
  // be shown in the menu.
  // Deprecated. Use AddProfile instead.
  void AddProfileToCache(const base::FilePath& profile_path,
                         const base::string16& name,
                         const std::string& gaia_id,
                         const base::string16& user_name,
                         size_t icon_index,
                         const std::string& supervised_user_id);
  // Deprecated. Use RemoveProfile instead.
  void DeleteProfileFromCache(const base::FilePath& profile_path);

  // ProfileInfoInterface:
  size_t GetNumberOfProfiles() const override;
  // Don't cache this value and reuse, because resorting the menu could cause
  // the item being referred to to change out from under you.
  // Deprecated. Prefer using the ProfileAttributesStorage interface instead of
  // directly referring to this implementation.
  size_t GetIndexOfProfileWithPath(
      const base::FilePath& profile_path) const override;
  base::string16 GetNameOfProfileAtIndex(size_t index) const override;
  base::string16 GetShortcutNameOfProfileAtIndex(size_t index) const override;
  base::FilePath GetPathOfProfileAtIndex(size_t index) const override;
  base::Time GetProfileActiveTimeAtIndex(size_t index) const override;
  base::string16 GetUserNameOfProfileAtIndex(size_t index) const override;
  const gfx::Image& GetAvatarIconOfProfileAtIndex(size_t index) const override;
  std::string GetLocalAuthCredentialsOfProfileAtIndex(
      size_t index) const override;
  std::string GetPasswordChangeDetectionTokenAtIndex(
      size_t index) const override;
  // Note that a return value of false could mean an error in collection or
  // that there are currently no background apps running. However, the action
  // which results is the same in both cases (thus far).
  bool GetBackgroundStatusOfProfileAtIndex(size_t index) const override;
  base::string16 GetGAIANameOfProfileAtIndex(size_t index) const override;
  base::string16 GetGAIAGivenNameOfProfileAtIndex(size_t index) const override;
  std::string GetGAIAIdOfProfileAtIndex(size_t index) const override;
  // Returns the GAIA picture for the given profile. This may return NULL
  // if the profile does not have a GAIA picture or if the picture must be
  // loaded from disk.
  const gfx::Image* GetGAIAPictureOfProfileAtIndex(size_t index) const override;
  bool IsUsingGAIAPictureOfProfileAtIndex(size_t index) const override;
  bool ProfileIsSupervisedAtIndex(size_t index) const override;
  bool ProfileIsChildAtIndex(size_t index) const override;
  bool ProfileIsLegacySupervisedAtIndex(size_t index) const override;
  bool IsOmittedProfileAtIndex(size_t index) const override;
  bool ProfileIsSigninRequiredAtIndex(size_t index) const override;
  std::string GetSupervisedUserIdOfProfileAtIndex(size_t index) const override;
  bool ProfileIsEphemeralAtIndex(size_t index) const override;
  bool ProfileIsUsingDefaultNameAtIndex(size_t index) const override;
  bool ProfileIsAuthenticatedAtIndex(size_t index) const override;
  bool ProfileIsUsingDefaultAvatarAtIndex(size_t index) const override;
  bool ProfileIsAuthErrorAtIndex(size_t index) const;

  size_t GetAvatarIconIndexOfProfileAtIndex(size_t index) const;

  // Statistics
  bool HasStatsBrowsingHistoryOfProfileAtIndex(size_t index) const;
  int GetStatsBrowsingHistoryOfProfileAtIndex(size_t index) const;
  bool HasStatsPasswordsOfProfileAtIndex(size_t index) const;
  int GetStatsPasswordsOfProfileAtIndex(size_t index) const;
  bool HasStatsBookmarksOfProfileAtIndex(size_t index) const;
  int GetStatsBookmarksOfProfileAtIndex(size_t index) const;
  bool HasStatsSettingsOfProfileAtIndex(size_t index) const;
  int GetStatsSettingsOfProfileAtIndex(size_t index) const;

  void SetProfileActiveTimeAtIndex(size_t index);
  // Warning: This will re-sort profiles and thus may change indices!
  void SetNameOfProfileAtIndex(size_t index, const base::string16& name);
  void SetShortcutNameOfProfileAtIndex(size_t index,
                                       const base::string16& name);
  void SetAuthInfoOfProfileAtIndex(size_t index,
                                   const std::string& gaia_id,
                                   const base::string16& user_name);
  void SetAvatarIconOfProfileAtIndex(size_t index, size_t icon_index);
  void SetIsOmittedProfileAtIndex(size_t index, bool is_omitted);
  void SetSupervisedUserIdOfProfileAtIndex(size_t index, const std::string& id);
  void SetLocalAuthCredentialsOfProfileAtIndex(size_t index,
                                               const std::string& auth);
  void SetPasswordChangeDetectionTokenAtIndex(size_t index,
                                              const std::string& token);
  void SetBackgroundStatusOfProfileAtIndex(size_t index,
                                           bool running_background_apps);
  // Warning: This will re-sort profiles and thus may change indices!
  void SetGAIANameOfProfileAtIndex(size_t index, const base::string16& name);
  // Warning: This will re-sort profiles and thus may change indices!
  void SetGAIAGivenNameOfProfileAtIndex(size_t index,
                                        const base::string16& name);
  void SetGAIAPictureOfProfileAtIndex(size_t index, const gfx::Image* image);
  void SetIsUsingGAIAPictureOfProfileAtIndex(size_t index, bool value);
  void SetProfileSigninRequiredAtIndex(size_t index, bool value);
  void SetProfileIsEphemeralAtIndex(size_t index, bool value);
  void SetProfileIsUsingDefaultNameAtIndex(size_t index, bool value);
  void SetProfileIsUsingDefaultAvatarAtIndex(size_t index, bool value);
  void SetProfileIsAuthErrorAtIndex(size_t index, bool value);

  // Determines whether |name| is one of the default assigned names.
  bool IsDefaultProfileName(const base::string16& name) const;

  // Returns unique name that can be assigned to a newly created profile.
  base::string16 ChooseNameForNewProfile(size_t icon_index) const override;

  // Returns an avatar icon index that can be assigned to a newly created
  // profile. Note that the icon may not be unique since there are a limited
  // set of default icons.
  size_t ChooseAvatarIconIndexForNewProfile() const override;

  // Statistics
  void SetStatsBrowsingHistoryOfProfileAtIndex(size_t index, int value);
  void SetStatsPasswordsOfProfileAtIndex(size_t index, int value);
  void SetStatsBookmarksOfProfileAtIndex(size_t index, int value);
  void SetStatsSettingsOfProfileAtIndex(size_t index, int value);

  const base::FilePath& GetUserDataDir() const;

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Checks whether the high res avatar at index |icon_index| exists, and
  // if it does not, calls |DownloadHighResAvatar|.
  void DownloadHighResAvatarIfNeeded(size_t icon_index,
                                     const base::FilePath& profile_path);

  // Saves the avatar |image| at |image_path|. This is used both for the
  // GAIA profile pictures and the ProfileAvatarDownloader that is used to
  // download the high res avatars.
  void SaveAvatarImageAtPath(const base::FilePath& profile_path,
                             const gfx::Image* image,
                             const std::string& key,
                             const base::FilePath& image_path);

  void AddObserver(ProfileInfoCacheObserver* obs) override;
  void RemoveObserver(ProfileInfoCacheObserver* obs) override;

  void set_disable_avatar_download_for_testing(
      bool disable_avatar_download_for_testing) {
    disable_avatar_download_for_testing_ = disable_avatar_download_for_testing;
  }

  // ProfileAttributesStorage:
  void AddProfile(const base::FilePath& profile_path,
                  const base::string16& name,
                  const std::string& gaia_id,
                  const base::string16& user_name,
                  size_t icon_index,
                  const std::string& supervised_user_id) override;
  void RemoveProfile(const base::FilePath& profile_path) override;
  // Returns a vector containing one attributes entry per known profile. They
  // are not sorted in any particular order.
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributes() override;
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributesSortedByName()
      override;
  bool GetProfileAttributesWithPath(
      const base::FilePath& path,
      ProfileAttributesEntry** entry) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest, DownloadHighResAvatarTest);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest,
                           NothingToDownloadHighResAvatarTest);

  const base::DictionaryValue* GetInfoForProfileAtIndex(size_t index) const;
  // Saves the profile info to a cache and takes ownership of |info|.
  void SetInfoForProfileAtIndex(size_t index, base::DictionaryValue* info);
  std::string CacheKeyFromProfilePath(const base::FilePath& profile_path) const;
  std::vector<std::string>::iterator FindPositionForProfile(
      const std::string& search_key,
      const base::string16& search_name);

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

  // Loads or uses an already loaded high resolution image of the
  // generic profile avatar.
  const gfx::Image* GetHighResAvatarOfProfileAtIndex(size_t index) const;

  // Starts downloading the high res avatar at index |icon_index| for profile
  // with path |profile_path|.
  void DownloadHighResAvatar(size_t icon_index,
                             const base::FilePath& profile_path);

  // Returns the decoded image at |image_path|. Used both by the GAIA profile
  // image and the high res avatars.
  const gfx::Image* LoadAvatarPictureFromPath(
      const base::FilePath& profile_path,
      const std::string& key,
      const base::FilePath& image_path) const;

  // Called when the picture given by |key| has been loaded from disk and
  // decoded into |image|.
  void OnAvatarPictureLoaded(const base::FilePath& profile_path,
                             const std::string& key,
                             gfx::Image** image) const;
  // Called when the picture given by |file_name| has been saved to disk.
  // Used both for the GAIA profile picture and the high res avatar files.
  void OnAvatarPictureSaved(const std::string& file_name,
                            const base::FilePath& profile_path);

  // Migrate any legacy profile names ("First user", "Default Profile") to
  // new style default names ("Person 1"), and download and high-res avatars
  // used by the profiles.
  void MigrateLegacyProfileNamesAndDownloadAvatars();

  PrefService* prefs_;
  std::vector<std::string> sorted_keys_;
  base::ScopedPtrHashMap<base::FilePath,
                         std::unique_ptr<ProfileAttributesEntry>>
      profile_attributes_entries_;
  base::FilePath user_data_dir_;

  mutable base::ObserverList<ProfileInfoCacheObserver> observer_list_;

  // A cache of gaia/high res avatar profile pictures. This cache is updated
  // lazily so it needs to be mutable.
  mutable std::map<std::string, gfx::Image*> cached_avatar_images_;
  // Marks a profile picture as loading from disk. This prevents a picture from
  // loading multiple times.
  mutable std::map<std::string, bool> cached_avatar_images_loading_;

  // Map of profile pictures currently being downloaded from the remote
  // location and the ProfileAvatarDownloader instances downloading them.
  // This prevents a picture from being downloaded multiple times. The
  // ProfileAvatarDownloader instances are deleted when the download completes
  // or when the ProfileInfoCache is destroyed.
  std::map<std::string, ProfileAvatarDownloader*>
      avatar_images_downloads_in_progress_;

  // Determines of the ProfileAvatarDownloader should be created and executed
  // or not. Only set to true for tests.
  bool disable_avatar_download_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoCache);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
