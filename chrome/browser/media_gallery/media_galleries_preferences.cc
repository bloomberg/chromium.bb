// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/media_galleries_preferences.h"

#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/media_galleries_private/gallery_watch_state_tracker.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/common/pref_names.h"

namespace chrome {

namespace {

const char kMediaGalleriesDeviceIdKey[] = "deviceId";
const char kMediaGalleriesDisplayNameKey[] = "displayName";
const char kMediaGalleriesPathKey[] = "path";
const char kMediaGalleriesPrefIdKey[] = "prefId";
const char kMediaGalleriesTypeKey[] = "type";
const char kMediaGalleriesTypeAutoDetectedValue[] = "autoDetected";
const char kMediaGalleriesTypeUserAddedValue[] = "userAdded";
const char kMediaGalleriesTypeBlackListedValue[] = "blackListed";

bool GetPrefId(const DictionaryValue& dict, MediaGalleryPrefId* value) {
  std::string string_id;
  if (!dict.GetString(kMediaGalleriesPrefIdKey, &string_id) ||
      !base::StringToUint64(string_id, value)) {
    return false;
  }

  return true;
}

bool GetType(const DictionaryValue& dict, MediaGalleryPrefInfo::Type* type) {
  std::string string_type;
  if (!dict.GetString(kMediaGalleriesTypeKey, &string_type))
    return false;

  if (string_type == kMediaGalleriesTypeAutoDetectedValue) {
    *type = MediaGalleryPrefInfo::kAutoDetected;
    return true;
  }
  if (string_type == kMediaGalleriesTypeUserAddedValue) {
    *type = MediaGalleryPrefInfo::kUserAdded;
    return true;
  }
  if (string_type == kMediaGalleriesTypeBlackListedValue) {
    *type = MediaGalleryPrefInfo::kBlackListed;
    return true;
  }

  return false;
}

bool PopulateGalleryPrefInfoFromDictionary(
    const DictionaryValue& dict, MediaGalleryPrefInfo* out_gallery_info) {
  MediaGalleryPrefId pref_id;
  string16 display_name;
  std::string device_id;
  base::FilePath::StringType path;
  MediaGalleryPrefInfo::Type type = MediaGalleryPrefInfo::kAutoDetected;
  if (!GetPrefId(dict, &pref_id) ||
      !dict.GetString(kMediaGalleriesDisplayNameKey, &display_name) ||
      !dict.GetString(kMediaGalleriesDeviceIdKey, &device_id) ||
      !dict.GetString(kMediaGalleriesPathKey, &path) ||
      !GetType(dict, &type)) {
    return false;
  }

  out_gallery_info->pref_id = pref_id;
  out_gallery_info->display_name = display_name;
  out_gallery_info->device_id = device_id;
  out_gallery_info->path = base::FilePath(path);
  out_gallery_info->type = type;
  return true;
}

DictionaryValue* CreateGalleryPrefInfoDictionary(
    const MediaGalleryPrefInfo& gallery) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kMediaGalleriesPrefIdKey,
                  base::Uint64ToString(gallery.pref_id));
  dict->SetString(kMediaGalleriesDisplayNameKey, gallery.display_name);
  dict->SetString(kMediaGalleriesDeviceIdKey, gallery.device_id);
  dict->SetString(kMediaGalleriesPathKey, gallery.path.value());
  const char* type = NULL;
  switch (gallery.type) {
    case MediaGalleryPrefInfo::kAutoDetected:
      type = kMediaGalleriesTypeAutoDetectedValue;
      break;
    case MediaGalleryPrefInfo::kUserAdded:
      type = kMediaGalleriesTypeUserAddedValue;
      break;
    case MediaGalleryPrefInfo::kBlackListed:
      type = kMediaGalleriesTypeBlackListedValue;
      break;
    default:
      NOTREACHED();
      break;
  }
  dict->SetString(kMediaGalleriesTypeKey, type);
  return dict;
}

bool HasAutoDetectedGalleryPermission(const extensions::Extension& extension) {
  extensions::MediaGalleriesPermission::CheckParam param(
      extensions::MediaGalleriesPermission::kAllAutoDetectedPermission);
  return extension.CheckAPIPermissionWithParam(
      extensions::APIPermission::kMediaGalleries, &param);
}

}  // namespace

MediaGalleryPrefInfo::MediaGalleryPrefInfo()
    : pref_id(kInvalidMediaGalleryPrefId),
      type(kInvalidType) {
}
MediaGalleryPrefInfo::~MediaGalleryPrefInfo() {}

