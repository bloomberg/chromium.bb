// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry registers pictures directories and media devices as
// File API filesystems and keeps track of the path to filesystem ID mappings.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_REGISTRY_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/mtp_device_delegate_impl.h"
#include "chrome/browser/storage_monitor/removable_storage_observer.h"

class ExtensionGalleriesHost;
class MediaFileSystemContext;
class MediaGalleriesPreferences;
class Profile;
class ScopedMTPDeviceMapEntry;


namespace content {
class RenderViewHost;
}

namespace extensions {
class Extension;
}

namespace fileapi {
class IsolatedContext;
}

struct MediaFileSystemInfo {
  MediaFileSystemInfo(const string16& fs_name,
                      const base::FilePath& fs_path,
                      const std::string& filesystem_id,
                      MediaGalleryPrefId pref_id,
                      const std::string& transient_device_id,
                      bool removable,
                      bool media_device);
  MediaFileSystemInfo();
  ~MediaFileSystemInfo();

  string16 name;
  base::FilePath path;
  std::string fsid;
  MediaGalleryPrefId pref_id;
  std::string transient_device_id;
  bool removable;
  bool media_device;
};

typedef base::Callback<void(const std::vector<MediaFileSystemInfo>&)>
    MediaFileSystemsCallback;

class MediaFileSystemRegistry : public RemovableStorageObserver {
 public:
  MediaFileSystemRegistry();
  virtual ~MediaFileSystemRegistry();

  // Passes to |callback| the list of media filesystem IDs and paths for a
  // given RVH. Called on the UI thread.
  void GetMediaFileSystemsForExtension(
      const content::RenderViewHost* rvh,
      const extensions::Extension* extension,
      const MediaFileSystemsCallback& callback);

  // Returns the initialized media galleries preferences for the specified
  // |profile|. This method should be used instead of calling
  // MediaGalleriesPreferences directly because this method also ensures that
  // currently attached removable devices are added to the preferences.
  // Called on the UI thread.
  // Note: Caller must ensure that the storage monitor is initialized before
  // calling this method.
  MediaGalleriesPreferences* GetPreferences(Profile* profile);

  // RemovableStorageObserver implementation.
  virtual void OnRemovableStorageDetached(const StorageInfo& info) OVERRIDE;

 private:
  friend class MediaFileSystemRegistryTest;
  friend class TestMediaFileSystemContext;
  class MediaFileSystemContextImpl;

  // Map an extension to the ExtensionGalleriesHost.
  typedef std::map<std::string /*extension_id*/,
                   scoped_refptr<ExtensionGalleriesHost> > ExtensionHostMap;
  // Map a profile and extension to the ExtensionGalleriesHost.
  typedef std::map<Profile*, ExtensionHostMap> ExtensionGalleriesHostMap;
  // Map a profile to a PrefChangeRegistrar.
  typedef std::map<Profile*, PrefChangeRegistrar*> PrefChangeRegistrarMap;

  // Map a MTP or PTP device location to the raw pointer of
  // ScopedMTPDeviceMapEntry. It is safe to store a raw pointer in this
  // map.
  typedef std::map<const base::FilePath::StringType, ScopedMTPDeviceMapEntry*>
      MTPDeviceDelegateMap;

  void OnRememberedGalleriesChanged(PrefService* service);

  // Returns ScopedMTPDeviceMapEntry object for the given |device_location|.
  scoped_refptr<ScopedMTPDeviceMapEntry> GetOrCreateScopedMTPDeviceMapEntry(
      const base::FilePath::StringType& device_location);

  // Removes the ScopedMTPDeviceMapEntry associated with the given
  // |device_location|.
  void RemoveScopedMTPDeviceMapEntry(
      const base::FilePath::StringType& device_location);

  void OnExtensionGalleriesHostEmpty(Profile* profile,
                                     const std::string& extension_id);

  // Only accessed on the UI thread. This map owns all the
  // ExtensionGalleriesHost objects created.
  ExtensionGalleriesHostMap extension_hosts_map_;

  PrefChangeRegistrarMap pref_change_registrar_map_;

  // Only accessed on the UI thread.
  MTPDeviceDelegateMap mtp_device_delegate_map_;

  scoped_ptr<MediaFileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistry);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_REGISTRY_H_
