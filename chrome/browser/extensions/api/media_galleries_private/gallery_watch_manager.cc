// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GalleryWatchManager implementation.

#include "chrome/browser/extensions/api/media_galleries_private/gallery_watch_manager.h"

#include <list>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path_watcher.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

namespace {

using content::BrowserThread;

// Map to keep track of profile specific GalleryWatchManager objects.
// Key: Profile identifier.
// Value: GalleryWatchManager*.
// This map owns the GalleryWatchManager object.
typedef std::map<void*, extensions::GalleryWatchManager*> WatchManagerMap;
WatchManagerMap* g_gallery_watch_managers = NULL;

// Dispatches the gallery changed event on the UI thread.
void SendGalleryChangedEventOnUIThread(
    base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router,
    MediaGalleryPrefId gallery_id,
    const std::set<std::string>& extension_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (event_router.get())
    event_router->OnGalleryChanged(gallery_id, extension_ids);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//            GalleryWatchManager::GalleryFilePathWatcher                    //
///////////////////////////////////////////////////////////////////////////////

// This class does a recursive watch on the gallery file path and holds a list
// of extensions that are watching the gallery. When there is a file system
// activity within the gallery, GalleryFilePathWatcher notifies the interested
// extensions. This class lives on the file thread.
class GalleryWatchManager::GalleryFilePathWatcher
    : public base::RefCounted<GalleryFilePathWatcher> {
 public:
  // |on_destroyed_callback| is called when the last GalleryFilePathWatcher
  // reference goes away.
  GalleryFilePathWatcher(
      base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router,
      MediaGalleryPrefId gallery_id,
      const base::FilePath& path,
      const std::string& extension_id,
      const base::Closure& on_destroyed_callback);

  // Adds the extension reference to the watched gallery.
  void AddExtension(const std::string& extension_id);

  // Removes the extension reference to the watched gallery.
  void RemoveExtension(const std::string& extension_id);

  // Handles the extension unloaded/uninstalled event.
  void OnExtensionUnloaded(const std::string& extension_id);

  // Sets up the watch operation for the specified |gallery_path_|. On
  // success, returns true.
  bool SetupWatch();

  // Removes all the extension references when the browser profile is in
  // shutdown mode.
  void RemoveAllWatchReferences();

 private:
  friend class base::RefCounted<GalleryFilePathWatcher>;

  // Key: Extension identifier, e.g "qoueruoweuroiwueroiwujkshdf".
  // Value: Time at which the last gallery changed event is dispatched.
  //        Initialized to null Time value.
  typedef std::map<std::string, base::Time> ExtensionWatchInfoMap;

  // Private because GalleryFilePathWatcher is ref-counted.
  virtual ~GalleryFilePathWatcher();

  // FilePathWatcher callback.
  void OnFilePathChanged(const base::FilePath& path, bool error);

  // Remove the watch references for the extension specified by the
  // |extension_id|.
  void RemoveExtensionReferences(const std::string& extension_id);

  // Used to notify the interested extensions about the gallery changed event.
  base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router_;

  // The gallery identifier, e.g "1".
  MediaGalleryPrefId gallery_id_;

  // The gallery file path watcher.
  base::FilePathWatcher file_watcher_;

  // The gallery file path, e.g "C:\My Pictures".
  base::FilePath gallery_path_;

  // A callback to call when |this| object is destroyed.
  base::Closure on_destroyed_callback_;

  // Map to keep track of the extension and its corresponding watch count.
  ExtensionWatchInfoMap extension_watch_info_map_;

  // Used to provide a weak pointer to FilePathWatcher callback.
  base::WeakPtrFactory<GalleryFilePathWatcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GalleryFilePathWatcher);
};

GalleryWatchManager::GalleryFilePathWatcher::GalleryFilePathWatcher(
    base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router,
    MediaGalleryPrefId gallery_id,
    const base::FilePath& path,
    const std::string& extension_id,
    const base::Closure& on_destroyed_callback)
    : event_router_(event_router),
      gallery_id_(gallery_id),
      on_destroyed_callback_(on_destroyed_callback),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  gallery_path_ = path;
  AddExtension(extension_id);
}

