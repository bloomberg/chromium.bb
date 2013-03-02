// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_preferences.h"

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
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
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
const char kMediaGalleriesVolumeLabelKey[] = "volumeLabel";
const char kMediaGalleriesVendorNameKey[] = "vendorName";
const char kMediaGalleriesModelNameKey[] = "modelName";
const char kMediaGalleriesSizeKey[] = "totalSize";
const char kMediaGalleriesLastAttachTimeKey[] = "lastAttachTime";
const char kMediaGalleriesPrefsVersionKey[] = "preferencesVersion";

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
  string16 volume_label;
  string16 vendor_name;
  string16 model_name;
  double total_size_in_bytes = 0.0;
  double last_attach_time = 0.0;
  bool volume_metadata_valid = false;
  int prefs_version = 0;

  if (!GetPrefId(dict, &pref_id) ||
      !dict.GetString(kMediaGalleriesDeviceIdKey, &device_id) ||
      !dict.GetString(kMediaGalleriesPathKey, &path) ||
      !GetType(dict, &type)) {
    return false;
  }

  dict.GetString(kMediaGalleriesDisplayNameKey, &display_name);
  dict.GetInteger(kMediaGalleriesPrefsVersionKey, &prefs_version);

  if (dict.GetString(kMediaGalleriesVolumeLabelKey, &volume_label) &&
      dict.GetString(kMediaGalleriesVendorNameKey, &vendor_name) &&
      dict.GetString(kMediaGalleriesModelNameKey, &model_name) &&
      dict.GetDouble(kMediaGalleriesSizeKey, &total_size_in_bytes) &&
      dict.GetDouble(kMediaGalleriesLastAttachTimeKey, &last_attach_time)) {
    volume_metadata_valid = true;
  }

  out_gallery_info->pref_id = pref_id;
  out_gallery_info->display_name = display_name;
  out_gallery_info->device_id = device_id;
  out_gallery_info->path = base::FilePath(path);
  out_gallery_info->type = type;
  out_gallery_info->volume_label = volume_label;
  out_gallery_info->vendor_name = vendor_name;
  out_gallery_info->model_name = model_name;
  out_gallery_info->total_size_in_bytes = total_size_in_bytes;
  out_gallery_info->last_attach_time =
      base::Time::FromInternalValue(last_attach_time);
  out_gallery_info->volume_metadata_valid = volume_metadata_valid;
  out_gallery_info->prefs_version = prefs_version;

  return true;
}

DictionaryValue* CreateGalleryPrefInfoDictionary(
    const MediaGalleryPrefInfo& gallery) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kMediaGalleriesPrefIdKey,
                  base::Uint64ToString(gallery.pref_id));
  if (!gallery.volume_metadata_valid)
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

  if (gallery.volume_metadata_valid) {
    dict->SetString(kMediaGalleriesVolumeLabelKey, gallery.volume_label);
    dict->SetString(kMediaGalleriesVendorNameKey, gallery.vendor_name);
    dict->SetString(kMediaGalleriesModelNameKey, gallery.model_name);
    dict->SetDouble(kMediaGalleriesSizeKey, gallery.total_size_in_bytes);
    dict->SetDouble(kMediaGalleriesLastAttachTimeKey,
                    gallery.last_attach_time.ToInternalValue());
  }

  // Version 0 of the prefs format was that the display_name was always
  // used to show the user-visible name of the gallery. Version 1 means
  // that there is an optional display_name, and when it is present, it
  // overrides the name that would be built from the volume metadata, path,
  // or whatever other data. So if we see a display_name with version 0, it
  // means it may be overwritten simply by getting new volume metadata.
  // A display_name with version 1 should not be overwritten.
  dict->SetInteger(kMediaGalleriesPrefsVersionKey, gallery.prefs_version);

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
      type(kInvalidType),
      total_size_in_bytes(0),
      volume_metadata_valid(false),
      prefs_version(0) {
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

    base::FilePath relative_path;
    StorageMonitor::StorageInfo info;
    if (MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path)) {
      // TODO(gbillock): Add in the volume metadata here when available.
      AddGalleryWithName(info.device_id, info.name, relative_path,
                         false /*user added*/);
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
    NotifyChangeObservers("");
}

void MediaGalleriesPreferences::NotifyChangeObservers(
    const std::string& extension_id) {
  FOR_EACH_OBSERVER(GalleryChangeObserver,
                    gallery_change_observers_,
                    OnGalleryChanged(this, extension_id));
}

void MediaGalleriesPreferences::AddGalleryChangeObserver(
    GalleryChangeObserver* observer) {
  gallery_change_observers_.AddObserver(observer);
}

void MediaGalleriesPreferences::RemoveGalleryChangeObserver(
    GalleryChangeObserver* observer) {
  gallery_change_observers_.RemoveObserver(observer);
}

void MediaGalleriesPreferences::OnRemovableStorageAttached(
    const StorageMonitor::StorageInfo& info) {
  if (!MediaStorageUtil::IsMediaDevice(info.device_id))
    return;

  if (info.name.empty()) {
    AddGallery(info.device_id, base::FilePath(info.location),
               false /*not user added*/,
               info.storage_label,
               info.vendor_name,
               info.model_name,
               info.total_size_in_bytes,
               base::Time::Now());
  } else {
    AddGalleryWithName(info.device_id, info.name,
                       base::FilePath(info.location), false);
  }
}

