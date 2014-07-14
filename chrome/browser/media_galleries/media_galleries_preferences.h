// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PREFERENCES_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PREFERENCES_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/storage_monitor/removable_storage_observer.h"

class Profile;

namespace base {
class DictionaryValue;
}

namespace extensions {
class Extension;
class ExtensionPrefs;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

typedef uint64 MediaGalleryPrefId;
const MediaGalleryPrefId kInvalidMediaGalleryPrefId = 0;

struct MediaGalleryPermission {
  MediaGalleryPrefId pref_id;
  bool has_permission;
};

struct MediaGalleryPrefInfo {
  enum Type {
    kUserAdded,     // Explicitly added by the user.
    kAutoDetected,  // Auto added to the list of galleries.
    kBlackListed,   // Auto added but then removed by the user.
    kScanResult,    // Discovered by a disk scan.
    kRemovedScan,   // Discovered by a disk scan but then removed by the user.
    kInvalidType,
  };

  MediaGalleryPrefInfo();
  ~MediaGalleryPrefInfo();

  // The absolute path of the gallery.
  base::FilePath AbsolutePath() const;

  // True if the gallery should not be displayed to the user
  // i.e. kBlackListed || kRemovedScan.
  bool IsBlackListedType() const;

  // The ID that identifies this gallery in this Profile.
  MediaGalleryPrefId pref_id;

  // The user-visible name of this gallery.
  base::string16 display_name;

  // A string which uniquely and persistently identifies the device that the
  // gallery lives on.
  std::string device_id;

  // The root of the gallery, relative to the root of the device.
  base::FilePath path;

  // The type of gallery.
  Type type;

  // The volume label of the volume/device on which the gallery
  // resides. Empty if there is no such label or it is unknown.
  base::string16 volume_label;

  // Vendor name for the volume/device on which the gallery is located.
  // Will be empty if unknown.
  base::string16 vendor_name;

  // Model name for the volume/device on which the gallery is located.
  // Will be empty if unknown.
  base::string16 model_name;

  // The capacity in bytes of the volume/device on which the gallery is
  // located. Will be zero if unknown.
  uint64 total_size_in_bytes;

  // If the gallery is on a removable device, the time that device was last
  // attached. It is stored in preferences by the base::Time internal value,
  // which is microseconds since the epoch.
  base::Time last_attach_time;

  // Set to true if the volume metadata fields (volume_label, vendor_name,
  // model_name, total_size_in_bytes) were set. False if these fields were
  // never written.
  bool volume_metadata_valid;

  // The following fields are populated with the audio, image, and video file
  // counts from the last scan. For files where it is hard to determine the
  // exact type, the file should be counted in all possible counts.
  int audio_count;
  int image_count;
  int video_count;

  // 0 if the display_name is set externally and always used for display.
  // 1 if the display_name is only set externally when it is overriding
  // the name constructed from volume metadata.
  int prefs_version;

  // Called by views to provide details for the gallery permission entries.
  base::string16 GetGalleryDisplayName() const;
  base::string16 GetGalleryTooltip() const;
  base::string16 GetGalleryAdditionalDetails() const;