base::FilePath MediaGalleryPrefInfo::AbsolutePath() const {
  base::FilePath base_path = MediaStorageUtil::FindDevicePathById(device_id);
  return base_path.empty() ? base_path : base_path.Append(path);
}

MediaGalleriesPreferences::GalleryChangeObserver::~GalleryChangeObserver() {}

MediaGalleriesPreferences::MediaGalleriesPreferences(Profile* profile)
    : profile_(profile) {
  AddDefaultGalleriesIfFreshProfile();
  InitFromPrefs(false /*no notification*/);
}

MediaGalleriesPreferences::~MediaGalleriesPreferences() {}

void MediaGalleriesPreferences::AddDefaultGalleriesIfFreshProfile() {
  // Only add defaults the first time.
  if (APIHasBeenUsed(profile_))
    return;

  // Fresh profile case.
  const int kDirectoryKeys[] = {
    DIR_USER_MUSIC,
    DIR_USER_PICTURES,
    DIR_USER_VIDEOS,
  };

  for (size_t i = 0; i < arraysize(kDirectoryKeys); ++i) {
    base::FilePath path;
    if (!PathService::Get(kDirectoryKeys[i], &path))
      continue;

    std::string device_id;
    string16 display_name;
    base::FilePath relative_path;
    if (MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, &display_name,
                                                &relative_path)) {
      AddGallery(device_id, display_name, relative_path, false /*user added*/);
    }
  }
}

void MediaGalleriesPreferences::InitFromPrefs(bool notify_observers) {
  known_galleries_.clear();
  device_map_.clear();

  PrefService* prefs = profile_->GetPrefs();
  const ListValue* list = prefs->GetList(
      prefs::kMediaGalleriesRememberedGalleries);
  if (list) {
    for (ListValue::const_iterator it = list->begin();
         it != list->end(); ++it) {
      const DictionaryValue* dict = NULL;
      if (!(*it)->GetAsDictionary(&dict))
        continue;

      MediaGalleryPrefInfo gallery_info;
      if (!PopulateGalleryPrefInfoFromDictionary(*dict, &gallery_info))
        continue;

      known_galleries_[gallery_info.pref_id] = gallery_info;
      device_map_[gallery_info.device_id].insert(gallery_info.pref_id);
    }
  }
  if (notify_observers)
    NotifyChangeObservers();
}

void MediaGalleriesPreferences::NotifyChangeObservers() {
  FOR_EACH_OBSERVER(GalleryChangeObserver,
                    gallery_change_observers_,
                    OnGalleryChanged(this));
}

void MediaGalleriesPreferences::AddGalleryChangeObserver(
    GalleryChangeObserver* observer) {
  gallery_change_observers_.AddObserver(observer);
}

void MediaGalleriesPreferences::RemoveGalleryChangeObserver(
    GalleryChangeObserver* observer) {
  gallery_change_observers_.RemoveObserver(observer);
}

bool MediaGalleriesPreferences::LookUpGalleryByPath(
    const base::FilePath& path,
    MediaGalleryPrefInfo* gallery_info) const {
  std::string device_id;
  string16 device_name;
  base::FilePath relative_path;
  if (!MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, &device_name,
                                               &relative_path)) {
    if (gallery_info) {
      gallery_info->pref_id = kInvalidMediaGalleryPrefId;
      gallery_info->display_name = string16();
      gallery_info->device_id = std::string();
      gallery_info->path = base::FilePath();
      gallery_info->type = MediaGalleryPrefInfo::kInvalidType;
    }
    return false;
  }
  relative_path = relative_path.NormalizePathSeparators();
  MediaGalleryPrefIdSet galleries_on_device =
      LookUpGalleriesByDeviceId(device_id);
  for (MediaGalleryPrefIdSet::const_iterator it = galleries_on_device.begin();
       it != galleries_on_device.end();
       ++it) {
    const MediaGalleryPrefInfo& gallery = known_galleries_.find(*it)->second;
    if (gallery.path != relative_path)
      continue;

    if (gallery_info)
      *gallery_info = gallery;
    return true;
  }

  if (gallery_info) {
    gallery_info->pref_id = kInvalidMediaGalleryPrefId;
    gallery_info->display_name = device_name;
    gallery_info->device_id = device_id;
    gallery_info->path = relative_path;
    gallery_info->type = MediaGalleryPrefInfo::kUserAdded;
  }
  return false;
}

MediaGalleryPrefIdSet MediaGalleriesPreferences::LookUpGalleriesByDeviceId(
    const std::string& device_id) const {
  DeviceIdPrefIdsMap::const_iterator found = device_map_.find(device_id);
  if (found == device_map_.end())
    return MediaGalleryPrefIdSet();
  return found->second;
}

