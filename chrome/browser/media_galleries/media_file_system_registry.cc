// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_file_system_registry.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/media_galleries/gallery_watch_manager.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "chrome/browser/media_galleries/media_file_system_context.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "chrome/browser/media_galleries/media_galleries_histograms.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/media_galleries/media_scan_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/common/fileapi/file_system_mount_option.h"
#include "storage/common/fileapi/file_system_types.h"

using content::BrowserThread;
using content::NavigationController;
using content::RenderProcessHost;
using content::WebContents;
using storage::ExternalMountPoints;
using storage_monitor::MediaStorageUtil;
using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;

namespace {

struct InvalidatedGalleriesInfo {
  std::set<ExtensionGalleriesHost*> extension_hosts;
  std::set<MediaGalleryPrefId> pref_ids;
};

// Tracks the liveness of multiple RenderProcessHosts that the caller is
// interested in. Once all of the RPHs have closed or been destroyed a call
// back informs the caller.
class RPHReferenceManager {
 public:
  // |no_references_callback| is called when the last RenderViewHost reference
  // goes away. RenderViewHost references are added through ReferenceFromRVH().
  explicit RPHReferenceManager(const base::Closure& no_references_callback);
  virtual ~RPHReferenceManager();

  // Remove all references, but don't call |no_references_callback|.
  void Reset() { STLDeleteValues(&observer_map_); }

  // Returns true if there are no references;
  bool empty() const { return observer_map_.empty(); }

  // Adds a reference to the passed |rvh|. Calling this multiple times with
  // the same |rvh| is a no-op.
  void ReferenceFromRVH(const content::RenderViewHost* rvh);

 private:
  class RPHWebContentsObserver : public content::WebContentsObserver {
   public:
    RPHWebContentsObserver(RPHReferenceManager* manager,
                           WebContents* web_contents);

   private:
    // content::WebContentsObserver
    virtual void WebContentsDestroyed() OVERRIDE;
    virtual void NavigationEntryCommitted(
        const content::LoadCommittedDetails& load_details) OVERRIDE;

    RPHReferenceManager* manager_;
  };

  class RPHObserver : public content::RenderProcessHostObserver {
   public:
    RPHObserver(RPHReferenceManager* manager, RenderProcessHost* host);
    virtual ~RPHObserver();

    void AddWebContentsObserver(WebContents* web_contents);
    void RemoveWebContentsObserver(WebContents* web_contents);
    bool HasWebContentsObservers() {
      return observed_web_contentses_.size() > 0;
    }

   private:
    virtual void RenderProcessHostDestroyed(RenderProcessHost* host) OVERRIDE;

    RPHReferenceManager* manager_;
    RenderProcessHost* host_;
    typedef std::map<WebContents*, RPHWebContentsObserver*> WCObserverMap;
    WCObserverMap observed_web_contentses_;
  };
  typedef std::map<const RenderProcessHost*, RPHObserver*> RPHObserverMap;

  // Handlers for observed events.
  void OnRenderProcessHostDestroyed(RenderProcessHost* rph);
  void OnWebContentsDestroyedOrNavigated(WebContents* contents);

  // A callback to call when the last RVH reference goes away.
  base::Closure no_references_callback_;

