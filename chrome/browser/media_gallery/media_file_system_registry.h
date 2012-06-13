// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry registers pictures directories and media devices as
// File API filesystems and keeps track of the path to filesystem ID mappings.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/system_monitor/system_monitor.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class FilePath;

namespace content {
class RenderProcessHost;
}

namespace fileapi {
class IsolatedContext;
}

namespace chrome {

class MediaFileSystemRegistry
    : public base::SystemMonitor::DevicesChangedObserver,
      public content::NotificationObserver {
 public:
  // (Filesystem ID, path)
  typedef std::pair<std::string, FilePath> MediaFSIDAndPath;

  // The instance is lazily created per browser process.
  static MediaFileSystemRegistry* GetInstance();

  // Returns the list of media filesystem IDs and paths for a given RPH.
  // Called on the UI thread.
  std::vector<MediaFSIDAndPath> GetMediaFileSystems(
      const content::RenderProcessHost* rph);

  // base::SystemMonitor::DevicesChangedObserver implementation.
  virtual void OnMediaDeviceDetached(
      const base::SystemMonitor::DeviceIdType& id) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend struct base::DefaultLazyInstanceTraits<MediaFileSystemRegistry>;

  // Mapping of media directories to filesystem IDs.
  typedef std::map<FilePath, std::string> MediaPathToFSIDMap;

  // Mapping of RPH to MediaPathToFSIDMaps.
  typedef std::map<const content::RenderProcessHost*,
                   MediaPathToFSIDMap> ChildIdToMediaFSMap;

  // Mapping of device id to mount path.
  typedef std::map<base::SystemMonitor::DeviceIdType, FilePath>
      DeviceIdToMediaPathMap;

  // Obtain an instance of this class via GetInstance().
  MediaFileSystemRegistry();
  virtual ~MediaFileSystemRegistry();

  // Helper functions to register / unregister listening for renderer process
  // closed / terminiated notifications.
  void RegisterForRPHGoneNotifications(const content::RenderProcessHost* rph);
  void UnregisterForRPHGoneNotifications(const content::RenderProcessHost* rph);

  // Registers a path as a media file system and return the filesystem id.
  std::string RegisterPathAsFileSystem(const FilePath& path);

  // Revoke a media file system with a given |path|.
  void RevokeMediaFileSystem(const FilePath& path);

  // Only accessed on the UI thread.
  ChildIdToMediaFSMap media_fs_map_;

  // Only accessed on the UI thread.
  DeviceIdToMediaPathMap device_id_map_;

  // Is only used on the UI thread.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistry);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_
