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
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "components/storage_monitor/removable_storage_observer.h"

class ExtensionGalleriesHost;
class GalleryWatchManager;
class MediaFileSystemContext;
class MediaGalleriesPreferences;
class MediaScanManager;
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

// Contains information about a particular filesystem being provided to a
// client, including metadata like the name and ID, and API handles like the
// fsid (filesystem ID) used to hook up the API objects.
struct MediaFileSystemInfo {
  MediaFileSystemInfo(const base::string16& fs_name,
                      const base::FilePath& fs_path,
                      const std::string& filesystem_id,
                      MediaGalleryPrefId pref_id,
                      const std::string& transient_device_id,
                      bool removable,
                      bool media_device);
  MediaFileSystemInfo();
  ~MediaFileSystemInfo();

  base::string16 name;
  base::FilePath path;
  std::string fsid;
  MediaGalleryPrefId pref_id;
  std::string transient_device_id;
  bool removable;
  bool media_device;
};

typedef base::Callback<void(const std::vector<MediaFileSystemInfo>&)>
    MediaFileSystemsCallback;

// Tracks usage of filesystems by extensions.
// This object lives on the UI thread.
class MediaFileSystemRegistry
    : public storage_monitor::RemovableStorageObserver,
      public MediaGalleriesPreferences::GalleryChangeObserver {
 public:
  MediaFileSystemRegistry();
  virtual ~MediaFileSystemRegistry();

  // Passes to |callback| the list of media filesystem IDs and paths for a
  // given RVH.
  void GetMediaFileSystemsForExtension(
      const content::RenderViewHost* rvh,
      const extensions::Extension* extension,
      const MediaFileSystemsCallback& callback);

  // Attempt to register the file system for |pref_id|. If |extension| does not
  // have permission to |pref_id|, sends |callback| FILE_ERROR_NOT_FOUND.
  void RegisterMediaFileSystemForExtension(
      const content::RenderViewHost* rvh,
      const extensions::Extension* extension,
      MediaGalleryPrefId pref_id,
      const base::Callback<void(base::File::Error result)>& callback);

  // Returns the media galleries preferences for the specified |profile|.
  // Caller is responsible for ensuring that the preferences are initialized
  // before use.
  MediaGalleriesPreferences* GetPreferences(Profile* profile);

  MediaScanManager* media_scan_manager();
  GalleryWatchManager* gallery_watch_manager();

  // RemovableStorageObserver implementation.
  virtual void OnRemovableStorageDetached(
      const storage_monitor::StorageInfo& info) OVERRIDE;

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

  virtual void OnPermissionRemoved(MediaGalleriesPreferences* pref,
                                   const std::string& extension_id,
                                   MediaGalleryPrefId pref_id) OVERRIDE;
  virtual void OnGalleryRemoved(MediaGalleriesPreferences* pref,
                                MediaGalleryPrefId pref_id) OVERRIDE;

  // Look up or create the extension gallery host.
  ExtensionGalleriesHost* GetExtensionGalleryHost(
      Profile* profile,
      MediaGalleriesPreferences* preferences,
      const std::string& extension_id);

  void OnExtensionGalleriesHostEmpty(Profile* profile,
                                     const std::string& extension_id);

  // This map owns all the ExtensionGalleriesHost objects created.
  ExtensionGalleriesHostMap extension_hosts_map_;

  scoped_ptr<MediaFileSystemContext> file_system_context_;

  scoped_ptr<MediaScanManager> media_scan_manager_;
  scoped_ptr<GalleryWatchManager> gallery_watch_manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistry);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_REGISTRY_H_