  // The set of render processes and web contents that may have references to
  // the file system ids this instance manages.
  RPHObserverMap observer_map_;
};

RPHReferenceManager::RPHReferenceManager(
    const base::Closure& no_references_callback)
    : no_references_callback_(no_references_callback) {
}

RPHReferenceManager::~RPHReferenceManager() {
  Reset();
}

void RPHReferenceManager::ReferenceFromRVH(const content::RenderViewHost* rvh) {
  WebContents* contents = WebContents::FromRenderViewHost(rvh);
  RenderProcessHost* rph = contents->GetRenderProcessHost();
  RPHObserver* state = NULL;
  if (!ContainsKey(observer_map_, rph)) {
    state = new RPHObserver(this, rph);
    observer_map_[rph] = state;
  } else {
    state = observer_map_[rph];
  }

  state->AddWebContentsObserver(contents);
}

RPHReferenceManager::RPHWebContentsObserver::RPHWebContentsObserver(
    RPHReferenceManager* manager,
    WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      manager_(manager) {
}

void RPHReferenceManager::RPHWebContentsObserver::WebContentsDestroyed() {
  manager_->OnWebContentsDestroyedOrNavigated(web_contents());
}

void RPHReferenceManager::RPHWebContentsObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (load_details.is_in_page)
    return;

  manager_->OnWebContentsDestroyedOrNavigated(web_contents());
}

RPHReferenceManager::RPHObserver::RPHObserver(
    RPHReferenceManager* manager, RenderProcessHost* host)
    : manager_(manager),
      host_(host) {
  host->AddObserver(this);
}

RPHReferenceManager::RPHObserver::~RPHObserver() {
  STLDeleteValues(&observed_web_contentses_);
  if (host_)
    host_->RemoveObserver(this);
}

void RPHReferenceManager::RPHObserver::AddWebContentsObserver(
    WebContents* web_contents) {
  if (ContainsKey(observed_web_contentses_, web_contents))
    return;

  RPHWebContentsObserver* observer =
    new RPHWebContentsObserver(manager_, web_contents);
  observed_web_contentses_[web_contents] = observer;
}

void RPHReferenceManager::RPHObserver::RemoveWebContentsObserver(
    WebContents* web_contents) {
  WCObserverMap::iterator wco_iter =
      observed_web_contentses_.find(web_contents);
  DCHECK(wco_iter != observed_web_contentses_.end());
  delete wco_iter->second;
  observed_web_contentses_.erase(wco_iter);
}

void RPHReferenceManager::RPHObserver::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  host_ = NULL;
  manager_->OnRenderProcessHostDestroyed(host);
}

void RPHReferenceManager::OnRenderProcessHostDestroyed(
    RenderProcessHost* rph) {
  RPHObserverMap::iterator rph_info = observer_map_.find(rph);
  // This could be a potential problem if the RPH is navigated to a page on the
  // same renderer (triggering OnWebContentsDestroyedOrNavigated()) and then the
  // renderer crashes.
  if (rph_info == observer_map_.end()) {
    NOTREACHED();
    return;
  }
  delete rph_info->second;
  observer_map_.erase(rph_info);
  if (observer_map_.empty())
    no_references_callback_.Run();
}

void RPHReferenceManager::OnWebContentsDestroyedOrNavigated(
    WebContents* contents) {
  RenderProcessHost* rph = contents->GetRenderProcessHost();
  RPHObserverMap::iterator rph_info = observer_map_.find(rph);
  DCHECK(rph_info != observer_map_.end());

  rph_info->second->RemoveWebContentsObserver(contents);
  if (!rph_info->second->HasWebContentsObservers())
    OnRenderProcessHostDestroyed(rph);
}

}  // namespace

MediaFileSystemInfo::MediaFileSystemInfo(const base::string16& fs_name,
                                         const base::FilePath& fs_path,
                                         const std::string& filesystem_id,
                                         MediaGalleryPrefId pref_id,
                                         const std::string& transient_device_id,
                                         bool removable,
                                         bool media_device)
    : name(fs_name),
      path(fs_path),
      fsid(filesystem_id),
      pref_id(pref_id),
      transient_device_id(transient_device_id),
      removable(removable),
      media_device(media_device) {
}

MediaFileSystemInfo::MediaFileSystemInfo() {}
MediaFileSystemInfo::~MediaFileSystemInfo() {}

