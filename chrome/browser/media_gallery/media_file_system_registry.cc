// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry implementation.

#include "chrome/browser/media_gallery/media_file_system_registry.h"

#include <set>

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "webkit/fileapi/isolated_context.h"

namespace chrome {

static base::LazyInstance<MediaFileSystemRegistry>::Leaky
    g_media_file_system_registry = LAZY_INSTANCE_INITIALIZER;

using base::SystemMonitor;
using content::BrowserThread;
using content::RenderProcessHost;
using fileapi::IsolatedContext;

/******************
 * Public methods
 ******************/

// static
MediaFileSystemRegistry* MediaFileSystemRegistry::GetInstance() {
  return g_media_file_system_registry.Pointer();
}

std::vector<MediaFileSystemRegistry::MediaFSIDAndPath>
MediaFileSystemRegistry::GetMediaFileSystems(
    const content::RenderProcessHost* rph) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<MediaFSIDAndPath> results;
  std::pair<ChildIdToMediaFSMap::iterator, bool> ret =
      media_fs_map_.insert(std::make_pair(rph, MediaPathToFSIDMap()));
  ChildIdToMediaFSMap::iterator& child_it = ret.first;
  if (ret.second) {
    // Never seen a GetMediaFileSystems call from this RPH. Initialize its
    // file system mappings.
    RegisterForRPHGoneNotifications(rph);
    FilePath pictures_path;
    if (PathService::Get(chrome::DIR_USER_PICTURES, &pictures_path)) {
      std::string fsid = RegisterPathAsFileSystem(pictures_path);
      child_it->second.insert(std::make_pair(pictures_path, fsid));
    }
  }

  // TODO(thestig) Handle overlap between devices and media directories.
  SystemMonitor* monitor = SystemMonitor::Get();
  const std::vector<SystemMonitor::MediaDeviceInfo> media_devices =
      monitor->GetAttachedMediaDevices();
  for (size_t i = 0; i < media_devices.size(); ++i) {
    const SystemMonitor::DeviceIdType& id = media_devices[i].a;
    const FilePath& path = media_devices[i].c;
    device_id_map_.insert(std::make_pair(id, path));
    std::string fsid = RegisterPathAsFileSystem(path);
    child_it->second.insert(std::make_pair(path, fsid));
  }

  MediaPathToFSIDMap& child_map = child_it->second;
  for (MediaPathToFSIDMap::const_iterator it = child_map.begin();
       it != child_map.end();
       ++it) {
    const FilePath path = it->first;
    const std::string fsid = it->second;
    results.push_back(std::make_pair(fsid, path));
  }
  return results;
}

void MediaFileSystemRegistry::OnMediaDeviceDetached(
    const base::SystemMonitor::DeviceIdType& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DeviceIdToMediaPathMap::iterator it = device_id_map_.find(id);
  if (it == device_id_map_.end())
    return;
  RevokeMediaFileSystem(it->second);
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
  SystemMonitor::Get()->AddDevicesChangedObserver(this);
}

MediaFileSystemRegistry::~MediaFileSystemRegistry() {
  SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
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
  // Make sure |path| does not refer to '/' on Unix.
  // TODO(thestig) Check how BaseName() works for say, 'C:\' on Windows.
  CHECK(!path.BaseName().IsAbsolute());
  CHECK(!path.BaseName().empty());

  std::set<FilePath> fileset;
  fileset.insert(path);
  const std::string fsid =
      IsolatedContext::GetInstance()->RegisterIsolatedFileSystem(fileset);
  CHECK(!fsid.empty());
  return fsid;
}

void MediaFileSystemRegistry::RevokeMediaFileSystem(const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  IsolatedContext* isolated_context = IsolatedContext::GetInstance();
  for (ChildIdToMediaFSMap::iterator child_it = media_fs_map_.begin();
       child_it != media_fs_map_.end();
       ++child_it) {
    MediaPathToFSIDMap& child_map = child_it->second;
    MediaPathToFSIDMap::iterator media_path_it = child_map.find(path);
    if (media_path_it == child_map.end())
      continue;
    isolated_context->RevokeIsolatedFileSystem(media_path_it->second);
    child_map.erase(media_path_it);
  }
}

}  // namespace chrome