void GalleryWatchManager::GalleryFilePathWatcher::AddExtension(
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (ContainsKey(extension_watch_info_map_, extension_id))
    return;
  extension_watch_info_map_[extension_id] = base::Time();
  AddRef();
}

void GalleryWatchManager::GalleryFilePathWatcher::RemoveExtension(
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (extension_watch_info_map_.erase(extension_id) == 1)
    Release();
}

void GalleryWatchManager::GalleryFilePathWatcher::OnExtensionUnloaded(
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  RemoveExtensionReferences(extension_id);
}

bool GalleryWatchManager::GalleryFilePathWatcher::SetupWatch() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return file_watcher_.Watch(
      gallery_path_, true,
      base::Bind(&GalleryFilePathWatcher::OnFilePathChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GalleryWatchManager::GalleryFilePathWatcher::RemoveAllWatchReferences() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  std::set<std::string> extension_ids;
  for (ExtensionWatchInfoMap::iterator iter = extension_watch_info_map_.begin();
       iter != extension_watch_info_map_.end(); ++iter)
    extension_ids.insert(iter->first);

  for (std::set<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it)
    RemoveExtensionReferences(*it);
}

GalleryWatchManager::GalleryFilePathWatcher::~GalleryFilePathWatcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  on_destroyed_callback_.Run();
}

void GalleryWatchManager::GalleryFilePathWatcher::OnFilePathChanged(
    const base::FilePath& path,
    bool error) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (error || (path != gallery_path_))
    return;

  std::set<std::string> extension_ids;
  for (ExtensionWatchInfoMap::iterator iter = extension_watch_info_map_.begin();
       iter != extension_watch_info_map_.end(); ++iter) {
    if (!iter->second.is_null()) {
      // Ignore gallery change event if it is received too frequently.
      // For example, when an user copies/deletes 1000 media files from a
      // gallery, this callback is called 1000 times within a span of 10ms.
      // GalleryWatchManager should not send 1000 gallery changed events to
      // the watching extension.
      const int kMinSecondsToIgnoreGalleryChangedEvent = 3;
      base::TimeDelta diff = base::Time::Now() - iter->second;
      if (diff.InSeconds() < kMinSecondsToIgnoreGalleryChangedEvent)
        continue;
    }
    iter->second = base::Time::Now();
    extension_ids.insert(iter->first);
  }
  if (!extension_ids.empty()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(SendGalleryChangedEventOnUIThread, event_router_,
                   gallery_id_, extension_ids));
  }
}

void GalleryWatchManager::GalleryFilePathWatcher::RemoveExtensionReferences(
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  ExtensionWatchInfoMap::iterator it =
      extension_watch_info_map_.find(extension_id);
  if (it == extension_watch_info_map_.end())
    return;
  extension_watch_info_map_.erase(it);
  Release();
}

///////////////////////////////////////////////////////////////////////////////
//                       GalleryWatchManager                                 //
///////////////////////////////////////////////////////////////////////////////

// static
GalleryWatchManager* GalleryWatchManager::GetForProfile(
    void* profile_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(profile_id);
  bool has_watch_manager = (g_gallery_watch_managers &&
                            GalleryWatchManager::HasForProfile(profile_id));
  if (!g_gallery_watch_managers)
    g_gallery_watch_managers = new WatchManagerMap;
  if (!has_watch_manager)
    (*g_gallery_watch_managers)[profile_id] = new GalleryWatchManager;
  return (*g_gallery_watch_managers)[profile_id];
}

// static
bool GalleryWatchManager::HasForProfile(void* profile_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(profile_id);
  if (!g_gallery_watch_managers)
    return false;
  WatchManagerMap::const_iterator it =
      g_gallery_watch_managers->find(profile_id);
  return (it != g_gallery_watch_managers->end());
}