// The main owner of this class is
// |MediaFileSystemRegistry::extension_hosts_map_|, but a callback may
// temporarily hold a reference.
class ExtensionGalleriesHost
    : public base::RefCountedThreadSafe<ExtensionGalleriesHost> {
 public:
  // |no_references_callback| is called when the last RenderViewHost reference
  // goes away. RenderViewHost references are added through ReferenceFromRVH().
  ExtensionGalleriesHost(MediaFileSystemContext* file_system_context,
                         const base::FilePath& profile_path,
                         const std::string& extension_id,
                         const base::Closure& no_references_callback)
      : file_system_context_(file_system_context),
        profile_path_(profile_path),
        extension_id_(extension_id),
        no_references_callback_(no_references_callback),
        rph_refs_(base::Bind(&ExtensionGalleriesHost::CleanUp,
                             base::Unretained(this))) {
  }

  // For each gallery in the list of permitted |galleries|, checks if the
  // device is attached and if so looks up or creates a file system name and
  // passes the information needed for the renderer to create those file
  // system objects to the |callback|.
  void GetMediaFileSystems(const MediaGalleryPrefIdSet& galleries,
                           const MediaGalleriesPrefInfoMap& galleries_info,
                           const MediaFileSystemsCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Extract all the device ids so we can make sure they are attached.
    MediaStorageUtil::DeviceIdSet* device_ids =
        new MediaStorageUtil::DeviceIdSet;
    for (std::set<MediaGalleryPrefId>::const_iterator id = galleries.begin();
         id != galleries.end();
         ++id) {
      device_ids->insert(galleries_info.find(*id)->second.device_id);
    }
    MediaStorageUtil::FilterAttachedDevices(device_ids, base::Bind(
        &ExtensionGalleriesHost::GetMediaFileSystemsForAttachedDevices, this,
        base::Owned(device_ids), galleries, galleries_info, callback));
  }

  // Checks if |gallery| is attached and if so, registers the file system and
  // then calls |callback| with the result.
  void RegisterMediaFileSystem(
      const MediaGalleryPrefInfo& gallery,
      const base::Callback<void(base::File::Error result)>& callback) {
    // Extract all the device ids so we can make sure they are attached.
    MediaStorageUtil::DeviceIdSet* device_ids =
        new MediaStorageUtil::DeviceIdSet;
    device_ids->insert(gallery.device_id);
    MediaStorageUtil::FilterAttachedDevices(device_ids, base::Bind(
        &ExtensionGalleriesHost::RegisterAttachedMediaFileSystem, this,
        base::Owned(device_ids), gallery, callback));
  }

  // Revoke the file system for |id| if this extension has created one for |id|.
  void RevokeGalleryByPrefId(MediaGalleryPrefId id) {
    PrefIdFsInfoMap::iterator gallery = pref_id_map_.find(id);
    if (gallery == pref_id_map_.end())
      return;

    file_system_context_->RevokeFileSystem(gallery->second.fsid);
    pref_id_map_.erase(gallery);

    if (pref_id_map_.empty()) {
      rph_refs_.Reset();
      CleanUp();
    }
  }

  // Indicate that the passed |rvh| will reference the file system ids created
  // by this class.
  void ReferenceFromRVH(const content::RenderViewHost* rvh) {
    rph_refs_.ReferenceFromRVH(rvh);
  }

 private:
  typedef std::map<MediaGalleryPrefId, MediaFileSystemInfo> PrefIdFsInfoMap;

  // Private destructor and friend declaration for ref counted implementation.
  friend class base::RefCountedThreadSafe<ExtensionGalleriesHost>;

  virtual ~ExtensionGalleriesHost() {
    DCHECK(rph_refs_.empty());
    DCHECK(pref_id_map_.empty());
  }

  void GetMediaFileSystemsForAttachedDevices(
      const MediaStorageUtil::DeviceIdSet* attached_devices,
      const MediaGalleryPrefIdSet& galleries,
      const MediaGalleriesPrefInfoMap& galleries_info,
      const MediaFileSystemsCallback& callback) {
    std::vector<MediaFileSystemInfo> result;

    if (rph_refs_.empty()) {
      // We're actually in the middle of shutdown, and Filter...() lagging
      // which can invoke this method interleaved in the destruction callback
      // sequence and re-populate pref_id_map_.
      callback.Run(result);
      return;
    }

    for (std::set<MediaGalleryPrefId>::const_iterator pref_id_it =
             galleries.begin();
         pref_id_it != galleries.end();
         ++pref_id_it) {
      const MediaGalleryPrefId& pref_id = *pref_id_it;
      const MediaGalleryPrefInfo& gallery_info =
          galleries_info.find(pref_id)->second;
      const std::string& device_id = gallery_info.device_id;
      if (!ContainsKey(*attached_devices, device_id))
        continue;

      PrefIdFsInfoMap::const_iterator existing_info =
          pref_id_map_.find(pref_id);
      if (existing_info != pref_id_map_.end()) {
        result.push_back(existing_info->second);
        continue;
      }

      base::FilePath path = gallery_info.AbsolutePath();
      if (!MediaStorageUtil::CanCreateFileSystem(device_id, path))
        continue;

      std::string fs_name = MediaFileSystemBackend::ConstructMountName(
          profile_path_, extension_id_, pref_id);
      if (!file_system_context_->RegisterFileSystem(device_id, fs_name, path))
        continue;

      MediaFileSystemInfo new_entry(
          gallery_info.GetGalleryDisplayName(),
          file_system_context_->GetRegisteredPath(fs_name),
          fs_name,
          pref_id,
          GetTransientIdForRemovableDeviceId(device_id),
          StorageInfo::IsRemovableDevice(device_id),
          StorageInfo::IsMediaDevice(device_id));
      result.push_back(new_entry);
      pref_id_map_[pref_id] = new_entry;
    }

    if (result.size() == 0) {
      rph_refs_.Reset();
      CleanUp();
    }

    DCHECK_EQ(pref_id_map_.size(), result.size());
    callback.Run(result);
  }

  void RegisterAttachedMediaFileSystem(
      const MediaStorageUtil::DeviceIdSet* attached_device,
      const MediaGalleryPrefInfo& gallery,
      const base::Callback<void(base::File::Error result)>& callback) {
    base::File::Error result = base::File::FILE_ERROR_NOT_FOUND;

    // If rph_refs is empty then we're actually in the middle of shutdown, and
    // Filter...() lagging which can invoke this method interleaved in the
    // destruction callback sequence and re-populate pref_id_map_.
    if (!attached_device->empty() && !rph_refs_.empty()) {
      std::string fs_name = MediaFileSystemBackend::ConstructMountName(
          profile_path_, extension_id_, gallery.pref_id);
      base::FilePath path = gallery.AbsolutePath();
      const std::string& device_id = gallery.device_id;

      if (ContainsKey(pref_id_map_, gallery.pref_id)) {
        result = base::File::FILE_OK;
      } else if (MediaStorageUtil::CanCreateFileSystem(device_id, path) &&
                 file_system_context_->RegisterFileSystem(device_id, fs_name,
                                                          path)) {
        result = base::File::FILE_OK;
        pref_id_map_[gallery.pref_id] = MediaFileSystemInfo(
            gallery.GetGalleryDisplayName(),
            file_system_context_->GetRegisteredPath(fs_name),
            fs_name,
            gallery.pref_id,
            GetTransientIdForRemovableDeviceId(device_id),
            StorageInfo::IsRemovableDevice(device_id),
            StorageInfo::IsMediaDevice(device_id));
      }
    }

    if (pref_id_map_.empty()) {
      rph_refs_.Reset();
      CleanUp();
    }
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback, result));
  }

  std::string GetTransientIdForRemovableDeviceId(const std::string& device_id) {
    if (!StorageInfo::IsRemovableDevice(device_id))
      return std::string();

    return StorageMonitor::GetInstance()->GetTransientIdForDeviceId(device_id);
  }

  void CleanUp() {
    DCHECK(rph_refs_.empty());
    for (PrefIdFsInfoMap::const_iterator it = pref_id_map_.begin();
         it != pref_id_map_.end();
         ++it) {
      file_system_context_->RevokeFileSystem(it->second.fsid);
    }
    pref_id_map_.clear();

    no_references_callback_.Run();
  }

  // MediaFileSystemRegistry owns |this| and |file_system_context_|, so it's
  // safe to store a raw pointer.
  MediaFileSystemContext* file_system_context_;

  // Path for the active profile.
  const base::FilePath profile_path_;

  // Id of the extension this host belongs to.
  const std::string extension_id_;

  // A callback to call when the last RVH reference goes away.
  base::Closure no_references_callback_;

  // A map from the gallery preferences id to the file system information.
  PrefIdFsInfoMap pref_id_map_;

  // The set of render processes and web contents that may have references to
  // the file system ids this instance manages.
  RPHReferenceManager rph_refs_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionGalleriesHost);
};

