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
#include "base/lazy_instance.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/system_monitor/system_monitor.h"
#include "webkit/fileapi/media/media_file_system_config.h"

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
class MediaGalleriesPreferences;
class ScopedMediaDeviceMapEntry;

class MediaFileSystemRegistry
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  struct MediaFSInfo {
    string16 name;
    std::string fsid;
    FilePath path;
  };

  // The instance is lazily created per browser process.
  static MediaFileSystemRegistry* GetInstance();

  // Returns the list of media filesystem IDs and paths for a given RVH.
  // Called on the UI thread.
  std::vector<MediaFSInfo> GetMediaFileSystemsForExtension(
      const content::RenderViewHost* rvh,
      const extensions::Extension* extension);

#if defined(SUPPORT_MEDIA_FILESYSTEM)
  // Registers and returns the file system id for the mtp or ptp device
  // specified by |device_id| and |path|. Updates |entry| with the corresponding
  // ScopedMediaDeviceMapEntry object.
  std::string RegisterFileSystemForMtpDevice(
      const std::string& device_id, const FilePath& path,
      scoped_refptr<ScopedMediaDeviceMapEntry>* entry);
#endif

  // base::SystemMonitor::DevicesChangedObserver implementation.
  virtual void OnRemovableStorageAttached(
      const std::string& id, const string16& name,
      const FilePath::StringType& location) OVERRIDE;
  virtual void OnRemovableStorageDetached(const std::string& id) OVERRIDE;

 private:
  friend struct base::DefaultLazyInstanceTraits<MediaFileSystemRegistry>;

  // Map an extension to the ExtensionGalleriesHost.
  typedef std::map<std::string /*extension_id*/,
                   scoped_refptr<ExtensionGalleriesHost> > ExtensionHostMap;
  // Map a profile and extension to the ExtensionGalleriesHost.
  typedef std::map<Profile*, ExtensionHostMap> ExtensionGalleriesHostMap;

#if defined(SUPPORT_MEDIA_FILESYSTEM)
  // Map a mtp or ptp device location to the weak pointer of
  // ScopedMediaDeviceMapEntry.
  typedef std::map<const FilePath::StringType,
                   base::WeakPtr<ScopedMediaDeviceMapEntry> >
      MTPDeviceDelegateMap;
#endif

  // Obtain an instance of this class via GetInstance().
  MediaFileSystemRegistry();
  virtual ~MediaFileSystemRegistry();

#if defined(SUPPORT_MEDIA_FILESYSTEM)
  // Returns ScopedMediaDeviceMapEntry object for the given |device_location|.
  ScopedMediaDeviceMapEntry* GetOrCreateScopedMediaDeviceMapEntry(
      const FilePath::StringType& device_location);

  // Removes the ScopedMediaDeviceMapEntry associated with the given
  // |device_location|.
  void RemoveScopedMediaDeviceMapEntry(
      const FilePath::StringType& device_location);
#endif

  // Register all the media devices found in system monitor as auto-detected
  // galleries for the passed |preferences|.
  void AddAttachedMediaDeviceGalleries(MediaGalleriesPreferences* preferences);

  void OnExtensionGalleriesHostEmpty(Profile* profile,
                                     const std::string& extension_id);

  // Only accessed on the UI thread. This map owns all the
  // ExtensionGalleriesHost objects created.
  ExtensionGalleriesHostMap extension_hosts_map_;

#if defined(SUPPORT_MEDIA_FILESYSTEM)
  // Only accessed on the UI thread.
  MTPDeviceDelegateMap mtp_delegate_map_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistry);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_
