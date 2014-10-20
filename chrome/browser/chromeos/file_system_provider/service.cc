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
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "storage/browser/fileapi/external_mount_points.h"

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
const char kPrefKeyWritable[] = "writable";
const char kPrefKeySupportsNotifyTag[] = "supports-notify-tag";
const char kPrefKeyObservedEntries[] = "observed-entries";
const char kPrefKeyObservedEntryEntryPath[] = "entry-path";
const char kPrefKeyObservedEntryRecursive[] = "recursive";
const char kPrefKeyObservedEntryLastTag[] = "last-tag";

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

  // Provided file systems should be already unmounted because of receiving
  // OnExtensionUnload calls for each installed extension. However, for tests
  // we may still have mounted extensions.
  // TODO(mtomasz): Create a TestingService class and remove this code.
  ProvidedFileSystemMap::iterator it = file_system_map_.begin();
  while (it != file_system_map_.end()) {
    const std::string file_system_id =
        it->second->GetFileSystemInfo().file_system_id();
    const std::string extension_id =
        it->second->GetFileSystemInfo().extension_id();
    ++it;
    const bool unmount_result = UnmountFileSystem(
        extension_id, file_system_id, UNMOUNT_REASON_SHUTDOWN);
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
                              const MountOptions& options) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If already exists a file system provided by the same extension with this
  // id, then abort.
  if (GetProvidedFileSystem(extension_id, options.file_system_id)) {
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

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  // The mount point path and name are unique per system, since they are system
  // wide. This is necessary for copying between profiles.
  const base::FilePath& mount_path =
      util::GetMountPath(profile_, extension_id, options.file_system_id);
  const std::string mount_point_name = mount_path.BaseName().AsUTF8Unsafe();

  if (!mount_points->RegisterFileSystem(mount_point_name,
                                        storage::kFileSystemTypeProvided,
                                        storage::FileSystemMountOption(),
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
  //   file_system_id = hello_world
  //   mount_point_name =  b33f1337-hello_world-5aa5
  //   writable = false
  //   supports_notify_tag = false
  //   mount_path = /provided/b33f1337-hello_world-5aa5
  ProvidedFileSystemInfo file_system_info(extension_id, options, mount_path);

  ProvidedFileSystemInterface* file_system =
      file_system_factory_.Run(profile_, file_system_info);
  DCHECK(file_system);
  file_system_map_[FileSystemKey(extension_id, options.file_system_id)] =
      file_system;
  mount_point_name_to_key_map_[mount_point_name] =
      FileSystemKey(extension_id, options.file_system_id);
  RememberFileSystem(file_system_info, *file_system->GetObservedEntries());

  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnProvidedFileSystemMount(file_system_info, base::File::FILE_OK));

  return true;
}

bool Service::UnmountFileSystem(const std::string& extension_id,
                                const std::string& file_system_id,
                                UnmountReason reason) {
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

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
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

  if (reason == UNMOUNT_REASON_USER) {
    ForgetFileSystem(file_system_info.extension_id(),
                     file_system_info.file_system_id());
  }

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
          file_system_info.extension_id(),
          file_system_info.file_system_id(),
          reason == extensions::UnloadedExtensionInfo::REASON_PROFILE_SHUTDOWN
              ? UNMOUNT_REASON_SHUTDOWN
              : UNMOUNT_REASON_USER);
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

void Service::OnObservedEntryChanged(
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& observed_path,
    ChangeType change_type,
    const ChildChanges& child_changes,
    const base::Closure& callback) {
  callback.Run();
}

void Service::OnObservedEntryTagUpdated(
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& observed_path,
    const std::string& tag) {
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  // TODO(mtomasz): Consider optimizing it by moving information about observed
  // entries, or even file systems to leveldb.
  DictionaryPrefUpdate dict_update(pref_service,
                                   prefs::kFileSystemProviderMounted);

  // All of the following checks should not happen in healthy environment.
  // However, since they rely on storage, DCHECKs can't be used.
  base::DictionaryValue* file_systems_per_extension = NULL;
  base::DictionaryValue* file_system = NULL;
  base::DictionaryValue* observed_entries = NULL;
  base::DictionaryValue* observed_entry = NULL;
  if (!dict_update->GetDictionaryWithoutPathExpansion(
          file_system_info.extension_id(), &file_systems_per_extension) ||
      !file_systems_per_extension->GetDictionaryWithoutPathExpansion(
          file_system_info.file_system_id(), &file_system) ||
      !file_system->GetDictionaryWithoutPathExpansion(kPrefKeyObservedEntries,
                                                      &observed_entries) ||
      !observed_entries->GetDictionaryWithoutPathExpansion(
          observed_path.value(), &observed_entry)) {
    // Broken preferences.
    LOG(ERROR) << "Broken preferences detected while updating a tag.";
    return;
  }

  observed_entry->SetStringWithoutPathExpansion(kPrefKeyObservedEntryLastTag,
                                                tag);
}

void Service::OnObservedEntryListChanged(
    const ProvidedFileSystemInfo& file_system_info,
    const ObservedEntries& observed_entries) {
  RememberFileSystem(file_system_info, observed_entries);
}

void Service::RememberFileSystem(const ProvidedFileSystemInfo& file_system_info,
                                 const ObservedEntries& observed_entries) {
  base::DictionaryValue* const file_system = new base::DictionaryValue();
  file_system->SetStringWithoutPathExpansion(kPrefKeyFileSystemId,
                                             file_system_info.file_system_id());
  file_system->SetStringWithoutPathExpansion(kPrefKeyDisplayName,
                                             file_system_info.display_name());
  file_system->SetBooleanWithoutPathExpansion(kPrefKeyWritable,
                                              file_system_info.writable());
  file_system->SetBooleanWithoutPathExpansion(
      kPrefKeySupportsNotifyTag, file_system_info.supports_notify_tag());

  base::DictionaryValue* const observed_entries_value =
      new base::DictionaryValue();
  file_system->SetWithoutPathExpansion(kPrefKeyObservedEntries,
                                       observed_entries_value);

  for (ObservedEntries::const_iterator it = observed_entries.begin();
       it != observed_entries.end();
       ++it) {
    base::DictionaryValue* const observed_entry = new base::DictionaryValue();
    observed_entries_value->SetWithoutPathExpansion(it->first.value(),
                                                    observed_entry);
    observed_entry->SetStringWithoutPathExpansion(
        kPrefKeyObservedEntryEntryPath, it->second.entry_path.value());
    observed_entry->SetBooleanWithoutPathExpansion(
        kPrefKeyObservedEntryRecursive, it->second.recursive);
    observed_entry->SetStringWithoutPathExpansion(kPrefKeyObservedEntryLastTag,
                                                  it->second.last_tag);
  }

  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  DictionaryPrefUpdate dict_update(pref_service,
                                   prefs::kFileSystemProviderMounted);

  base::DictionaryValue* file_systems_per_extension = NULL;
  if (!dict_update->GetDictionaryWithoutPathExpansion(
          file_system_info.extension_id(), &file_systems_per_extension)) {
    file_systems_per_extension = new base::DictionaryValue();
    dict_update->SetWithoutPathExpansion(file_system_info.extension_id(),
                                         file_systems_per_extension);
  }

  file_systems_per_extension->SetWithoutPathExpansion(
      file_system_info.file_system_id(), file_system);
}

void Service::ForgetFileSystem(const std::string& extension_id,
                               const std::string& file_system_id) {
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  DictionaryPrefUpdate dict_update(pref_service,
                                   prefs::kFileSystemProviderMounted);

  base::DictionaryValue* file_systems_per_extension = NULL;
  if (!dict_update->GetDictionaryWithoutPathExpansion(
          extension_id, &file_systems_per_extension))
    return;  // Nothing to forget.

  file_systems_per_extension->RemoveWithoutPathExpansion(file_system_id, NULL);
  if (!file_systems_per_extension->size())
    dict_update->Remove(extension_id, NULL);
}

void Service::RestoreFileSystems(const std::string& extension_id) {
  // TODO(mtomasz): Restore observed entries together with their tags.
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  const base::DictionaryValue* const file_systems =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  DCHECK(file_systems);

  const base::DictionaryValue* file_systems_per_extension = NULL;
  if (!file_systems->GetDictionaryWithoutPathExpansion(
          extension_id, &file_systems_per_extension)) {
    return;  // Nothing to restore.
  }

  // Use a copy of the dictionary, since the original one may be modified while
  // iterating over it.
  scoped_ptr<const base::DictionaryValue> file_systems_per_extension_copy(
      file_systems_per_extension->DeepCopy());

  for (base::DictionaryValue::Iterator it(*file_systems_per_extension_copy);
       !it.IsAtEnd();
       it.Advance()) {
    const base::Value* file_system_value = NULL;
    const base::DictionaryValue* file_system = NULL;
    file_systems_per_extension_copy->GetWithoutPathExpansion(
        it.key(), &file_system_value);
    DCHECK(file_system_value);

    std::string file_system_id;
    std::string display_name;
    bool writable = false;
    bool supports_notify_tag = false;

    if (!file_system_value->GetAsDictionary(&file_system) ||
        !file_system->GetStringWithoutPathExpansion(kPrefKeyFileSystemId,
                                                    &file_system_id) ||
        !file_system->GetStringWithoutPathExpansion(kPrefKeyDisplayName,
                                                    &display_name) ||
        !file_system->GetBooleanWithoutPathExpansion(kPrefKeyWritable,
                                                     &writable) ||
        !file_system->GetBooleanWithoutPathExpansion(kPrefKeySupportsNotifyTag,
                                                     &supports_notify_tag) ||
        file_system_id.empty() || display_name.empty()) {
      LOG(ERROR)
          << "Malformed provided file system information in preferences.";
      continue;
    }

    MountOptions options;
    options.file_system_id = file_system_id;
    options.display_name = display_name;
    options.writable = writable;
    options.supports_notify_tag = supports_notify_tag;

    const bool result = MountFileSystem(extension_id, options);
    if (!result) {
      LOG(ERROR) << "Failed to restore a provided file system from "
                 << "preferences: " << extension_id << ", " << file_system_id
                 << ", " << display_name << ".";
      // Since remounting of the file system failed, then remove it from
      // preferences to avoid remounting it over and over again with a failure.
      ForgetFileSystem(extension_id, file_system_id);
    }

    // Restore observed entries. It's optional, since this field is new.
    const base::DictionaryValue* observed_entries = NULL;
    if (file_system->GetDictionaryWithoutPathExpansion(kPrefKeyObservedEntries,
                                                       &observed_entries)) {
      ProvidedFileSystemInterface* const restored_file_system =
          GetProvidedFileSystem(extension_id, file_system_id);
      DCHECK(restored_file_system);

      for (base::DictionaryValue::Iterator it(*observed_entries); !it.IsAtEnd();
           it.Advance()) {
        const base::Value* observed_entry_value = NULL;
        const base::DictionaryValue* observed_entry = NULL;
        observed_entries->GetWithoutPathExpansion(it.key(),
                                                  &observed_entry_value);
        DCHECK(observed_entry_value);

        std::string entry_path;
        bool recursive = false;
        std::string last_tag;

        if (!observed_entry_value->GetAsDictionary(&observed_entry) ||
            !observed_entry->GetStringWithoutPathExpansion(
                kPrefKeyObservedEntryEntryPath, &entry_path) ||
            !observed_entry->GetBooleanWithoutPathExpansion(
                kPrefKeyObservedEntryRecursive, &recursive) ||
            !observed_entry->GetStringWithoutPathExpansion(
                kPrefKeyObservedEntryLastTag, &last_tag) ||
            it.key() != entry_path || entry_path.empty() ||
            (!options.supports_notify_tag && !last_tag.empty())) {
          LOG(ERROR) << "Malformed observed entry information in preferences.";
          continue;
        }

        ObservedEntry restored_observed_entry;
        restored_observed_entry.entry_path =
            base::FilePath::FromUTF8Unsafe(entry_path);
        restored_observed_entry.recursive = recursive;
        restored_observed_entry.last_tag = last_tag;
        (*restored_file_system
              ->GetObservedEntries())[restored_observed_entry.entry_path] =
            restored_observed_entry;
      }
    }
  }
}

}  // namespace file_system_provider
}  // namespace chromeos
