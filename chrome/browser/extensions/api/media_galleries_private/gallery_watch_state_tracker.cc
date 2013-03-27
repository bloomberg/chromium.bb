// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GalleryWatchStateTracker implementation.

#include "chrome/browser/extensions/api/media_galleries_private/gallery_watch_state_tracker.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/media_galleries_private/gallery_watch_manager.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

namespace {

// State store key to track the registered gallery watchers for the extensions.
const char kRegisteredGalleryWatchers[] = "media_gallery_watchers";

// Converts the storage |list| value to WatchedGalleryIds.
chrome::MediaGalleryPrefIdSet WatchedGalleryIdsFromValue(
    const base::ListValue* list) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  chrome::MediaGalleryPrefIdSet gallery_ids;
  std::string gallery_id_str;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    if (!list->GetString(i, &gallery_id_str) || gallery_id_str.empty())
      continue;
    chrome::MediaGalleryPrefId gallery_id;
    if (base::StringToUint64(gallery_id_str, &gallery_id))
      gallery_ids.insert(gallery_id);
  }
  return gallery_ids;
}

// Converts WatchedGalleryIds to a storage list value.
scoped_ptr<base::ListValue> WatchedGalleryIdsToValue(
    const chrome::MediaGalleryPrefIdSet gallery_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_ptr<base::ListValue> list(new ListValue());
  for (chrome::MediaGalleryPrefIdSet::const_iterator id_iter =
           gallery_ids.begin();
       id_iter != gallery_ids.end(); ++id_iter)
    list->AppendString(base::Uint64ToString(*id_iter));
  return list.Pass();
}

// Looks up an extension by ID. Does not include disabled extensions.
const Extension* GetExtensionById(Profile* profile,
                                  const std::string& extension_id) {
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return NULL;
  return service->GetExtensionById(extension_id, false);
}

// Returns the initialized media galleries preferences for the specified
// |profile|.
chrome::MediaGalleriesPreferences* GetMediaGalleryPreferences(
    Profile* profile) {
  return g_browser_process->media_file_system_registry()->GetPreferences(
      profile);
}

}  // namespace

GalleryWatchStateTracker::GalleryWatchStateTracker(Profile* profile)
    : profile_(profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
}

GalleryWatchStateTracker::~GalleryWatchStateTracker() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

// static
GalleryWatchStateTracker* GalleryWatchStateTracker::GetForProfile(
    Profile* profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if defined(OS_WIN)
  // Gallery watch operation is supported only on windows.
  // Please refer to crbug.com/144491 for more details.
  DCHECK(profile);
  MediaGalleriesPrivateAPI* private_api =
      MediaGalleriesPrivateAPI::Get(profile);
  if (!private_api)
    return NULL;  // In unit tests, we don't have a MediaGalleriesPrivateAPI.
  return private_api->GetGalleryWatchStateTracker();
#endif
  return NULL;
}

void GalleryWatchStateTracker::OnGalleryPermissionChanged(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery_id,
    bool has_permission) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Granted gallery permission.
  if (has_permission && HasGalleryWatchInfo(extension_id, gallery_id, false)) {
    SetupGalleryWatch(extension_id, gallery_id);
    return;
  }

  // Revoked gallery permission.
  if (!has_permission && HasGalleryWatchInfo(extension_id, gallery_id, true))
    RemoveGalleryWatch(extension_id, gallery_id);
}

chrome::MediaGalleryPrefIdSet
GalleryWatchStateTracker::GetAllWatchedGalleryIDsForExtension(
    const std::string& extension_id) const {
  chrome::MediaGalleryPrefIdSet gallery_ids;
  WatchedExtensionsMap::const_iterator extension_id_iter =
      watched_extensions_map_.find(extension_id);
  if (extension_id_iter != watched_extensions_map_.end()) {
    for (WatchedGalleriesMap::const_iterator gallery_id_iter =
             extension_id_iter->second.begin();
         gallery_id_iter != extension_id_iter->second.end();
         ++gallery_id_iter) {
      gallery_ids.insert(gallery_id_iter->first);
    }
  }
  return gallery_ids;
}

void GalleryWatchStateTracker::RemoveAllGalleryWatchersForExtension(
    const std::string& extension_id) {
  WatchedExtensionsMap::iterator extension_id_iter =
      watched_extensions_map_.find(extension_id);
  if (extension_id_iter == watched_extensions_map_.end())
    return;
  const WatchedGalleriesMap& galleries = extension_id_iter->second;
  for (WatchedGalleriesMap::const_iterator gallery_id_iter = galleries.begin();
       gallery_id_iter != galleries.end(); ++gallery_id_iter)
    RemoveGalleryWatch(extension_id, gallery_id_iter->second);
  watched_extensions_map_.erase(extension_id_iter);
  WriteToStorage(extension_id);
}

