// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry implementation.

#include "chrome/browser/media_gallery/media_file_system_registry.h"

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_gallery/media_galleries_preferences.h"
#include "chrome/browser/media_gallery/media_galleries_preferences_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/media/media_file_system_config.h"

#if defined(SUPPORT_MEDIA_FILESYSTEM)
#include "webkit/fileapi/media/media_device_map_service.h"
#endif

namespace chrome {

static base::LazyInstance<MediaFileSystemRegistry>::Leaky
    g_media_file_system_registry = LAZY_INSTANCE_INITIALIZER;

using base::SystemMonitor;
using content::BrowserThread;
using content::NavigationController;
using content::RenderProcessHost;
using content::WebContents;
using fileapi::IsolatedContext;

namespace {

struct InvalidatedGalleriesInfo {
  std::set<ExtensionGalleriesHost*> extension_hosts;
  std::set<MediaGalleryPrefId> pref_ids;
};

std::string RegisterFileSystem(std::string device_id, const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  IsolatedContext* isolated_context = IsolatedContext::GetInstance();
  if (MediaStorageUtil::IsMassStorageDevice(device_id)) {
    // Sanity checks for |path|.
    CHECK(path.IsAbsolute());
    CHECK(!path.ReferencesParent());
    std::string fs_name(extension_misc::kMediaFileSystemPathPart);
    const std::string fsid = isolated_context->RegisterFileSystemForPath(
        fileapi::kFileSystemTypeNativeMedia, path, &fs_name);
    CHECK(!fsid.empty());
    return fsid;
  }

  // TODO(kmadhusu) handle MTP devices.
  NOTREACHED();
  return std::string();
}

}  // namespace

// Refcounted only so it can easily be put in map. All instances are owned
// by |MediaFileSystemRegistry::extension_hosts_map_|.
class ExtensionGalleriesHost
    : public base::RefCounted<ExtensionGalleriesHost>,
      public content::NotificationObserver {
 public:
  // |no_references_callback| is called when the last RenderViewHost reference
  // goes away. RenderViewHost references are added through ReferenceFromRVH().
  explicit ExtensionGalleriesHost(const base::Closure& no_references_callback)
      : no_references_callback_(no_references_callback) {
  }

  // For each gallery in the list of permitted |galleries|, looks up or creates
  // a file system id and returns the information needed for the renderer to
  // create those file system objects.
  std::vector<MediaFileSystemRegistry::MediaFSInfo> GetMediaFileSystems(
      const MediaGalleryPrefIdSet& galleries,
      const MediaGalleriesPrefInfoMap& galleries_info) {
    std::vector<MediaFileSystemRegistry::MediaFSInfo> result;
    for (std::set<MediaGalleryPrefId>::const_iterator id = galleries.begin();
         id != galleries.end();
         ++id) {
      PrefIdFsInfoMap::const_iterator existing_info = pref_id_map_.find(*id);
      if (existing_info != pref_id_map_.end()) {
        result.push_back(existing_info->second);
        continue;
      }
      const MediaGalleryPrefInfo& gallery_info =
          galleries_info.find(*id)->second;
      const std::string& device_id = gallery_info.device_id;
      // TODO(kmadhusu) handle MTP devices.
      DCHECK(MediaStorageUtil::IsMassStorageDevice(device_id));
      FilePath path = MediaStorageUtil::FindDevicePathById(device_id);
      // TODO(vandebo) we also need to check that these galleries are attached.
      // For now, just skip over entries that we couldn't find.
      if (path.empty())
        continue;
      CHECK(!path.empty());
      path = path.Append(gallery_info.path);

      MediaFileSystemRegistry::MediaFSInfo new_entry;
      new_entry.name = gallery_info.display_name;
      new_entry.path = path;
      new_entry.fsid = RegisterFileSystem(device_id, path);
      result.push_back(new_entry);
      pref_id_map_[*id] = new_entry;
    }

    // TODO(vandebo) We need a way of getting notification when permission for
    // galleries are revoked (http://crbug.com/145855).  For now, invalidate
    // any fsid we have that we didn't get this time around.
    if (galleries.size() != pref_id_map_.size()) {
      MediaGalleryPrefIdSet old_galleries;
      for (PrefIdFsInfoMap::const_iterator it = pref_id_map_.begin();
           it != pref_id_map_.end();
           ++it) {
        old_galleries.insert(it->first);
      }
      MediaGalleryPrefIdSet invalid_galleries;
      std::set_difference(old_galleries.begin(), old_galleries.end(),
                          galleries.begin(), galleries.end(),
                          std::inserter(invalid_galleries,
                                        invalid_galleries.begin()));
      for (MediaGalleryPrefIdSet::const_iterator it = invalid_galleries.begin();
           it != invalid_galleries.end();
           ++it) {
        RevokeGalleryByPrefId(*it);
      }
    }

    return result;
  }

  // TODO(kmadhusu): Clean up this code. http://crbug.com/140340.
  // Revoke the file system for |id| if this extension has created one for |id|.
  void RevokeGalleryByPrefId(MediaGalleryPrefId id) {
    PrefIdFsInfoMap::iterator gallery = pref_id_map_.find(id);
    if (gallery == pref_id_map_.end())
      return;

    IsolatedContext* isolated_context = IsolatedContext::GetInstance();
    isolated_context->RevokeFileSystem(gallery->second.fsid);

    pref_id_map_.erase(gallery);
  }

  // Indicate that the passed |rvh| will reference the file system ids created
  // by this class.  It is safe to call this multiple times with the same RVH.
  void ReferenceFromRVH(const content::RenderViewHost* rvh) {
    WebContents* contents = WebContents::FromRenderViewHost(rvh);
    if (registrar_.IsRegistered(this,
                                content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                                content::Source<WebContents>(contents))) {
      return;
    }
    registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   content::Source<WebContents>(contents));
    registrar_.Add(
        this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<NavigationController>(&contents->GetController()));

    RenderProcessHost* rph = contents->GetRenderProcessHost();
    rph_refs_[rph].insert(contents);
    if (rph_refs_[rph].size() == 1) {
      registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                     content::Source<RenderProcessHost>(rph));
    }
  }

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
        OnRendererProcessClosed(
            content::Source<RenderProcessHost>(source).ptr());
        break;
      }
      case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
        OnWebContentsDestroyedOrNavigated(
            content::Source<WebContents>(source).ptr());
        break;
      }
      case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
        NavigationController* controller =
            content::Source<NavigationController>(source).ptr();
        WebContents* contents = controller->GetWebContents();
        OnWebContentsDestroyedOrNavigated(contents);
        break;
      }
      default: {
        NOTREACHED();
        break;
      }
    }
  }

 private:
  typedef std::map<MediaGalleryPrefId, MediaFileSystemRegistry::MediaFSInfo>
      PrefIdFsInfoMap;
  typedef std::map<const RenderProcessHost*, std::set<const WebContents*> >
      RenderProcessHostRefCount;

  // Private destructor and friend declaration for ref counted implementation.
  friend class base::RefCounted<ExtensionGalleriesHost>;

  virtual ~ExtensionGalleriesHost() {
    DCHECK(rph_refs_.empty());
    DCHECK(pref_id_map_.empty());
  }

  void OnRendererProcessClosed(const RenderProcessHost* rph) {
    RenderProcessHostRefCount::const_iterator rph_info = rph_refs_.find(rph);
    DCHECK(rph_info != rph_refs_.end());
    // We're going to remove everything from the set, so we make a copy
    // before operating on it.
    std::set<const WebContents*> closed_web_contents = rph_info->second;
    DCHECK(!closed_web_contents.empty());

    for (std::set<const WebContents*>::const_iterator it =
             closed_web_contents.begin();
         it != closed_web_contents.end();
         ++it) {
      OnWebContentsDestroyedOrNavigated(*it);
    }
  }

  void OnWebContentsDestroyedOrNavigated(const WebContents* contents) {
    registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                      content::Source<WebContents>(contents));
    registrar_.Remove(
        this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<NavigationController>(&contents->GetController()));

    RenderProcessHost* rph = contents->GetRenderProcessHost();
    RenderProcessHostRefCount::iterator process_refs = rph_refs_.find(rph);
    DCHECK(process_refs != rph_refs_.end());
    process_refs->second.erase(contents);
    if (process_refs->second.empty()) {
      registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                        content::Source<RenderProcessHost>(rph));
      rph_refs_.erase(process_refs);
    }

    if (rph_refs_.empty()) {
      IsolatedContext* isolated_context = IsolatedContext::GetInstance();
      for (PrefIdFsInfoMap::const_iterator it = pref_id_map_.begin();
           it != pref_id_map_.end();
           ++it) {
        isolated_context->RevokeFileSystem(it->second.fsid);
      }
      pref_id_map_.clear();
      no_references_callback_.Run();
    }
  }

  // A callback to call when the last RVH reference goes away.
  base::Closure no_references_callback_;

  // A map from the gallery preferences id to the file system information.
  PrefIdFsInfoMap pref_id_map_;

  // The set of render processes and web contents that may have references to
  // the file system ids this instance manages.
  RenderProcessHostRefCount rph_refs_;

  // A registrar for listening notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionGalleriesHost);
};

