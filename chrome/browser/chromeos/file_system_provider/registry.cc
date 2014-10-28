// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/registry.h"

#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "storage/browser/fileapi/external_mount_points.h"

namespace chromeos {
namespace file_system_provider {

const char kPrefKeyFileSystemId[] = "file-system-id";
const char kPrefKeyDisplayName[] = "display-name";
const char kPrefKeyWritable[] = "writable";
const char kPrefKeySupportsNotifyTag[] = "supports-notify-tag";
const char kPrefKeyObservedEntries[] = "observed-entries";
const char kPrefKeyObservedEntryEntryPath[] = "entry-path";
const char kPrefKeyObservedEntryRecursive[] = "recursive";
const char kPrefKeyObservedEntryLastTag[] = "last-tag";
const char kPrefKeyObservedEntryPersistentOrigins[] = "persistent-origins";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kFileSystemProviderMounted,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

Registry::Registry(Profile* profile) : profile_(profile) {
}

Registry::~Registry() {
}

void Registry::RememberFileSystem(
    const ProvidedFileSystemInfo& file_system_info,
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

  for (const auto& it : observed_entries) {
    base::DictionaryValue* const observed_entry = new base::DictionaryValue();
    observed_entries_value->SetWithoutPathExpansion(
        it.second.entry_path.value(), observed_entry);
    observed_entry->SetStringWithoutPathExpansion(
        kPrefKeyObservedEntryEntryPath, it.second.entry_path.value());
    observed_entry->SetBooleanWithoutPathExpansion(
        kPrefKeyObservedEntryRecursive, it.second.recursive);
    observed_entry->SetStringWithoutPathExpansion(kPrefKeyObservedEntryLastTag,
                                                  it.second.last_tag);
    base::ListValue* const persistent_origins_value = new base::ListValue();
    observed_entry->SetWithoutPathExpansion(
        kPrefKeyObservedEntryPersistentOrigins, persistent_origins_value);
    for (const auto& subscriber_it : it.second.subscribers) {
      // Only persistent subscribers should be stored in persistent storage.
      // Other ones should not be restired after a restart.
      if (subscriber_it.second.persistent)
        persistent_origins_value->AppendString(subscriber_it.first.spec());
    }
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

void Registry::ForgetFileSystem(const std::string& extension_id,
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

scoped_ptr<Registry::RestoredFileSystems> Registry::RestoreFileSystems(
    const std::string& extension_id) {
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  const base::DictionaryValue* const file_systems =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  DCHECK(file_systems);

  const base::DictionaryValue* file_systems_per_extension = NULL;
  if (!file_systems->GetDictionaryWithoutPathExpansion(
          extension_id, &file_systems_per_extension)) {
    return make_scoped_ptr(new RestoredFileSystems);  // Nothing to restore.
  }

  scoped_ptr<RestoredFileSystems> restored_file_systems(
      new RestoredFileSystems);

  for (base::DictionaryValue::Iterator it(*file_systems_per_extension);
       !it.IsAtEnd();
       it.Advance()) {
    const base::Value* file_system_value = NULL;
    const base::DictionaryValue* file_system = NULL;
    file_systems_per_extension->GetWithoutPathExpansion(it.key(),
                                                        &file_system_value);
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

    RestoredFileSystem restored_file_system;
    restored_file_system.extension_id = extension_id;
    restored_file_system.options = options;

    // Restore observed entries. It's optional, since this field is new.
    const base::DictionaryValue* observed_entries = NULL;
    if (file_system->GetDictionaryWithoutPathExpansion(kPrefKeyObservedEntries,
                                                       &observed_entries)) {
      for (base::DictionaryValue::Iterator it(*observed_entries); !it.IsAtEnd();
           it.Advance()) {
        const base::Value* observed_entry_value = NULL;
        const base::DictionaryValue* observed_entry = NULL;
        observed_entries->GetWithoutPathExpansion(it.key(),
                                                  &observed_entry_value);
        DCHECK(file_system_value);

        std::string entry_path;
        bool recursive = false;
        std::string last_tag;
        const base::ListValue* persistent_origins = NULL;

        if (!observed_entry_value->GetAsDictionary(&observed_entry) ||
            !observed_entry->GetStringWithoutPathExpansion(
                kPrefKeyObservedEntryEntryPath, &entry_path) ||
            !observed_entry->GetBooleanWithoutPathExpansion(
                kPrefKeyObservedEntryRecursive, &recursive) ||
            !observed_entry->GetStringWithoutPathExpansion(
                kPrefKeyObservedEntryLastTag, &last_tag) ||
            !observed_entry->GetListWithoutPathExpansion(
                kPrefKeyObservedEntryPersistentOrigins, &persistent_origins) ||
            it.key() != entry_path || entry_path.empty() ||
            (!options.supports_notify_tag &&
             (!last_tag.empty() || persistent_origins->GetSize()))) {
          LOG(ERROR) << "Malformed observed entry information in preferences.";
          continue;
        }

        ObservedEntry restored_observed_entry;
        restored_observed_entry.entry_path =
            base::FilePath::FromUTF8Unsafe(entry_path);
        restored_observed_entry.recursive = recursive;
        restored_observed_entry.last_tag = last_tag;
        for (const auto& persistent_origin : *persistent_origins) {
          std::string origin;
          if (persistent_origin->GetAsString(&origin)) {
            LOG(ERROR) << "Malformed subscriber information in preferences.";
            continue;
          }
          const GURL origin_as_gurl(origin);
          restored_observed_entry.subscribers[origin_as_gurl].origin =
              origin_as_gurl;
          restored_observed_entry.subscribers[origin_as_gurl].persistent = true;
        }
        restored_file_system.observed_entries[ObservedEntryKey(
            base::FilePath::FromUTF8Unsafe(entry_path), recursive)] =
            restored_observed_entry;
      }
    }
    restored_file_systems->push_back(restored_file_system);
  }

  return restored_file_systems.Pass();
}

void Registry::UpdateObservedEntryTag(
    const ProvidedFileSystemInfo& file_system_info,
    const ObservedEntry& observed_entry) {
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
  base::DictionaryValue* observed_entry_value = NULL;
  if (!dict_update->GetDictionaryWithoutPathExpansion(
          file_system_info.extension_id(), &file_systems_per_extension) ||
      !file_systems_per_extension->GetDictionaryWithoutPathExpansion(
          file_system_info.file_system_id(), &file_system) ||
      !file_system->GetDictionaryWithoutPathExpansion(kPrefKeyObservedEntries,
                                                      &observed_entries) ||
      !observed_entries->GetDictionaryWithoutPathExpansion(
          observed_entry.entry_path.value(), &observed_entry_value)) {
    // Broken preferences.
    LOG(ERROR) << "Broken preferences detected while updating a tag.";
    return;
  }

  observed_entry_value->SetStringWithoutPathExpansion(
      kPrefKeyObservedEntryLastTag, observed_entry.last_tag);
}

}  // namespace file_system_provider
}  // namespace chromeos
