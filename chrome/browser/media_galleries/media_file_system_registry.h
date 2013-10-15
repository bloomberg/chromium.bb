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

// Contains information about a particular filesystem being provided to a
// client, including metadata like the name and ID, and API handles like the
// fsid (filesystem ID) used to hook up the API objects.
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

class MediaFileSystemRegistry
    : public RemovableStorageObserver,
      public MediaGalleriesPreferences::GalleryChangeObserver {
 public:
  MediaFileSystemRegistry();
  virtual ~MediaFileSystemRegistry();

  // Passes to |callback| the list of media filesystem IDs and paths for a
  // given RVH. Called on the UI thread.
  void GetMediaFileSystemsForExtension(
      const content::RenderViewHost* rvh,
      const extensions::Extension* extension,
      const MediaFileSystemsCallback& callback);

  // Returns the media galleries preferences for the specified |profile|.
  // Called on the UI thread. Caller is responsible for ensuring that the
  // preferences are initialized before use.
  MediaGalleriesPreferences* GetPreferences(Profile* profile);

  // RemovableStorageObserver implementation.
  virtual void OnRemovableStorageDetached(const StorageInfo& info) OVERRIDE;

 private:
  class MediaFileSystemContextImpl;

  friend class MediaFileSystemContextImpl;
  friend class MediaFileSystemRegistryTest;
  friend class TestMediaFileSystemContext;

  // Map an extension to the ExtensionGalleriesHost.
  typedef std::map<std::string /*extension_id*/,
                   scoped_refptr<ExtensionGalleriesHost> > ExtensionHostMap;
  // Map a profile and extension to the ExtensionGalleriesHost.
  typedef std::map<Profile*, ExtensionHostMap> ExtensionGalleriesHostMap;

  // Map a filesystem id (fsid) to the reference to an MTP device.
  typedef std::map<std::string, scoped_refptr<ScopedMTPDeviceMapEntry> >
      MTPDeviceEntryMap;

  // Map a MTP or PTP device location to the raw pointer of
  // ScopedMTPDeviceMapEntry. It is safe to store a raw pointer in this
  // map.
  typedef std::map<const base::FilePath::StringType, ScopedMTPDeviceMapEntry*>
      MTPDeviceDelegateMap;

  virtual void OnPermissionRemoved(MediaGalleriesPreferences* pref,
                                   const std::string& extension_id,
                                   MediaGalleryPrefId pref_id) OVERRIDE;
  virtual void OnGalleryRemoved(MediaGalleriesPreferences* pref,
                                MediaGalleryPrefId pref_id) OVERRIDE;

  // Returns ScopedMTPDeviceMapEntry object for the given |device_location|.
  scoped_refptr<ScopedMTPDeviceMapEntry> GetOrCreateScopedMTPDeviceMapEntry(
      const base::FilePath::StringType& device_location,
      const std::string& fsid);

  void RevokeMTPFileSystem(const std::string& fsid);

  void OnExtensionGalleriesHostEmpty(Profile* profile,
                                     const std::string& extension_id);

  // Removes the ScopedMTPDeviceMapEntry associated with the given
  // |device_location|.
  void RemoveScopedMTPDeviceMapEntry(
      const base::FilePath::StringType& device_location);

  // Only accessed on the UI thread. This map owns all the
  // ExtensionGalleriesHost objects created.
  ExtensionGalleriesHostMap extension_hosts_map_;

  // Contains a map of fsid to ScopedMTPDeviceMapEntry.
  MTPDeviceEntryMap mtp_device_map_;

  // Only accessed on the UI thread.
  MTPDeviceDelegateMap mtp_device_delegate_map_;

  scoped_ptr<MediaFileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistry);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_REGISTRY_H_
