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
#include "base/time.h"
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
    chrome::MediaGalleryPrefId gallery_id,
    const std::set<std::string>& extension_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
      chrome::MediaGalleryPrefId gallery_id,
      const FilePath& path,
      const std::string& extension_id,
      const base::Closure& on_destroyed_callback);

  // Adds the extension reference to the watched gallery.
  void AddExtension(const std::string& extension_id);

  // Removes the extension reference to the watched gallery.
  void RemoveExtension(const std::string& extension_id);

  // Handles the extension unloaded/uninstalled/destroyed event.
  void OnExtensionDestroyed(const std::string& extension_id);

  // Sets up the watch operation for the specified |gallery_path_|. On
  // success, returns true.
  bool SetupWatch();

  // Removes all the extension references when the browser profile is in
  // shutdown mode.
  void RemoveAllWatchReferences();

 private:
  friend class base::RefCounted<GalleryFilePathWatcher>;

  // Keeps track of extension watch details.
  struct ExtensionWatchInfo {
    ExtensionWatchInfo();

    // Number of watches in this extension, e.g "3"
    int watch_count;

    // Used to manage the gallery changed events.
    base::Time last_gallery_changed_event;
  };

  typedef std::map<std::string, ExtensionWatchInfo> ExtensionWatchInfoMap;

  // Private because GalleryFilePathWatcher is ref-counted.
  virtual ~GalleryFilePathWatcher();

  // FilePathWatcher callback.
  void OnFilePathChanged(const FilePath& path, bool error);

  // Remove the watch references for the extension specified by the
  // |extension_id|.
  void RemoveExtensionReferences(const std::string& extension_id);

  // Used to notify the interested extensions about the gallery changed event.
  base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router_;

  // The gallery identifier, e.g "1".
  chrome::MediaGalleryPrefId gallery_id_;

  // The gallery file path watcher.
  base::files::FilePathWatcher file_watcher_;

  // The gallery file path, e.g "C:\My Pictures".
  FilePath gallery_path_;

  // A callback to call when |this| object is destroyed.
  base::Closure on_destroyed_callback_;

  // Map to keep track of the extension and its corresponding watch count.
  // Key: Extension identifier, e.g "qoueruoweuroiwueroiwujkshdf".
  // Value: Watch information.
  ExtensionWatchInfoMap extension_watch_info_map_;

  // Used to provide a weak pointer to FilePathWatcher callback.
  base::WeakPtrFactory<GalleryFilePathWatcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GalleryFilePathWatcher);
};

GalleryWatchManager::GalleryFilePathWatcher::GalleryFilePathWatcher(
    base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router,
    chrome::MediaGalleryPrefId gallery_id,
    const FilePath& path,
    const std::string& extension_id,
    const base::Closure& on_destroyed_callback)
    : event_router_(event_router),
      gallery_id_(gallery_id),
      on_destroyed_callback_(on_destroyed_callback),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  gallery_path_ = path;
  AddExtension(extension_id);
}

void GalleryWatchManager::GalleryFilePathWatcher::AddExtension(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  extension_watch_info_map_[extension_id].watch_count++;
  AddRef();
}

void GalleryWatchManager::GalleryFilePathWatcher::RemoveExtension(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  ExtensionWatchInfoMap::iterator it =
      extension_watch_info_map_.find(extension_id);
  if (it == extension_watch_info_map_.end())
    return;
  // If entry found - decrease it's count and remove if necessary
  it->second.watch_count--;
  if (0 == it->second.watch_count)
    extension_watch_info_map_.erase(it);
  Release();
}

void GalleryWatchManager::GalleryFilePathWatcher::OnExtensionDestroyed(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  RemoveExtensionReferences(extension_id);
}

bool GalleryWatchManager::GalleryFilePathWatcher::SetupWatch() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return file_watcher_.Watch(
      gallery_path_, true,
      base::Bind(&GalleryFilePathWatcher::OnFilePathChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GalleryWatchManager::GalleryFilePathWatcher::RemoveAllWatchReferences() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::set<std::string> extension_ids;
  for (ExtensionWatchInfoMap::iterator iter = extension_watch_info_map_.begin();
       iter != extension_watch_info_map_.end(); ++iter)
    extension_ids.insert(iter->first);

  for (std::set<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it)
    RemoveExtensionReferences(*it);
}

GalleryWatchManager::GalleryFilePathWatcher::~GalleryFilePathWatcher() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  on_destroyed_callback_.Run();
}