bool MediaGalleriesPreferences::LookUpGalleryByPath(
    const base::FilePath& path,
    MediaGalleryPrefInfo* gallery_info) const {
  StorageMonitor::StorageInfo info;
  base::FilePath relative_path;
  if (!MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path)) {
    if (gallery_info)
      *gallery_info = MediaGalleryPrefInfo();
    return false;
  }

  relative_path = relative_path.NormalizePathSeparators();
  MediaGalleryPrefIdSet galleries_on_device =
      LookUpGalleriesByDeviceId(info.device_id);
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

  // This method is called by controller::FilesSelected when the user
  // adds a new gallery. Control reaches here when the selected gallery is
  // on a volume we know about, but have no gallery already for. Returns
  // hypothetical data to the caller about what the prefs will look like
  // if the gallery is added.
  // TODO(gbillock): split this out into another function so it doesn't
  // conflate LookUp.
  if (gallery_info) {
    gallery_info->pref_id = kInvalidMediaGalleryPrefId;
    gallery_info->display_name = info.name;
    gallery_info->device_id = info.device_id;
    gallery_info->path = relative_path;
    gallery_info->type = MediaGalleryPrefInfo::kUserAdded;
    // TODO(gbillock): Need to add volume metadata here from |info|.
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
    const std::string& device_id,
    const base::FilePath& relative_path, bool user_added,
    const string16& volume_label, const string16& vendor_name,
    const string16& model_name, uint64 total_size_in_bytes,
    base::Time last_attach_time) {
  return AddGalleryInternal(device_id, string16(), relative_path, user_added,
                            volume_label, vendor_name, model_name,
                            total_size_in_bytes, last_attach_time, true, 1);
}

MediaGalleryPrefId MediaGalleriesPreferences::AddGalleryWithName(
    const std::string& device_id, const string16& display_name,
    const base::FilePath& relative_path, bool user_added) {
  return AddGalleryInternal(device_id, display_name, relative_path, user_added,
                            string16(), string16(), string16(),
                            0, base::Time(), false, 1);
}

MediaGalleryPrefId MediaGalleriesPreferences::AddGalleryInternal(
    const std::string& device_id, const string16& display_name,
    const base::FilePath& relative_path, bool user_added,
    const string16& volume_label, const string16& vendor_name,
    const string16& model_name, uint64 total_size_in_bytes,
    base::Time last_attach_time,
    bool volume_metadata_valid,
    int prefs_version) {
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
    bool update_gallery_metadata = volume_metadata_valid &&
        ((existing.volume_label != volume_label) ||
         (existing.vendor_name != vendor_name) ||
         (existing.model_name != model_name) ||
         (existing.total_size_in_bytes != total_size_in_bytes) ||
         (existing.last_attach_time != last_attach_time));

    if (!update_gallery_name && !update_gallery_type &&
        !update_gallery_metadata)
      return *it;

    PrefService* prefs = profile_->GetPrefs();
    scoped_ptr<ListPrefUpdate> update(
        new ListPrefUpdate(prefs, prefs::kMediaGalleriesRememberedGalleries));
    ListValue* list = update->Get();

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
        if (update_gallery_metadata) {
          dict->SetString(kMediaGalleriesVolumeLabelKey, volume_label);
          dict->SetString(kMediaGalleriesVendorNameKey, vendor_name);
          dict->SetString(kMediaGalleriesModelNameKey, model_name);
          dict->SetDouble(kMediaGalleriesSizeKey, total_size_in_bytes);
          dict->SetDouble(kMediaGalleriesLastAttachTimeKey,
                          last_attach_time.ToInternalValue());
        }
        dict->SetInteger(kMediaGalleriesPrefsVersionKey, prefs_version);
        break;
      }
    }

    // Commits the prefs update.
    update.reset();

    if (update_gallery_name || update_gallery_metadata || update_gallery_type)
      InitFromPrefs(true /* notify observers */);
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
  if (volume_metadata_valid) {
    gallery_info.volume_label = volume_label;
    gallery_info.vendor_name = vendor_name;
    gallery_info.model_name = model_name;
    gallery_info.total_size_in_bytes = total_size_in_bytes;
    gallery_info.last_attach_time = last_attach_time;
  }
  gallery_info.volume_metadata_valid = volume_metadata_valid;
  gallery_info.prefs_version = prefs_version;

  {
    ListPrefUpdate update(prefs, prefs::kMediaGalleriesRememberedGalleries);
    ListValue* list = update.Get();
    list->Append(CreateGalleryPrefInfoDictionary(gallery_info));
  }
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
  return AddGalleryInternal(gallery_info.device_id,
                            gallery_info.display_name,
                            gallery_info.path,
                            true /*user added*/,
                            gallery_info.volume_label,
                            gallery_info.vendor_name,
                            gallery_info.model_name,
                            gallery_info.total_size_in_bytes,
                            gallery_info.last_attach_time,
                            gallery_info.volume_metadata_valid,
                            gallery_info.prefs_version);
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
      NotifyChangeObservers(extension.id());
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
  NotifyChangeObservers(extension.id());
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