base::FilePath MediaGalleriesPreferences::LookUpGalleryPathForExtension(
    MediaGalleryPrefId gallery_id,
    const extensions::Extension* extension,
    bool include_unpermitted_galleries) {
  DCHECK(extension);
  if (!include_unpermitted_galleries &&
      !ContainsKey(GalleriesForExtension(*extension), gallery_id))
    return base::FilePath();

  MediaGalleriesPrefInfoMap::const_iterator it =
      known_galleries_.find(gallery_id);
  if (it == known_galleries_.end())
    return base::FilePath();
  return MediaStorageUtil::FindDevicePathById(it->second.device_id);
}

MediaGalleryPrefId MediaGalleriesPreferences::AddGallery(
    const std::string& device_id, const string16& display_name,
    const base::FilePath& relative_path, bool user_added) {
  DCHECK_GT(display_name.size(), 0U);
  base::FilePath normalized_relative_path =
      relative_path.NormalizePathSeparators();
  MediaGalleryPrefIdSet galleries_on_device =
    LookUpGalleriesByDeviceId(device_id);
  for (MediaGalleryPrefIdSet::const_iterator it = galleries_on_device.begin();
       it != galleries_on_device.end();
       ++it) {
    const MediaGalleryPrefInfo& existing = known_galleries_.find(*it)->second;
    if (existing.path != normalized_relative_path)
      continue;

    bool update_gallery_type =
        user_added && (existing.type == MediaGalleryPrefInfo::kBlackListed);
    bool update_gallery_name = existing.display_name != display_name;
    if (!update_gallery_name && !update_gallery_type)
      return *it;

    PrefService* prefs = profile_->GetPrefs();
    ListPrefUpdate update(prefs, prefs::kMediaGalleriesRememberedGalleries);
    ListValue* list = update.Get();

    for (ListValue::const_iterator list_iter = list->begin();
         list_iter != list->end();
         ++list_iter) {
      DictionaryValue* dict;
      MediaGalleryPrefId iter_id;
      if ((*list_iter)->GetAsDictionary(&dict) &&
          GetPrefId(*dict, &iter_id) &&
          *it == iter_id) {
        if (update_gallery_type) {
          dict->SetString(kMediaGalleriesTypeKey,
                          kMediaGalleriesTypeAutoDetectedValue);
        }
        if (update_gallery_name)
          dict->SetString(kMediaGalleriesDisplayNameKey, display_name);
        InitFromPrefs(true /* notify observers */);
        break;
      }
    }
    return *it;
  }

  PrefService* prefs = profile_->GetPrefs();

  MediaGalleryPrefInfo gallery_info;
  gallery_info.pref_id = prefs->GetUint64(prefs::kMediaGalleriesUniqueId);
  prefs->SetUint64(prefs::kMediaGalleriesUniqueId, gallery_info.pref_id + 1);
  gallery_info.display_name = display_name;
  gallery_info.device_id = device_id;
  gallery_info.path = normalized_relative_path;
  gallery_info.type = MediaGalleryPrefInfo::kAutoDetected;
  if (user_added)
    gallery_info.type = MediaGalleryPrefInfo::kUserAdded;

  ListPrefUpdate update(prefs, prefs::kMediaGalleriesRememberedGalleries);
  ListValue* list = update.Get();
  list->Append(CreateGalleryPrefInfoDictionary(gallery_info));
  InitFromPrefs(true /* notify observers */);

  return gallery_info.pref_id;
}

MediaGalleryPrefId MediaGalleriesPreferences::AddGalleryByPath(
    const base::FilePath& path) {
  MediaGalleryPrefInfo gallery_info;
  if (LookUpGalleryByPath(path, &gallery_info) &&
      gallery_info.type != MediaGalleryPrefInfo::kBlackListed) {
    return gallery_info.pref_id;
  }
  return AddGallery(gallery_info.device_id, gallery_info.display_name,
                    gallery_info.path, true /*user added*/);
}