  // Returns true if the gallery is currently a removable device gallery which
  // is now attached, or a fixed storage gallery.
  bool IsGalleryAvailable() const;
};

typedef std::map<MediaGalleryPrefId, MediaGalleryPrefInfo>
    MediaGalleriesPrefInfoMap;
typedef std::set<MediaGalleryPrefId> MediaGalleryPrefIdSet;

// A class to manage the media gallery preferences.  There is one instance per
// user profile. This class lives on the UI thread.
class MediaGalleriesPreferences
    : public KeyedService,
      public storage_monitor::RemovableStorageObserver {
 public:
  class GalleryChangeObserver {
   public:
    // |extension_id| specifies the extension affected by this change.
    // |pref_id| refers to the gallery.
    virtual void OnPermissionAdded(MediaGalleriesPreferences* pref,
                                   const std::string& extension_id,
                                   MediaGalleryPrefId pref_id) {}

    virtual void OnPermissionRemoved(MediaGalleriesPreferences* pref,
                                     const std::string& extension_id,
                                     MediaGalleryPrefId pref_id) {}

    virtual void OnGalleryAdded(MediaGalleriesPreferences* pref,
                                MediaGalleryPrefId pref_id) {}

    virtual void OnGalleryRemoved(MediaGalleriesPreferences* pref,
                                  MediaGalleryPrefId pref_id) {}

    virtual void OnGalleryInfoUpdated(MediaGalleriesPreferences* pref,
                                      MediaGalleryPrefId pref_id) {}

   protected:
    virtual ~GalleryChangeObserver();
  };

  explicit MediaGalleriesPreferences(Profile* profile);
  virtual ~MediaGalleriesPreferences();

  // Ensures that the preferences is initialized. The provided callback, if
  // non-null, will be called when initialization is complete. If initialization
  // has already completed, this callback will be invoked in the calling stack.
  // Before the callback is run, other calls may not return the correct results.
  // Should be invoked on the UI thread; callbacks will be run on the UI thread.
  // This call also ensures that the StorageMonitor is initialized.
  // Note for unit tests: This requires an active FILE thread and
  // EnsureMediaDirectoriesExists instance to complete reliably.
  void EnsureInitialized(base::Closure callback);

  // Return true if the storage monitor has already been initialized.
  bool IsInitialized() const;

  Profile* profile();

  void AddGalleryChangeObserver(GalleryChangeObserver* observer);
  void RemoveGalleryChangeObserver(GalleryChangeObserver* observer);

  // RemovableStorageObserver implementation.
  virtual void OnRemovableStorageAttached(
      const storage_monitor::StorageInfo& info) OVERRIDE;

  // Lookup a media gallery and fill in information about it and return true if
  // it exists. Return false if it does not, filling in default information.
  bool LookUpGalleryByPath(const base::FilePath& path,
                           MediaGalleryPrefInfo* gallery) const;

  MediaGalleryPrefIdSet LookUpGalleriesByDeviceId(
      const std::string& device_id) const;

  // Returns the absolute file path of the gallery specified by the
  // |gallery_id|. Returns an empty file path if the |gallery_id| is invalid.
  // Set |include_unpermitted_galleries| to true to get the file path of the
  // gallery to which this |extension| has no access permission.
  //
  // TODO(tommycli): Remove this function after Media Galleries API Private
  // is transitioned over to public. Also, the body of the function wrong.
  // It just returns the absolute path to the device, not the gallery.
  base::FilePath LookUpGalleryPathForExtension(
      MediaGalleryPrefId gallery_id,
      const extensions::Extension* extension,
      bool include_unpermitted_galleries);

  // Teaches the registry about a new gallery. If the gallery is in a
  // blacklisted state, it is unblacklisted. |type| should not be a blacklisted
  // type. Returns the gallery's pref id.
  MediaGalleryPrefId AddGallery(const std::string& device_id,
                                const base::FilePath& relative_path,
                                MediaGalleryPrefInfo::Type type,
                                const base::string16& volume_label,
                                const base::string16& vendor_name,
                                const base::string16& model_name,
                                uint64 total_size_in_bytes,
                                base::Time last_attach_time,
                                int audio_count,
                                int image_count,
                                int video_count);

  // Teach the registry about a gallery simply from the path. If the gallery is
  // in a blacklisted state, it is unblacklisted. |type| should not be a
  // blacklisted type. Returns the gallery's pref id.
  MediaGalleryPrefId AddGalleryByPath(const base::FilePath& path,
                                      MediaGalleryPrefInfo::Type type);

  // Logically removes the gallery identified by |id| from the store. For
  // auto added or scan result galleries, this means moving them into a
  // blacklisted state, otherwise they may come back when they are detected
  // again.
  void ForgetGalleryById(MediaGalleryPrefId id);

  // Remove the gallery identified by |id| from the store entirely. If it is an
  // auto added or scan result gallery, it could get added again when the
  // location is noticed again.
  void EraseGalleryById(MediaGalleryPrefId id);

  // Returns true if some extension has permission for |id|, which may not be
  // an auto detected type.
  bool NonAutoGalleryHasPermission(MediaGalleryPrefId id) const;

  MediaGalleryPrefIdSet GalleriesForExtension(
      const extensions::Extension& extension);

  // Returns true if the permission changed. Returns false if there was
  // no change.
  bool SetGalleryPermissionForExtension(const extensions::Extension& extension,
                                        MediaGalleryPrefId pref_id,
                                        bool has_permission);

  const MediaGalleriesPrefInfoMap& known_galleries() const;

  // These keep track of when we last successfully completed a media scan.
  // This is used to provide cached results when appropriate.
  base::Time GetLastScanCompletionTime() const;
  void SetLastScanCompletionTime(const base::Time& time);

  // KeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if the media gallery preferences system has ever been used
  // for this profile. To be exact, it checks if a gallery has ever been added
  // (including defaults).
  static bool APIHasBeenUsed(Profile* profile);

 private:
  friend class MediaGalleriesPreferencesTest;
  friend class MediaGalleriesPermissionsTest;

  typedef std::map<std::string /*device id*/, MediaGalleryPrefIdSet>
      DeviceIdPrefIdsMap;

  // These must be called on the UI thread.
  void OnInitializationCallbackReturned();
  void FinishInitialization();

  // Populates the default galleries. Call only on fresh profiles.
  void AddDefaultGalleries();

  // This is a hack - Some devices (iTunes, Picasa) are singletons in that only
  // one instance of that type is supported at a time. As such, the device id
  // should just be "itunes:" or "picasa:" but that would mean finding the
  // location of the database file multiple times, which may be an async
  // operation. Storing the location of the backing database in the device
  // id allows that look up to be avoided. However, the cost is that if the
  // database moves, the device id in preferences has to be updated.  This
  // method searches for a gallery of the type passed in and updates its
  // device id.  It returns true if the device id is up to date.
  bool UpdateDeviceIDForSingletonType(const std::string& device_id);

  void OnStorageMonitorInit(bool add_default_galleries);

  // Handle an iPhoto, iTunes, or Picasa finder returning a device ID to us.
  void OnFinderDeviceID(const std::string& device_id);

  // Builds |known_galleries_| from the persistent store.
  void InitFromPrefs();

  MediaGalleryPrefId AddGalleryInternal(const std::string& device_id,
                                        const base::string16& display_name,
                                        const base::FilePath& relative_path,
                                        MediaGalleryPrefInfo::Type type,
                                        const base::string16& volume_label,
                                        const base::string16& vendor_name,
                                        const base::string16& model_name,
                                        uint64 total_size_in_bytes,
                                        base::Time last_attach_time,
                                        bool volume_metadata_valid,
                                        int audio_count,
                                        int image_count,
                                        int video_count,
                                        int prefs_version);

  void EraseOrBlacklistGalleryById(MediaGalleryPrefId id, bool erase);

  // Sets permission for the media galleries identified by |gallery_id| for the
  // extension in the given |prefs|. Returns true only if anything changed.
  bool SetGalleryPermissionInPrefs(const std::string& extension_id,
                                   MediaGalleryPrefId gallery_id,
                                   bool has_access);

  // Removes the entry for the media galleries permissions identified by
  // |gallery_id| for the extension in the given |prefs|.
  // Returns true only if anything changed.
  bool UnsetGalleryPermissionInPrefs(const std::string& extension_id,
                                     MediaGalleryPrefId gallery_id);

  // Return all media gallery permissions for the extension in the given
  // |prefs|.
  std::vector<MediaGalleryPermission> GetGalleryPermissionsFromPrefs(
      const std::string& extension_id) const;

  // Remove all the media gallery permissions in |prefs| for the gallery
  // specified by |gallery_id|.
  void RemoveGalleryPermissionsFromPrefs(MediaGalleryPrefId gallery_id);

  // Get the ExtensionPrefs to use; this will be either the ExtensionPrefs
  // object associated with |profile_|, or extension_prefs_for_testing_, if
  // SetExtensionPrefsForTesting() has been called.
  extensions::ExtensionPrefs* GetExtensionPrefs() const;

  // Set the ExtensionPrefs object to be returned by GetExtensionPrefs().
  void SetExtensionPrefsForTesting(extensions::ExtensionPrefs* extension_prefs);

  bool initialized_;
  std::vector<base::Closure> on_initialize_callbacks_;
  int pre_initialization_callbacks_waiting_;

  // The profile that owns |this|.
  Profile* profile_;

  // The ExtensionPrefs used in a testing environment, where KeyedServices
  // aren't used. This will be NULL unless it is set with
  // SetExtensionPrefsForTesting().
  extensions::ExtensionPrefs* extension_prefs_for_testing_;

  // An in-memory cache of known galleries.
  MediaGalleriesPrefInfoMap known_galleries_;

  // A mapping from device id to the set of gallery pref ids on that device.
  // All pref ids in |device_map_| are also in |known_galleries_|.
  DeviceIdPrefIdsMap device_map_;

  ObserverList<GalleryChangeObserver> gallery_change_observers_;

  base::WeakPtrFactory<MediaGalleriesPreferences> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPreferences);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PREFERENCES_H_