// static
void GalleryWatchManager::OnProfileShutdown(void* profile_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(profile_id);
  if (!g_gallery_watch_managers || g_gallery_watch_managers->empty())
    return;
  WatchManagerMap::iterator it = g_gallery_watch_managers->find(profile_id);
  if (it == g_gallery_watch_managers->end())
    return;
  delete it->second;
  g_gallery_watch_managers->erase(it);
  if (g_gallery_watch_managers->empty()) {
    delete g_gallery_watch_managers;
    g_gallery_watch_managers = NULL;
  }
}

// static
bool GalleryWatchManager::SetupGalleryWatch(
    void* profile_id,
    MediaGalleryPrefId gallery_id,
    const base::FilePath& watch_path,
    const std::string& extension_id,
    base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return GalleryWatchManager::GetForProfile(profile_id)->StartGalleryWatch(
      gallery_id, watch_path, extension_id, event_router);
}

// static
void GalleryWatchManager::RemoveGalleryWatch(void* profile_id,
                                             const base::FilePath& watch_path,
                                             const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!GalleryWatchManager::HasForProfile(profile_id))
    return;
  GalleryWatchManager::GetForProfile(profile_id)->StopGalleryWatch(
      watch_path, extension_id);
}

void GalleryWatchManager::OnExtensionUnloaded(void* profile_id,
                                              const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!GalleryWatchManager::HasForProfile(profile_id))
    return;
  GalleryWatchManager::GetForProfile(profile_id)->HandleExtensionUnloadedEvent(
      extension_id);
}

GalleryWatchManager::GalleryWatchManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
}

GalleryWatchManager::~GalleryWatchManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DeleteAllWatchers();
}

bool GalleryWatchManager::StartGalleryWatch(
    MediaGalleryPrefId gallery_id,
    const base::FilePath& watch_path,
    const std::string& extension_id,
    base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  WatcherMap::const_iterator iter = gallery_watchers_.find(watch_path);
  if (iter != gallery_watchers_.end()) {
    // Already watched.
    iter->second->AddExtension(extension_id);
    return true;
  }

  // Need to add a new watcher.
  scoped_refptr<GalleryFilePathWatcher> watch(
      new GalleryFilePathWatcher(
          event_router, gallery_id, watch_path, extension_id,
          base::Bind(&GalleryWatchManager::RemoveGalleryFilePathWatcherEntry,
                     base::Unretained(this),
                     watch_path)));
  if (!watch->SetupWatch())
    return false;
  gallery_watchers_[watch_path] = watch.get();
  return true;
}

void GalleryWatchManager::StopGalleryWatch(
    const base::FilePath& watch_path,
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  WatcherMap::iterator iter = gallery_watchers_.find(watch_path);
  if (iter == gallery_watchers_.end())
    return;
  // Remove the renderer process for this watch.
  iter->second->RemoveExtension(extension_id);
}

void GalleryWatchManager::HandleExtensionUnloadedEvent(
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  std::list<base::FilePath> watchers_to_notify;
  for (WatcherMap::iterator iter = gallery_watchers_.begin();
       iter != gallery_watchers_.end(); ++iter)
    watchers_to_notify.push_back(iter->first);

  for (std::list<base::FilePath>::const_iterator path =
           watchers_to_notify.begin();
       path != watchers_to_notify.end(); ++path) {
     WatcherMap::iterator iter = gallery_watchers_.find(*path);
     if (iter == gallery_watchers_.end())
       continue;
     iter->second->OnExtensionUnloaded(extension_id);
  }
}

void GalleryWatchManager::DeleteAllWatchers() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (gallery_watchers_.empty())
    return;

  // Create a copy of |gallery_watchers_| to delete because
  // GalleryFilePathWatcher::RemoveAllWatchReferences will
  // eventually call GalleryWatchManager::RemoveGalleryFilePathWatcherEntry()
  // and modify |gallery_watchers_|.
  WatcherMap watchers_to_delete(gallery_watchers_);
  for (WatcherMap::const_iterator iter = watchers_to_delete.begin();
       iter != watchers_to_delete.end(); ++iter)
    iter->second->RemoveAllWatchReferences();
}

void GalleryWatchManager::RemoveGalleryFilePathWatcherEntry(
    const base::FilePath& watch_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  gallery_watchers_.erase(watch_path);
}

}  // namespace extensions