/******************
 * Public methods
 ******************/

// static
MediaFileSystemRegistry* MediaFileSystemRegistry::GetInstance() {
  return g_media_file_system_registry.Pointer();
}

// TODO(vandebo) We need to make this async so that we check that the
// galleries are attached (requires the file thread).
std::vector<MediaFileSystemRegistry::MediaFSInfo>
MediaFileSystemRegistry::GetMediaFileSystemsForExtension(
    const content::RenderViewHost* rvh,
    const extensions::Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile =
      Profile::FromBrowserContext(rvh->GetProcess()->GetBrowserContext());
  MediaGalleriesPreferences* preferences =
      MediaGalleriesPreferencesFactory::GetForProfile(profile);

  if (!ContainsKey(extension_hosts_map_, profile))
    AddAttachedMediaDeviceGalleries(preferences);
  MediaGalleryPrefIdSet galleries =
      preferences->GalleriesForExtension(*extension);
  ExtensionGalleriesHost* extension_host =
      extension_hosts_map_[profile][extension->id()].get();

  // If the extension has no galleries and it didn't have any last time, just
  // return the empty list. The second check is needed because of
  // http://crbug.com/145855.
  if (galleries.empty() && !extension_host)
    return std::vector<MediaFileSystemRegistry::MediaFSInfo>();

  if (!extension_host) {
    extension_host = new ExtensionGalleriesHost(
        base::Bind(&MediaFileSystemRegistry::OnExtensionGalleriesHostEmpty,
                   base::Unretained(this), profile, extension->id()));
    extension_hosts_map_[profile][extension->id()] = extension_host;
  }
  extension_host->ReferenceFromRVH(rvh);

  return extension_host->GetMediaFileSystems(galleries,
                                             preferences->known_galleries());
}