GalleryWatchManager::GalleryFilePathWatcher::ExtensionWatchInfo::
ExtensionWatchInfo()
    : watch_count(0) {
}

void GalleryWatchManager::GalleryFilePathWatcher::OnFilePathChanged(
    const FilePath& path,
    bool error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (error || (path != gallery_path_))
    return;

  std::set<std::string> extension_ids;
  for (ExtensionWatchInfoMap::iterator iter = extension_watch_info_map_.begin();
       iter != extension_watch_info_map_.end(); ++iter) {
    if (!iter->second.last_gallery_changed_event.is_null()) {
      // Ignore gallery change event if it is received too frequently.
      // For example, when an user copies/deletes 1000 media files from a
      // gallery, this callback is called 1000 times within a span of 10ms.
      // GalleryWatchManager should not send 1000 gallery changed events to
      // the watching extension.
      const int kMinSecondsToIgnoreGalleryChangedEvent = 3;
      base::TimeDelta diff =
          base::Time::Now() - iter->second.last_gallery_changed_event;
      if (diff.InSeconds() < kMinSecondsToIgnoreGalleryChangedEvent)
        continue;
    }
    iter->second.last_gallery_changed_event = base::Time::Now();
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  ExtensionWatchInfoMap::iterator it =
      extension_watch_info_map_.find(extension_id);
  if (it == extension_watch_info_map_.end())
    return;
  int watch_count = it->second.watch_count;
  extension_watch_info_map_.erase(it);
  for (int i = 0; i < watch_count; ++i)
    Release();
}

///////////////////////////////////////////////////////////////////////////////
//                       GalleryWatchManager                                 //
///////////////////////////////////////////////////////////////////////////////

// static
GalleryWatchManager* GalleryWatchManager::GetForProfile(
    void* profile_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(profile_id);
  if (!g_gallery_watch_managers)
    return false;
  WatchManagerMap::const_iterator it =
      g_gallery_watch_managers->find(profile_id);
  return (it != g_gallery_watch_managers->end());
}

// static
void GalleryWatchManager::OnProfileShutdown(void* profile_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(profile_id);
  if (!g_gallery_watch_managers || g_gallery_watch_managers->empty())
    return;
  WatchManagerMap::iterator it = g_gallery_watch_managers->find(profile_id);
  if (it == g_gallery_watch_managers->end())
    return;
  delete it->second;
  g_gallery_watch_managers->erase(it);
  if (g_gallery_watch_managers->empty())
    delete g_gallery_watch_managers;
}

GalleryWatchManager::~GalleryWatchManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DeleteAllWatchers();
}

bool GalleryWatchManager::StartGalleryWatch(
    chrome::MediaGalleryPrefId gallery_id,
    const FilePath& watch_path,
    const std::string& extension_id,
    base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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
    const FilePath& watch_path,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WatcherMap::iterator iter = gallery_watchers_.find(watch_path);
  if (iter == gallery_watchers_.end())
    return;
  // Remove the renderer process for this watch.
  iter->second->RemoveExtension(extension_id);
}

void GalleryWatchManager::OnExtensionDestroyed(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::list<FilePath> watchers_to_notify;
  for (WatcherMap::iterator iter = gallery_watchers_.begin();
       iter != gallery_watchers_.end(); ++iter)
    watchers_to_notify.push_back(iter->first);

  for (std::list<FilePath>::const_iterator path = watchers_to_notify.begin();
       path != watchers_to_notify.end(); ++path) {
     WatcherMap::iterator iter = gallery_watchers_.find(*path);
     if (iter == gallery_watchers_.end())
       continue;
     iter->second->OnExtensionDestroyed(extension_id);
  }
}

GalleryWatchManager::GalleryWatchManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void GalleryWatchManager::DeleteAllWatchers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (gallery_watchers_.empty())
    return;

  for (WatcherMap::iterator iter = gallery_watchers_.begin();
       iter != gallery_watchers_.end(); ++iter)
    iter->second->RemoveAllWatchReferences();
}

void GalleryWatchManager::RemoveGalleryFilePathWatcherEntry(
    const FilePath& watch_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  gallery_watchers_.erase(watch_path);
}

}  // namespace extensions