/******************
 * Public methods
 ******************/

void MediaFileSystemRegistry::GetMediaFileSystemsForExtension(
    const content::RenderViewHost* rvh,
    const extensions::Extension* extension,
    const MediaFileSystemsCallback& callback) {
  // TODO(tommycli): Change to DCHECK after fixing http://crbug.com/374330.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile =
      Profile::FromBrowserContext(rvh->GetProcess()->GetBrowserContext());
  MediaGalleriesPreferences* preferences = GetPreferences(profile);
  MediaGalleryPrefIdSet galleries =
      preferences->GalleriesForExtension(*extension);

  if (galleries.empty()) {
    callback.Run(std::vector<MediaFileSystemInfo>());
    return;
  }

  ExtensionGalleriesHost* extension_host =
      GetExtensionGalleryHost(profile, preferences, extension->id());

  // This must come before the GetMediaFileSystems call to make sure the
  // RVH of the context is referenced before the filesystems are retrieved.
  extension_host->ReferenceFromRVH(rvh);

  extension_host->GetMediaFileSystems(galleries, preferences->known_galleries(),
                                      callback);
}

void MediaFileSystemRegistry::RegisterMediaFileSystemForExtension(
    const content::RenderViewHost* rvh,
    const extensions::Extension* extension,
    MediaGalleryPrefId pref_id,
    const base::Callback<void(base::File::Error result)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(kInvalidMediaGalleryPrefId, pref_id);

  Profile* profile =
      Profile::FromBrowserContext(rvh->GetProcess()->GetBrowserContext());
  MediaGalleriesPreferences* preferences = GetPreferences(profile);
  MediaGalleriesPrefInfoMap::const_iterator gallery =
      preferences->known_galleries().find(pref_id);
  MediaGalleryPrefIdSet permitted_galleries =
      preferences->GalleriesForExtension(*extension);

  if (gallery == preferences->known_galleries().end() ||
      !ContainsKey(permitted_galleries, pref_id)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(callback, base::File::FILE_ERROR_NOT_FOUND));
    return;
  }

  ExtensionGalleriesHost* extension_host =
      GetExtensionGalleryHost(profile, preferences, extension->id());

  // This must come before the GetMediaFileSystems call to make sure the
  // RVH of the context is referenced before the filesystems are retrieved.
  extension_host->ReferenceFromRVH(rvh);

  extension_host->RegisterMediaFileSystem(gallery->second, callback);
}

