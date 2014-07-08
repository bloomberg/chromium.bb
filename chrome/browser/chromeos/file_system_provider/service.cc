// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/service.h"

#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "webkit/browser/fileapi/external_mount_points.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Maximum number of file systems to be mounted in the same time, per profile.
const size_t kMaxFileSystems = 16;

// Default factory for provided file systems. |profile| must not be NULL.
ProvidedFileSystemInterface* CreateProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info) {
  DCHECK(profile);
  return new ProvidedFileSystem(profile, file_system_info);
}

}  // namespace

const char kPrefKeyFileSystemId[] = "file-system-id";
const char kPrefKeyDisplayName[] = "display-name";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kFileSystemProviderMounted,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

Service::Service(Profile* profile,
                 extensions::ExtensionRegistry* extension_registry)
    : profile_(profile),
      extension_registry_(extension_registry),
      file_system_factory_(base::Bind(CreateProvidedFileSystem)),
      weak_ptr_factory_(this) {
  extension_registry_->AddObserver(this);
}

Service::~Service() {
  extension_registry_->RemoveObserver(this);
  RememberFileSystems();

  ProvidedFileSystemMap::iterator it = file_system_map_.begin();
  while (it != file_system_map_.end()) {
    const std::string file_system_id =
        it->second->GetFileSystemInfo().file_system_id();
    const std::string extension_id =
        it->second->GetFileSystemInfo().extension_id();
    ++it;
    const bool unmount_result = UnmountFileSystem(extension_id, file_system_id);
    DCHECK(unmount_result);
  }

  DCHECK_EQ(0u, file_system_map_.size());
  STLDeleteValues(&file_system_map_);
}

// static
Service* Service::Get(content::BrowserContext* context) {
  return ServiceFactory::Get(context);
}

void Service::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void Service::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void Service::SetFileSystemFactoryForTesting(
    const FileSystemFactoryCallback& factory_callback) {
  DCHECK(!factory_callback.is_null());
  file_system_factory_ = factory_callback;
}

bool Service::MountFileSystem(const std::string& extension_id,
                              const std::string& file_system_id,
                              const std::string& display_name) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If already exists a file system provided by the same extension with this
  // id, then abort.
  if (GetProvidedFileSystem(extension_id, file_system_id)) {
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      OnProvidedFileSystemMount(ProvidedFileSystemInfo(),
                                                base::File::FILE_ERROR_EXISTS));
    return false;
  }

  // Restrict number of file systems to prevent system abusing.
  if (file_system_map_.size() + 1 > kMaxFileSystems) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemMount(ProvidedFileSystemInfo(),
                                  base::File::FILE_ERROR_TOO_MANY_OPENED));
    return false;
  }

  fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  // The mount point path and name are unique per system, since they are system
  // wide. This is necessary for copying between profiles.
  const base::FilePath& mount_path =
      util::GetMountPath(profile_, extension_id, file_system_id);
  const std::string mount_point_name = mount_path.BaseName().AsUTF8Unsafe();

  if (!mount_points->RegisterFileSystem(mount_point_name,
                                        fileapi::kFileSystemTypeProvided,
                                        fileapi::FileSystemMountOption(),
                                        mount_path)) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemMount(ProvidedFileSystemInfo(),
                                  base::File::FILE_ERROR_INVALID_OPERATION));
    return false;
  }

  // Store the file system descriptor. Use the mount point name as the file
  // system provider file system id.
  // Examples:
  //   file_system_id = 41
  //   mount_point_name =  b33f1337-41-5aa5
  //   mount_path = /provided/b33f1337-41-5aa5
  ProvidedFileSystemInfo file_system_info(
      extension_id, file_system_id, display_name, mount_path);

  ProvidedFileSystemInterface* file_system =
      file_system_factory_.Run(profile_, file_system_info);
  DCHECK(file_system);
  file_system_map_[FileSystemKey(extension_id, file_system_id)] = file_system;
  mount_point_name_to_key_map_[mount_point_name] =
      FileSystemKey(extension_id, file_system_id);

  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnProvidedFileSystemMount(file_system_info, base::File::FILE_OK));

  return true;
}

bool Service::UnmountFileSystem(const std::string& extension_id,
                                const std::string& file_system_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const ProvidedFileSystemMap::iterator file_system_it =
      file_system_map_.find(FileSystemKey(extension_id, file_system_id));
  if (file_system_it == file_system_map_.end()) {
    const ProvidedFileSystemInfo empty_file_system_info;
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemUnmount(empty_file_system_info,
                                    base::File::FILE_ERROR_NOT_FOUND));
    return false;
  }

  fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  const ProvidedFileSystemInfo& file_system_info =
      file_system_it->second->GetFileSystemInfo();

  const std::string mount_point_name =
      file_system_info.mount_path().BaseName().value();
  if (!mount_points->RevokeFileSystem(mount_point_name)) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemUnmount(file_system_info,
                                    base::File::FILE_ERROR_INVALID_OPERATION));
    return false;
  }

  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnProvidedFileSystemUnmount(file_system_info, base::File::FILE_OK));

  mount_point_name_to_key_map_.erase(mount_point_name);

  delete file_system_it->second;
  file_system_map_.erase(file_system_it);

  return true;
}