void GalleryWatchStateTracker::OnGalleryWatchAdded(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  bool update_storage =
      AddWatchedGalleryIdInfoForExtension(extension_id, gallery_id);
  if (update_storage)
    WriteToStorage(extension_id);
}

void GalleryWatchStateTracker::OnGalleryWatchRemoved(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!ContainsKey(watched_extensions_map_, extension_id))
    return;
  watched_extensions_map_[extension_id].erase(gallery_id);
  if (watched_extensions_map_[extension_id].empty())
    watched_extensions_map_.erase(extension_id);
  WriteToStorage(extension_id);
}

void GalleryWatchStateTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
      if (!storage)
        return;
      storage->GetExtensionValue(
          extension->id(),
          kRegisteredGalleryWatchers,
          base::Bind(&GalleryWatchStateTracker::ReadFromStorage,
                     AsWeakPtr(),
                     extension->id()));
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      Extension* extension = const_cast<Extension*>(
          content::Details<extensions::UnloadedExtensionInfo>(
              details)->extension);
      DCHECK(extension);
      if (!ContainsKey(watched_extensions_map_, extension->id()))
        return;
      content::BrowserThread::PostTask(
          content::BrowserThread::FILE, FROM_HERE,
          base::Bind(&GalleryWatchManager::OnExtensionUnloaded,
                     profile_,
                     extension->id()));
      for (WatchedGalleriesMap::iterator iter =
               watched_extensions_map_[extension->id()].begin();
           iter != watched_extensions_map_[extension->id()].end(); ++iter) {
        iter->second = false;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void GalleryWatchStateTracker::WriteToStorage(const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
  if (!storage)
    return;
  chrome::MediaGalleryPrefIdSet gallery_ids =
      GetAllWatchedGalleryIDsForExtension(extension_id);
  storage->SetExtensionValue(
      extension_id,
      kRegisteredGalleryWatchers,
      WatchedGalleryIdsToValue(gallery_ids).PassAs<base::Value>());
}

void GalleryWatchStateTracker::ReadFromStorage(const std::string& extension_id,
                                               scoped_ptr<base::Value> value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::ListValue* list = NULL;
  if (!value.get() || !value->GetAsList(&list))
    return;
  chrome::MediaGalleryPrefIdSet gallery_ids = WatchedGalleryIdsFromValue(list);
  for (chrome::MediaGalleryPrefIdSet::const_iterator id_iter =
           gallery_ids.begin();
       id_iter != gallery_ids.end(); ++id_iter) {
    watched_extensions_map_[extension_id][*id_iter] = false;
    SetupGalleryWatch(extension_id, *id_iter);
  }
}

void GalleryWatchStateTracker::SetupGalleryWatch(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const Extension* extension = GetExtensionById(profile_, extension_id);
  DCHECK(extension);
  base::FilePath gallery_file_path(
      GetMediaGalleryPreferences(profile_)->LookUpGalleryPathForExtension(
          gallery_id, extension, false));
  if (gallery_file_path.empty())
    return;
  MediaGalleriesPrivateEventRouter* router =
      MediaGalleriesPrivateAPI::Get(profile_)->GetEventRouter();
  DCHECK(router);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&GalleryWatchManager::SetupGalleryWatch,
                 profile_,
                 gallery_id,
                 gallery_file_path,
                 extension_id,
                 router->AsWeakPtr()),
      base::Bind(&GalleryWatchStateTracker::HandleSetupGalleryWatchResponse,
                 AsWeakPtr(),
                 extension_id,
                 gallery_id));
}

void GalleryWatchStateTracker::RemoveGalleryWatch(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const Extension* extension = GetExtensionById(profile_, extension_id);
  DCHECK(extension);
  base::FilePath gallery_file_path(
      GetMediaGalleryPreferences(profile_)->LookUpGalleryPathForExtension(
          gallery_id, extension, true));
  if (gallery_file_path.empty())
    return;
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&GalleryWatchManager::RemoveGalleryWatch,
                 profile_,
                 gallery_file_path,
                 extension_id));
  watched_extensions_map_[extension_id][gallery_id] = false;
}

bool GalleryWatchStateTracker::HasGalleryWatchInfo(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery_id,
    bool has_active_watcher) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return (ContainsKey(watched_extensions_map_, extension_id) &&
          ContainsKey(watched_extensions_map_[extension_id], gallery_id) &&
          watched_extensions_map_[extension_id][gallery_id] ==
              has_active_watcher);
}

void GalleryWatchStateTracker::HandleSetupGalleryWatchResponse(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery_id,
    bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!success)
    return;  // Failed to setup the gallery watch for the given extension.
  AddWatchedGalleryIdInfoForExtension(extension_id, gallery_id);
}

bool GalleryWatchStateTracker::AddWatchedGalleryIdInfoForExtension(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (HasGalleryWatchInfo(extension_id, gallery_id, true))
    return false;
  watched_extensions_map_[extension_id][gallery_id] = true;
  return true;
}

}  // namespace extensions
