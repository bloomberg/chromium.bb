// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
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
class ProfileInfoCache : public ProfileInfoInterface,
                         public base::SupportsWeakPtr<ProfileInfoCache> {
 public:
  ProfileInfoCache(PrefService* prefs, const base::FilePath& user_data_dir);
  virtual ~ProfileInfoCache();

  // If the |supervised_user_id| is non-empty, the profile will be marked to be
  // omitted from the avatar-menu list on desktop versions. This is used while a
  // supervised user is in the process of being registered with the server. Use
  // SetIsOmittedProfileAtIndex() to clear the flag when the profile is ready to
  // be shown in the menu.
  void AddProfileToCache(const base::FilePath& profile_path,
                         const base::string16& name,
                         const base::string16& username,
                         size_t icon_index,
                         const std::string& supervised_user_id);
  void DeleteProfileFromCache(const base::FilePath& profile_path);

  // ProfileInfoInterface:
  virtual size_t GetNumberOfProfiles() const OVERRIDE;
  // Don't cache this value and reuse, because resorting the menu could cause
  // the item being referred to to change out from under you.
  virtual size_t GetIndexOfProfileWithPath(
      const base::FilePath& profile_path) const OVERRIDE;
  virtual base::string16 GetNameOfProfileAtIndex(size_t index) const OVERRIDE;
  virtual base::string16 GetShortcutNameOfProfileAtIndex(size_t index)
      const OVERRIDE;
  virtual base::FilePath GetPathOfProfileAtIndex(size_t index) const OVERRIDE;
  virtual base::Time GetProfileActiveTimeAtIndex(size_t index) const OVERRIDE;
  virtual base::string16 GetUserNameOfProfileAtIndex(
      size_t index) const OVERRIDE;
  virtual const gfx::Image& GetAvatarIconOfProfileAtIndex(
      size_t index) const OVERRIDE;
  virtual std::string GetLocalAuthCredentialsOfProfileAtIndex(
      size_t index) const OVERRIDE;
  // Note that a return value of false could mean an error in collection or
  // that there are currently no background apps running. However, the action
  // which results is the same in both cases (thus far).
  virtual bool GetBackgroundStatusOfProfileAtIndex(
      size_t index) const OVERRIDE;
  virtual base::string16 GetGAIANameOfProfileAtIndex(
      size_t index) const OVERRIDE;
  virtual base::string16 GetGAIAGivenNameOfProfileAtIndex(
      size_t index) const OVERRIDE;
  // Returns the GAIA picture for the given profile. This may return NULL
  // if the profile does not have a GAIA picture or if the picture must be
  // loaded from disk.
  virtual const gfx::Image* GetGAIAPictureOfProfileAtIndex(
      size_t index) const OVERRIDE;
  virtual bool IsUsingGAIAPictureOfProfileAtIndex(
      size_t index) const OVERRIDE;
  virtual bool ProfileIsSupervisedAtIndex(size_t index) const OVERRIDE;
  virtual bool IsOmittedProfileAtIndex(size_t index) const OVERRIDE;
  virtual bool ProfileIsSigninRequiredAtIndex(size_t index) const OVERRIDE;
  virtual std::string GetSupervisedUserIdOfProfileAtIndex(size_t index) const
      OVERRIDE;
  virtual bool ProfileIsEphemeralAtIndex(size_t index) const OVERRIDE;
  virtual bool ProfileIsUsingDefaultNameAtIndex(size_t index) const OVERRIDE;

  size_t GetAvatarIconIndexOfProfileAtIndex(size_t index) const;

  void SetProfileActiveTimeAtIndex(size_t index);
  // Warning: This will re-sort profiles and thus may change indices!
  void SetNameOfProfileAtIndex(size_t index, const base::string16& name);
  void SetShortcutNameOfProfileAtIndex(size_t index,
                                       const base::string16& name);
  void SetUserNameOfProfileAtIndex(size_t index,
                                   const base::string16& user_name);
  void SetAvatarIconOfProfileAtIndex(size_t index, size_t icon_index);
  void SetIsOmittedProfileAtIndex(size_t index, bool is_omitted);
  void SetSupervisedUserIdOfProfileAtIndex(size_t index, const std::string& id);
  void SetLocalAuthCredentialsOfProfileAtIndex(size_t index,
                                               const std::string& auth);
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

  // Returns unique name that can be assigned to a newly created profile.
  base::string16 ChooseNameForNewProfile(size_t icon_index) const;

  // Returns an avatar icon index that can be assigned to a newly created
  // profile. Note that the icon may not be unique since there are a limited
  // set of default icons.
  size_t ChooseAvatarIconIndexForNewProfile() const;

  const base::FilePath& GetUserDataDir() const;

  // Gets all names of profiles associated with this instance of Chrome.
  // Because this method will be called during uninstall, before the creation
  // of the ProfileManager, it reads directly from the local state preferences,
  // rather than going through the ProfileInfoCache object.
  static std::vector<base::string16> GetProfileNames();

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Starts downloading the high res avatar at index |icon_index| for profile
  // with path |profile_path|.
  void DownloadHighResAvatar(size_t icon_index,
                             const base::FilePath& profile_path);

  // Saves the avatar |image| at |image_path|. This is used both for the
  // GAIA profile pictures and the ProfileAvatarDownloader that is used to
  // download the high res avatars.
  void SaveAvatarImageAtPath(const gfx::Image* image,
                             const std::string& key,
                             const base::FilePath& image_path,
                             const base::FilePath& profile_path);

  void AddObserver(ProfileInfoCacheObserver* obs);
  void RemoveObserver(ProfileInfoCacheObserver* obs);

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest, DownloadHighResAvatarTest);

  const base::DictionaryValue* GetInfoForProfileAtIndex(size_t index) const;
  // Saves the profile info to a cache and takes ownership of |info|.
  // Currently the only information that is cached is the profile's name,
  // user name, and avatar icon.
  void SetInfoQuietlyForProfileAtIndex(size_t index,
                                       base::DictionaryValue* info);
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

  // Returns the decoded image at |image_path|. Used both by the GAIA profile
  // image and the high res avatars.
  const gfx::Image* LoadAvatarPictureFromPath(
      const std::string& key,
      const base::FilePath& image_path) const;

  // Called when the picture given by |key| has been loaded from disk and
  // decoded into |image|.
  void OnAvatarPictureLoaded(const std::string& key,
                             gfx::Image** image) const;
  // Called when the picture given by |file_name| has been saved to disk.
  // Used both for the GAIA profile picture and the high res avatar files.
  void OnAvatarPictureSaved(const std::string& file_name,
                            const base::FilePath& profile_path);

  PrefService* prefs_;
  std::vector<std::string> sorted_keys_;
  base::FilePath user_data_dir_;

  ObserverList<ProfileInfoCacheObserver> observer_list_;

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
  mutable std::map<std::string, ProfileAvatarDownloader*>
      avatar_images_downloads_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoCache);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