bool Service::RequestUnmount(const std::string& extension_id,
                             const std::string& file_system_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ProvidedFileSystemMap::iterator file_system_it =
      file_system_map_.find(FileSystemKey(extension_id, file_system_id));
  if (file_system_it == file_system_map_.end())
    return false;

  file_system_it->second->RequestUnmount(
      base::Bind(&Service::OnRequestUnmountStatus,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_system_it->second->GetFileSystemInfo()));
  return true;
}

std::vector<ProvidedFileSystemInfo> Service::GetProvidedFileSystemInfoList() {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<ProvidedFileSystemInfo> result;
  for (ProvidedFileSystemMap::const_iterator it = file_system_map_.begin();
       it != file_system_map_.end();
       ++it) {
    result.push_back(it->second->GetFileSystemInfo());
  }
  return result;
}

ProvidedFileSystemInterface* Service::GetProvidedFileSystem(
    const std::string& extension_id,
    const std::string& file_system_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const ProvidedFileSystemMap::const_iterator file_system_it =
      file_system_map_.find(FileSystemKey(extension_id, file_system_id));
  if (file_system_it == file_system_map_.end())
    return NULL;

  return file_system_it->second;
}

void Service::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  // If the reason is not a profile shutdown, then forget the mounted file
  // systems from preferences.
  if (reason != extensions::UnloadedExtensionInfo::REASON_PROFILE_SHUTDOWN)
    ForgetFileSystems(extension->id());

  // Unmount all of the provided file systems associated with this extension.
  ProvidedFileSystemMap::iterator it = file_system_map_.begin();
  while (it != file_system_map_.end()) {
    const ProvidedFileSystemInfo& file_system_info =
        it->second->GetFileSystemInfo();
    // Advance the iterator beforehand, otherwise it will become invalidated
    // by the UnmountFileSystem() call.
    ++it;
    if (file_system_info.extension_id() == extension->id()) {
      const bool unmount_result = UnmountFileSystem(
          file_system_info.extension_id(), file_system_info.file_system_id());
      DCHECK(unmount_result);
    }
  }
}

void Service::OnExtensionLoaded(content::BrowserContext* browser_context,
                                const extensions::Extension* extension) {
  RestoreFileSystems(extension->id());
}

ProvidedFileSystemInterface* Service::GetProvidedFileSystem(
    const std::string& mount_point_name) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const MountPointNameToKeyMap::const_iterator mapping_it =
      mount_point_name_to_key_map_.find(mount_point_name);
  if (mapping_it == mount_point_name_to_key_map_.end())
    return NULL;

  const ProvidedFileSystemMap::const_iterator file_system_it =
      file_system_map_.find(mapping_it->second);
  if (file_system_it == file_system_map_.end())
    return NULL;

  return file_system_it->second;
}

void Service::OnRequestUnmountStatus(
    const ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {
  // Notify observers about failure in unmounting, since mount() will not be
  // called by the provided file system. In case of success mount() will be
  // invoked, and observers notified, so there is no need to call them now.
  if (error != base::File::FILE_OK) {
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      OnProvidedFileSystemUnmount(file_system_info, error));
  }
}

void Service::RememberFileSystems() {
  base::DictionaryValue extensions;
  const std::vector<ProvidedFileSystemInfo> file_system_info_list =
      GetProvidedFileSystemInfoList();

  for (std::vector<ProvidedFileSystemInfo>::const_iterator it =
           file_system_info_list.begin();
       it != file_system_info_list.end();
       ++it) {
    base::ListValue* file_systems = NULL;
    if (!extensions.GetList(it->extension_id(), &file_systems)) {
      file_systems = new base::ListValue();
      extensions.Set(it->extension_id(), file_systems);
    }

    base::DictionaryValue* file_system = new base::DictionaryValue();
    file_system->SetString(kPrefKeyFileSystemId, it->file_system_id());
    file_system->SetString(kPrefKeyDisplayName, it->display_name());
    file_systems->Append(file_system);
  }

  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  pref_service->Set(prefs::kFileSystemProviderMounted, extensions);
  pref_service->CommitPendingWrite();
}

void Service::ForgetFileSystems(const std::string& extension_id) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  DictionaryPrefUpdate update(pref_service, prefs::kFileSystemProviderMounted);
  base::DictionaryValue* extensions = update.Get();
  DCHECK(extensions);

  extensions->Remove(extension_id, NULL);
}

void Service::RestoreFileSystems(const std::string& extension_id) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  const base::DictionaryValue* extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  DCHECK(extensions);

  const base::ListValue* file_systems = NULL;

  if (!extensions->GetList(extension_id, &file_systems))
    return;

  for (size_t i = 0; i < file_systems->GetSize(); ++i) {
    const base::DictionaryValue* file_system = NULL;
    file_systems->GetDictionary(i, &file_system);
    DCHECK(file_system);

    std::string file_system_id;
    file_system->GetString(kPrefKeyFileSystemId, &file_system_id);
    DCHECK(!file_system_id.empty());

    std::string display_name;
    file_system->GetString(kPrefKeyDisplayName, &display_name);
    DCHECK(!display_name.empty());

    if (file_system_id.empty() || display_name.empty()) {
      LOG(ERROR)
          << "Malformed provided file system information in preferences.";
      continue;
    }

    const bool result =
        MountFileSystem(extension_id, file_system_id, display_name);
    if (!result) {
      LOG(ERROR) << "Failed to restore a provided file system from "
                 << "preferences: " << extension_id << ", " << file_system_id
                 << ", " << display_name << ".";
    }
  }
}

}  // namespace file_system_provider
}  // namespace chromeos
