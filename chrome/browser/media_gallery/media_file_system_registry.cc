// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry implementation.

#include "chrome/browser/media_gallery/media_file_system_registry.h"

#include <set>

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/media/media_file_system_config.h"

#if defined(SUPPORT_MEDIA_FILESYSTEM)
#include "webkit/fileapi/media/media_device_map_service.h"

using fileapi::MediaDeviceMapService;
#endif

namespace chrome {

static base::LazyInstance<MediaFileSystemRegistry>::Leaky
    g_media_file_system_registry = LAZY_INSTANCE_INITIALIZER;

using base::SystemMonitor;
using content::BrowserThread;
using content::RenderProcessHost;
using fileapi::IsolatedContext;

namespace {

bool IsGalleryPermittedForExtension(const extensions::Extension& extension,
                                    const FilePath::StringType& location) {
  if (extension.HasAPIPermission(
        extensions::APIPermission::kMediaGalleriesAllGalleries)) {
    return true;
  }
  // TODO(vandebo) Check with prefs for permission to this gallery.
  return false;
}

}  // namespace


/******************
 * Public methods
 ******************/

// static
MediaFileSystemRegistry* MediaFileSystemRegistry::GetInstance() {
  return g_media_file_system_registry.Pointer();
}

std::vector<MediaFileSystemRegistry::MediaFSInfo>
MediaFileSystemRegistry::GetMediaFileSystemsForExtension(
    const content::RenderProcessHost* rph,
    const extensions::Extension& extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<MediaFSInfo> results;
  std::pair<ChildIdToMediaFSMap::iterator, bool> ret =
      media_fs_map_.insert(std::make_pair(rph, MediaPathToFSIDMap()));
  ChildIdToMediaFSMap::iterator& child_it = ret.first;
  if (ret.second) {
    // Never seen a GetMediaFileSystems call from this RPH. Initialize its
    // file system mappings.
    RegisterForRPHGoneNotifications(rph);
    FilePath pictures_path;
    // TODO(vandebo) file system galleries need a unique id as well.
    if (PathService::Get(chrome::DIR_USER_PICTURES, &pictures_path) &&
        IsGalleryPermittedForExtension(extension, pictures_path.value())) {
      std::string fsid = RegisterPathAsFileSystem(pictures_path);
      child_it->second.insert(std::make_pair(pictures_path, fsid));
    }
  }

  // TODO(thestig) Handle overlap between devices and media directories.
  SystemMonitor* monitor = SystemMonitor::Get();
  const std::vector<SystemMonitor::RemovableStorageInfo> media_devices =
      monitor->GetAttachedRemovableStorage();
  for (size_t i = 0; i < media_devices.size(); ++i) {
    MediaStorageUtil::Type type;
    MediaStorageUtil::CrackDeviceId(media_devices[i].device_id, &type, NULL);
    // TODO(vandebo) Handle MTP devices.
    if (type != MediaStorageUtil::MTP_OR_PTP &&
        IsGalleryPermittedForExtension(extension, media_devices[i].location)) {
      device_id_map_.insert(std::make_pair(media_devices[i].device_id,
                                           media_devices[i]));
      FilePath path(media_devices[i].location);
      const std::string fsid = RegisterPathAsFileSystem(path);
      child_it->second.insert(std::make_pair(path, fsid));
    }
  }

  MediaPathToFSIDMap& child_map = child_it->second;
  for (MediaPathToFSIDMap::const_iterator it = child_map.begin();
       it != child_map.end();
       ++it) {
    MediaFSInfo entry;
    // TODO(vandebo) use a better name, fsid for now.
    entry.name = it->second;
    entry.fsid = it->second;
    entry.path = it->first;
    results.push_back(entry);
  }
  return results;
}

void MediaFileSystemRegistry::OnRemovableStorageDetached(
    const std::string& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DeviceIdToInfoMap::iterator it = device_id_map_.find(id);
  if (it == device_id_map_.end())
    return;

  FilePath path(it->second.location);
  RevokeMediaFileSystem(path);
  device_id_map_.erase(it);
}

void MediaFileSystemRegistry::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED ||
         type == content::NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const RenderProcessHost* rph =
      content::Source<content::RenderProcessHost>(source).ptr();
  ChildIdToMediaFSMap::iterator child_it = media_fs_map_.find(rph);
  CHECK(child_it != media_fs_map_.end());
  // No need to revoke the isolated file systems. The RPH will do that.
  media_fs_map_.erase(child_it);
  UnregisterForRPHGoneNotifications(rph);
}

/******************
 * Private methods
 ******************/

MediaFileSystemRegistry::MediaFileSystemRegistry() {
  // SystemMonitor may be NULL in unit tests.
  SystemMonitor* system_monitor = SystemMonitor::Get();
  if (system_monitor)
    system_monitor->AddDevicesChangedObserver(this);
}

MediaFileSystemRegistry::~MediaFileSystemRegistry() {
  // SystemMonitor may be NULL in unit tests.
  SystemMonitor* system_monitor = SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveDevicesChangedObserver(this);
}

void MediaFileSystemRegistry::RegisterForRPHGoneNotifications(
    const content::RenderProcessHost* rph) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::Source<RenderProcessHost>(rph));
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::Source<RenderProcessHost>(rph));
}

void MediaFileSystemRegistry::UnregisterForRPHGoneNotifications(
    const content::RenderProcessHost* rph) {
  registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                    content::Source<RenderProcessHost>(rph));
  registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                    content::Source<RenderProcessHost>(rph));
}

std::string MediaFileSystemRegistry::RegisterPathAsFileSystem(
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Sanity checks for |path|.
  CHECK(path.IsAbsolute());
  CHECK(!path.ReferencesParent());

  // The directory name is not exposed to the js layer and we simply use
  // a fixed name (as we only register a single directory per file system).
  std::string register_name(extension_misc::kMediaFileSystemPathPart);
  const std::string fsid =
      IsolatedContext::GetInstance()->RegisterFileSystemForPath(
          fileapi::kFileSystemTypeNativeMedia, path, &register_name);
  CHECK(!fsid.empty());
  return fsid;
}

void MediaFileSystemRegistry::RevokeMediaFileSystem(const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  IsolatedContext* isolated_context = IsolatedContext::GetInstance();
  isolated_context->RevokeFileSystemByPath(path);

  for (ChildIdToMediaFSMap::iterator child_it = media_fs_map_.begin();
       child_it != media_fs_map_.end();
       ++child_it) {
    MediaPathToFSIDMap& child_map = child_it->second;
    MediaPathToFSIDMap::iterator media_path_it = child_map.find(path);
    if (media_path_it == child_map.end())
      continue;

    child_map.erase(media_path_it);
  }
}

}  // namespace chrome
