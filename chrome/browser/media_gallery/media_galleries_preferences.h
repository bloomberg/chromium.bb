// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERIES_PREFERENCES_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERIES_PREFERENCES_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class PrefRegistrySyncable;
class Profile;

namespace base {
class DictionaryValue;
}

namespace extensions {
class Extension;
class ExtensionPrefs;
}

namespace chrome {

typedef uint64 MediaGalleryPrefId;
const MediaGalleryPrefId kInvalidMediaGalleryPrefId = 0;

struct MediaGalleryPermission {
  MediaGalleryPrefId pref_id;
  bool has_permission;
};

struct MediaGalleryPrefInfo {
  enum Type {
    kAutoDetected,  // Auto added to the list of galleries.
    kUserAdded,     // Explicitly added by the user.
    kBlackListed,   // Auto added but then removed by the user.
    kInvalidType,
  };

  MediaGalleryPrefInfo();
  ~MediaGalleryPrefInfo();

  // The absolute path of the gallery.
  base::FilePath AbsolutePath() const;

  // The ID that identifies this gallery in this Profile.
  MediaGalleryPrefId pref_id;

  // The user-visible name of this gallery.
  string16 display_name;

  // A string which uniquely and persistently identifies the device that the
  // gallery lives on.
  std::string device_id;

  // The root of the gallery, relative to the root of the device.
  base::FilePath path;

  // The type of gallery.
  Type type;
};

typedef std::map<MediaGalleryPrefId, MediaGalleryPrefInfo>
    MediaGalleriesPrefInfoMap;
typedef std::set<MediaGalleryPrefId> MediaGalleryPrefIdSet;

// A class to manage the media gallery preferences.  There is one instance per
// user profile.
class MediaGalleriesPreferences : public ProfileKeyedService {
 public:
  class GalleryChangeObserver {
    public:
     virtual void OnGalleryChanged(MediaGalleriesPreferences* pref) {}

    protected:
     virtual ~GalleryChangeObserver();
  };

  explicit MediaGalleriesPreferences(Profile* profile);
  virtual ~MediaGalleriesPreferences();

  void AddGalleryChangeObserver(GalleryChangeObserver* observer);
  void RemoveGalleryChangeObserver(GalleryChangeObserver* observer);

  // Lookup a media gallery and fill in information about it and return true.
  // If the media gallery does not already exist, fill in as much of the
  // MediaGalleryPrefInfo struct as we can and return false.
  // TODO(vandebo) figure out if we want this to be async, in which case:
  // void LookUpGalleryByPath(base::FilePath& path,
  //                          callback(const MediaGalleryInfo&))
  bool LookUpGalleryByPath(const base::FilePath& path,
                           MediaGalleryPrefInfo* gallery) const;

  MediaGalleryPrefIdSet LookUpGalleriesByDeviceId(
      const std::string& device_id) const;

  // Returns the absolute file path of the gallery specified by the
  // |gallery_id|. Returns an empty file path if the |gallery_id| is invalid.
  // Set |include_unpermitted_galleries| to true to get the file path of the
  // gallery to which this |extension| has no access permission.
  base::FilePath LookUpGalleryPathForExtension(
      MediaGalleryPrefId gallery_id,
      const extensions::Extension* extension,
      bool include_unpermitted_galleries);

  // Teaches the registry about a new gallery.
  // Returns the gallery's pref id.
  MediaGalleryPrefId AddGallery(const std::string& device_id,
                                const string16& display_name,
                                const base::FilePath& relative_path,
                                bool user_added);

  // Teach the registry about a user added registry simply from the path.
  // Returns the gallery's pref id.
  MediaGalleryPrefId AddGalleryByPath(const base::FilePath& path);

  // Removes the gallery identified by |id| from the store.
  void ForgetGalleryById(MediaGalleryPrefId id);

  MediaGalleryPrefIdSet GalleriesForExtension(
      const extensions::Extension& extension) const;

  void SetGalleryPermissionForExtension(const extensions::Extension& extension,
                                        MediaGalleryPrefId pref_id,
                                        bool has_permission);

  const MediaGalleriesPrefInfoMap& known_galleries() const {
    return known_galleries_;
  }

  // ProfileKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // Returns true if the media gallery preferences system has ever been used
  // for this profile. To be exact, it checks if a gallery has ever been added
  // (including defaults).
  static bool APIHasBeenUsed(Profile* profile);

 private:
  typedef std::map<std::string /*device id*/, MediaGalleryPrefIdSet>
      DeviceIdPrefIdsMap;

  // Populates the default galleries if this is a fresh profile.
  void AddDefaultGalleriesIfFreshProfile();

  // Builds |known_galleries_| from the persistent store.
  // Notifies GalleryChangeObservers if |notify_observers| is true.
  void InitFromPrefs(bool notify_observers);

  // Notifies |gallery_change_observers_| about changes in |known_galleries_|.
  void NotifyChangeObservers();

  extensions::ExtensionPrefs* GetExtensionPrefs() const;

  // The profile that owns |this|.
  Profile* profile_;

  // An in-memory cache of known galleries.
  MediaGalleriesPrefInfoMap known_galleries_;

  // A mapping from device id to the set of gallery pref ids on that device.
  // All pref ids in |device_map_| are also in |known_galleries_|.
  DeviceIdPrefIdsMap device_map_;

  ObserverList<GalleryChangeObserver> gallery_change_observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPreferences);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERIES_PREFERENCES_H_
