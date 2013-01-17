// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry registers pictures directories and media devices as
// File API filesystems and keeps track of the path to filesystem ID mappings.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/media_gallery/media_galleries_preferences.h"
#include "chrome/browser/media_gallery/transient_device_ids.h"
#include "webkit/fileapi/media/mtp_device_file_system_config.h"

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
#include "chrome/browser/media_gallery/mtp_device_delegate_impl.h"
#endif

class Profile;

namespace content {
class RenderViewHost;
}

namespace extensions {
class Extension;
}

namespace fileapi {
class IsolatedContext;
}

namespace chrome {

class ExtensionGalleriesHost;
class MediaFileSystemContext;
class MediaGalleriesPreferences;
class ScopedMTPDeviceMapEntry;

struct MediaFileSystemInfo {
  MediaFileSystemInfo(const std::string& fs_name,
                      const FilePath& fs_path,
                      const std::string& filesystem_id,
                      MediaGalleryPrefId pref_id,
                      uint64 transient_device_id,
                      bool removable,
                      bool media_device);
  MediaFileSystemInfo();

  std::string name;  // JSON string, must not contain slashes.
  FilePath path;
  std::string fsid;
  MediaGalleryPrefId pref_id;
  uint64 transient_device_id;
  bool removable;
  bool media_device;
};

typedef base::Callback<void(const std::vector<MediaFileSystemInfo>&)>
    MediaFileSystemsCallback;

class MediaFileSystemRegistry
    : public base::SystemMonitor::DevicesChangedObserver {
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
  MediaGalleriesPreferences* GetPreferences(Profile* profile);

  // base::SystemMonitor::DevicesChangedObserver implementation.
  virtual void OnRemovableStorageAttached(
      const std::string& id,
      const string16& name,
      const FilePath::StringType& location) OVERRIDE;
  virtual void OnRemovableStorageDetached(const std::string& id) OVERRIDE;

  size_t GetExtensionHostCountForTests() const;

  // See TransientDeviceIds::GetTransientIdForDeviceId().
  uint64 GetTransientIdForDeviceId(const std::string& device_id);

  // Keys for metadata about a media gallery file system.
  static const char kDeviceIdKey[];
  static const char kGalleryIdKey[];
  static const char kNameKey[];

 private:
  friend class TestMediaFileSystemContext;
  class MediaFileSystemContextImpl;

  // Map an extension to the ExtensionGalleriesHost.
  typedef std::map<std::string /*extension_id*/,
                   scoped_refptr<ExtensionGalleriesHost> > ExtensionHostMap;
  // Map a profile and extension to the ExtensionGalleriesHost.
  typedef std::map<Profile*, ExtensionHostMap> ExtensionGalleriesHostMap;
  // Map a profile to a PrefChangeRegistrar.
  typedef std::map<Profile*, PrefChangeRegistrar*> PrefChangeRegistrarMap;

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  // Map a MTP or PTP device location to the raw pointer of
  // ScopedMTPDeviceMapEntry. It is safe to store a raw pointer in this
  // map.
  typedef std::map<const FilePath::StringType, ScopedMTPDeviceMapEntry*>
      MTPDeviceDelegateMap;
#endif

  void OnMediaGalleriesRememberedGalleriesChanged(PrefServiceBase* service);

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  // Returns ScopedMTPDeviceMapEntry object for the given |device_location|.
  scoped_refptr<ScopedMTPDeviceMapEntry> GetOrCreateScopedMTPDeviceMapEntry(
      const FilePath::StringType& device_location);

  // Removes the ScopedMTPDeviceMapEntry associated with the given
  // |device_location|.
  void RemoveScopedMTPDeviceMapEntry(
      const FilePath::StringType& device_location);
#endif

  void OnExtensionGalleriesHostEmpty(Profile* profile,
                                     const std::string& extension_id);

  // Only accessed on the UI thread. This map owns all the
  // ExtensionGalleriesHost objects created.
  ExtensionGalleriesHostMap extension_hosts_map_;

  PrefChangeRegistrarMap pref_change_registrar_map_;

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  // Only accessed on the UI thread.
  MTPDeviceDelegateMap mtp_device_delegate_map_;
#endif

  scoped_ptr<MediaFileSystemContext> file_system_context_;

  TransientDeviceIds transient_device_ids_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistry);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_