void MediaFileSystemRegistry::OnRemovableStorageAttached(
    const std::string& id, const string16& name,
    const FilePath::StringType& location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!MediaStorageUtil::IsMediaDevice(id))
    return;

  for (ExtensionGalleriesHostMap::iterator profile_it =
           extension_hosts_map_.begin();
       profile_it != extension_hosts_map_.end();
       ++profile_it) {
    MediaGalleriesPreferences* preferences =
        MediaGalleriesPreferencesFactory::GetForProfile(profile_it->first);
    preferences->AddGallery(id, name, FilePath(), false /*not user added*/);
  }
}

void MediaFileSystemRegistry::OnRemovableStorageDetached(
    const std::string& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Since revoking a gallery in the ExtensionGalleriesHost may cause it
  // to be removed from the map and therefore invalidate any iterator pointing
  // to it, this code first copies all the invalid gallery ids and the
  // extension hosts in which they may appear (per profile) and revoked it in
  // a second step.
  std::vector<InvalidatedGalleriesInfo> invalid_galleries_info;

  for (ExtensionGalleriesHostMap::iterator profile_it =
           extension_hosts_map_.begin();
       profile_it != extension_hosts_map_.end();
       ++profile_it) {
    MediaGalleriesPreferences* preferences =
        MediaGalleriesPreferencesFactory::GetForProfile(profile_it->first);
    InvalidatedGalleriesInfo invalid_galleries_in_profile;
    invalid_galleries_in_profile.pref_ids =
        preferences->LookUpGalleriesByDeviceId(id);

    for (ExtensionHostMap::const_iterator extension_host_it =
             profile_it->second.begin();
         extension_host_it != profile_it->second.end();
         ++extension_host_it) {
      invalid_galleries_in_profile.extension_hosts.insert(
          extension_host_it->second.get());
    }

    invalid_galleries_info.push_back(invalid_galleries_in_profile);
  }

  for (size_t i = 0; i < invalid_galleries_info.size(); i++) {
    for (std::set<ExtensionGalleriesHost*>::const_iterator extension_host_it =
             invalid_galleries_info[i].extension_hosts.begin();
         extension_host_it != invalid_galleries_info[i].extension_hosts.end();
         ++extension_host_it) {
      for (std::set<MediaGalleryPrefId>::const_iterator pref_id_it =
               invalid_galleries_info[i].pref_ids.begin();
           pref_id_it != invalid_galleries_info[i].pref_ids.end();
           ++pref_id_it) {
        (*extension_host_it)->RevokeGalleryByPrefId(*pref_id_it);
      }
    }
  }
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

void MediaFileSystemRegistry::AddAttachedMediaDeviceGalleries(
    MediaGalleriesPreferences* preferences) {
  // SystemMonitor may be NULL in unit tests.
  SystemMonitor* system_monitor = SystemMonitor::Get();
  if (!system_monitor)
    return;

  std::vector<SystemMonitor::RemovableStorageInfo> existing_devices =
      system_monitor->GetAttachedRemovableStorage();
  for (size_t i = 0; i < existing_devices.size(); i++) {
    if (!MediaStorageUtil::IsMediaDevice(existing_devices[i].device_id))
      continue;
    preferences->AddGallery(existing_devices[i].device_id,
                            existing_devices[i].name, FilePath(),
                            false /*not user added*/);
  }
}

void MediaFileSystemRegistry::OnExtensionGalleriesHostEmpty(
    Profile* profile, const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionGalleriesHostMap::iterator extension_hosts =
      extension_hosts_map_.find(profile);
  DCHECK(extension_hosts != extension_hosts_map_.end());
  ExtensionHostMap::size_type erase_count =
      extension_hosts->second.erase(extension_id);
  DCHECK_EQ(1U, erase_count);
  if (extension_hosts->second.empty())
    extension_hosts_map_.erase(extension_hosts);
}

}  // namespace chrome