MediaGalleriesPreferences* MediaFileSystemRegistry::GetPreferences(
    Profile* profile) {
  // Create an empty ExtensionHostMap for this profile on first initialization.
  if (!ContainsKey(extension_hosts_map_, profile)) {
    extension_hosts_map_[profile] = ExtensionHostMap();
    media_galleries::UsageCount(media_galleries::PROFILES_WITH_USAGE);
  }

  return MediaGalleriesPreferencesFactory::GetForProfile(profile);
}

MediaScanManager* MediaFileSystemRegistry::media_scan_manager() {
  if (!media_scan_manager_)
    media_scan_manager_.reset(new MediaScanManager);
  return media_scan_manager_.get();
}

GalleryWatchManager* MediaFileSystemRegistry::gallery_watch_manager() {
  if (!gallery_watch_manager_)
    gallery_watch_manager_.reset(new GalleryWatchManager);
  return gallery_watch_manager_.get();
}

void MediaFileSystemRegistry::OnRemovableStorageDetached(
    const StorageInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
    MediaGalleriesPreferences* preferences = GetPreferences(profile_it->first);
    // If |preferences| is not yet initialized, it won't contain any galleries.
    if (!preferences->IsInitialized())
      continue;

    InvalidatedGalleriesInfo invalid_galleries_in_profile;
    invalid_galleries_in_profile.pref_ids =
        preferences->LookUpGalleriesByDeviceId(info.device_id());

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

class MediaFileSystemRegistry::MediaFileSystemContextImpl
    : public MediaFileSystemContext {
 public:
  MediaFileSystemContextImpl() {}
  virtual ~MediaFileSystemContextImpl() {}

  virtual bool RegisterFileSystem(const std::string& device_id,
                                  const std::string& fs_name,
                                  const base::FilePath& path) OVERRIDE {
    if (StorageInfo::IsMassStorageDevice(device_id)) {
      return RegisterFileSystemForMassStorage(device_id, fs_name, path);
    } else {
      return RegisterFileSystemForMTPDevice(device_id, fs_name, path);
    }
  }

  virtual void RevokeFileSystem(const std::string& fs_name) OVERRIDE {
    ImportedMediaGalleryRegistry* imported_registry =
        ImportedMediaGalleryRegistry::GetInstance();
    if (imported_registry->RevokeImportedFilesystemOnUIThread(fs_name))
      return;

    ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(fs_name);

    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &MTPDeviceMapService::RevokeMTPFileSystem,
        base::Unretained(MTPDeviceMapService::GetInstance()),
        fs_name));
  }

  virtual base::FilePath GetRegisteredPath(
      const std::string& fs_name) const OVERRIDE {
    base::FilePath result;
    if (!ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(fs_name,
                                                                     &result)) {
      return base::FilePath();
    }
    return result;
  }

 private:
  // Registers and returns the file system id for the mass storage device
  // specified by |device_id| and |path|.
  bool RegisterFileSystemForMassStorage(const std::string& device_id,
                                        const std::string& fs_name,
                                        const base::FilePath& path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(StorageInfo::IsMassStorageDevice(device_id));

    // Sanity checks for |path|.
    CHECK(path.IsAbsolute());
    CHECK(!path.ReferencesParent());

    // TODO(gbillock): refactor ImportedMediaGalleryRegistry to delegate this
    // call tree, probably by having it figure out by device id what
    // registration is needed, or having per-device-type handlers at the
    // next higher level.
    bool result = false;
    if (StorageInfo::IsITunesDevice(device_id)) {
      ImportedMediaGalleryRegistry* registry =
          ImportedMediaGalleryRegistry::GetInstance();
      result = registry->RegisterITunesFilesystemOnUIThread(fs_name, path);
    } else if (StorageInfo::IsPicasaDevice(device_id)) {
      ImportedMediaGalleryRegistry* registry =
          ImportedMediaGalleryRegistry::GetInstance();
      result = registry->RegisterPicasaFilesystemOnUIThread(fs_name, path);
    } else if (StorageInfo::IsIPhotoDevice(device_id)) {
      ImportedMediaGalleryRegistry* registry =
          ImportedMediaGalleryRegistry::GetInstance();
      result = registry->RegisterIPhotoFilesystemOnUIThread(fs_name, path);
    } else {
      result = ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          fs_name,
          storage::kFileSystemTypeNativeMedia,
          storage::FileSystemMountOption(),
          path);
    }
    return result;
  }

  bool RegisterFileSystemForMTPDevice(const std::string& device_id,
                                      const std::string fs_name,
                                      const base::FilePath& path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!StorageInfo::IsMassStorageDevice(device_id));

    // Sanity checks for |path|.
    CHECK(MediaStorageUtil::CanCreateFileSystem(device_id, path));
    bool result = ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        fs_name,
        storage::kFileSystemTypeDeviceMedia,
        storage::FileSystemMountOption(),
        path);
    CHECK(result);
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &MTPDeviceMapService::RegisterMTPFileSystem,
        base::Unretained(MTPDeviceMapService::GetInstance()),
        path.value(), fs_name));
    return result;
  }

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemContextImpl);
};