void MediaGalleriesPreferences::ForgetGalleryById(MediaGalleryPrefId pref_id) {
  PrefService* prefs = profile_->GetPrefs();
  ListPrefUpdate update(prefs, prefs::kMediaGalleriesRememberedGalleries);
  ListValue* list = update.Get();

  if (!ContainsKey(known_galleries_, pref_id))
    return;

  for (ListValue::iterator iter = list->begin(); iter != list->end(); ++iter) {
    DictionaryValue* dict;
    MediaGalleryPrefId iter_id;
    if ((*iter)->GetAsDictionary(&dict) && GetPrefId(*dict, &iter_id) &&
        pref_id == iter_id) {
      GetExtensionPrefs()->RemoveMediaGalleryPermissions(pref_id);
      MediaGalleryPrefInfo::Type type;
      if (GetType(*dict, &type) &&
          type == MediaGalleryPrefInfo::kAutoDetected) {
        dict->SetString(kMediaGalleriesTypeKey,
                        kMediaGalleriesTypeBlackListedValue);
      } else {
        list->Erase(iter, NULL);
      }
      InitFromPrefs(true /* notify observers */);
      return;
    }
  }
}

MediaGalleryPrefIdSet MediaGalleriesPreferences::GalleriesForExtension(
    const extensions::Extension& extension) const {
  MediaGalleryPrefIdSet result;

  if (HasAutoDetectedGalleryPermission(extension)) {
    for (MediaGalleriesPrefInfoMap::const_iterator it =
             known_galleries_.begin(); it != known_galleries_.end(); ++it) {
      if (it->second.type == MediaGalleryPrefInfo::kAutoDetected)
        result.insert(it->second.pref_id);
    }
  }

  std::vector<MediaGalleryPermission> stored_permissions =
      GetExtensionPrefs()->GetMediaGalleryPermissions(extension.id());

  for (std::vector<MediaGalleryPermission>::const_iterator it =
           stored_permissions.begin(); it != stored_permissions.end(); ++it) {
    if (!it->has_permission) {
      result.erase(it->pref_id);
    } else {
      MediaGalleriesPrefInfoMap::const_iterator gallery =
          known_galleries_.find(it->pref_id);
      DCHECK(gallery != known_galleries_.end());
      if (gallery->second.type != MediaGalleryPrefInfo::kBlackListed) {
        result.insert(it->pref_id);
      } else {
        NOTREACHED() << gallery->second.device_id;
      }
    }
  }
  return result;
}

void MediaGalleriesPreferences::SetGalleryPermissionForExtension(
    const extensions::Extension& extension,
    MediaGalleryPrefId pref_id,
    bool has_permission) {
  // The gallery may not exist anymore if the user opened a second config
  // surface concurrently and removed it. Drop the permission update if so.
  MediaGalleriesPrefInfoMap::const_iterator gallery_info =
      known_galleries_.find(pref_id);
  if (gallery_info == known_galleries_.end())
    return;

#if defined(ENABLE_EXTENSIONS)
  extensions::GalleryWatchStateTracker* state_tracker =
      extensions::GalleryWatchStateTracker::GetForProfile(profile_);
#endif
  bool all_permission = HasAutoDetectedGalleryPermission(extension);
  if (has_permission && all_permission) {
    if (gallery_info->second.type == MediaGalleryPrefInfo::kAutoDetected) {
      GetExtensionPrefs()->UnsetMediaGalleryPermission(extension.id(), pref_id);
#if defined(ENABLE_EXTENSIONS)
      if (state_tracker) {
        state_tracker->OnGalleryPermissionChanged(extension.id(), pref_id,
                                                  true);
      }
#endif
      return;
    }
  }

  if (!has_permission && !all_permission) {
    GetExtensionPrefs()->UnsetMediaGalleryPermission(extension.id(), pref_id);
  } else {
    GetExtensionPrefs()->SetMediaGalleryPermission(extension.id(), pref_id,
                                                   has_permission);
  }
#if defined(ENABLE_EXTENSIONS)
  if (state_tracker) {
    state_tracker->OnGalleryPermissionChanged(extension.id(), pref_id,
                                              has_permission);
  }
#endif
}

void MediaGalleriesPreferences::Shutdown() {
  profile_ = NULL;
}

// static
bool MediaGalleriesPreferences::APIHasBeenUsed(Profile* profile) {
  MediaGalleryPrefId current_id =
      profile->GetPrefs()->GetUint64(prefs::kMediaGalleriesUniqueId);
  return current_id != kInvalidMediaGalleryPrefId + 1;
}

// static
void MediaGalleriesPreferences::RegisterUserPrefs(
    PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kMediaGalleriesRememberedGalleries,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterUint64Pref(prefs::kMediaGalleriesUniqueId,
                               kInvalidMediaGalleryPrefId + 1,
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
}

extensions::ExtensionPrefs*
MediaGalleriesPreferences::GetExtensionPrefs() const {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  return extension_service->extension_prefs();
}

}  // namespace chrome