// Constructor in 'private' section because depends on private class definition.
MediaFileSystemRegistry::MediaFileSystemRegistry()
    : file_system_context_(new MediaFileSystemContextImpl) {
  StorageMonitor::GetInstance()->AddObserver(this);
}

MediaFileSystemRegistry::~MediaFileSystemRegistry() {
  // TODO(gbillock): This is needed because the unit test uses the
  // g_browser_process registry. We should create one in the unit test,
  // and then can remove this.
  if (StorageMonitor::GetInstance())
    StorageMonitor::GetInstance()->RemoveObserver(this);
}

void MediaFileSystemRegistry::OnPermissionRemoved(
    MediaGalleriesPreferences* prefs,
    const std::string& extension_id,
    MediaGalleryPrefId pref_id) {
  Profile* profile = prefs->profile();
  ExtensionGalleriesHostMap::const_iterator host_map_it =
      extension_hosts_map_.find(profile);
  DCHECK(host_map_it != extension_hosts_map_.end());
  const ExtensionHostMap& extension_host_map = host_map_it->second;
  ExtensionHostMap::const_iterator gallery_host_it =
      extension_host_map.find(extension_id);
  if (gallery_host_it == extension_host_map.end())
    return;
  gallery_host_it->second->RevokeGalleryByPrefId(pref_id);
}

void MediaFileSystemRegistry::OnGalleryRemoved(
    MediaGalleriesPreferences* prefs,
    MediaGalleryPrefId pref_id) {
  Profile* profile = prefs->profile();
  // Get the Extensions, MediaGalleriesPreferences and ExtensionHostMap for
  // |profile|.
  const ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const extensions::ExtensionSet* extensions_set =
      extension_service->extensions();
  ExtensionGalleriesHostMap::const_iterator host_map_it =
      extension_hosts_map_.find(profile);
  DCHECK(host_map_it != extension_hosts_map_.end());
  const ExtensionHostMap& extension_host_map = host_map_it->second;

  // Go through ExtensionHosts, and remove indicated gallery, if any.
  // RevokeGalleryByPrefId() may end up deleting from |extension_host_map| and
  // even delete |extension_host_map| altogether. So do this in two loops to
  // avoid using an invalidated iterator or deleted map.
  std::vector<const extensions::Extension*> extensions;
  for (ExtensionHostMap::const_iterator it = extension_host_map.begin();
       it != extension_host_map.end();
       ++it) {
    extensions.push_back(extensions_set->GetByID(it->first));
  }
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (!ContainsKey(extension_hosts_map_, profile))
      break;
    ExtensionHostMap::const_iterator gallery_host_it =
        extension_host_map.find(extensions[i]->id());
    if (gallery_host_it == extension_host_map.end())
      continue;
    gallery_host_it->second->RevokeGalleryByPrefId(pref_id);
  }
}

ExtensionGalleriesHost* MediaFileSystemRegistry::GetExtensionGalleryHost(
    Profile* profile,
    MediaGalleriesPreferences* preferences,
    const std::string& extension_id) {
  ExtensionGalleriesHostMap::iterator extension_hosts =
      extension_hosts_map_.find(profile);
  // GetPreferences(), which had to be called because preferences is an
  // argument, ensures that profile is in the map.
  DCHECK(extension_hosts != extension_hosts_map_.end());
  if (extension_hosts->second.empty())
    preferences->AddGalleryChangeObserver(this);

  ExtensionGalleriesHost* result = extension_hosts->second[extension_id].get();
  if (!result) {
    result = new ExtensionGalleriesHost(
        file_system_context_.get(),
        profile->GetPath(),
        extension_id,
        base::Bind(&MediaFileSystemRegistry::OnExtensionGalleriesHostEmpty,
                   base::Unretained(this),
                   profile,
                   extension_id));
    extension_hosts_map_[profile][extension_id] = result;
  }
  return result;
}

void MediaFileSystemRegistry::OnExtensionGalleriesHostEmpty(
    Profile* profile, const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionGalleriesHostMap::iterator extension_hosts =
      extension_hosts_map_.find(profile);
  DCHECK(extension_hosts != extension_hosts_map_.end());
  ExtensionHostMap::size_type erase_count =
      extension_hosts->second.erase(extension_id);
  DCHECK_EQ(1U, erase_count);
  if (extension_hosts->second.empty()) {
    // When a profile has no ExtensionGalleriesHosts left, remove the
    // matching gallery-change-watcher since it is no longer needed. Leave the
    // |extension_hosts| entry alone, since it indicates the profile has been
    // previously used.
    MediaGalleriesPreferences* preferences = GetPreferences(profile);
    preferences->RemoveGalleryChangeObserver(this);
  }
}
