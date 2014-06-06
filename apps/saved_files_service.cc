// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_files_service.h"

#include <algorithm>

#include "apps/saved_files_service_factory.h"
#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/value_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace apps {

using extensions::APIPermission;
using extensions::Extension;
using extensions::ExtensionHost;
using extensions::ExtensionPrefs;

namespace {

// Preference keys

// The file entries that the app has permission to access.
const char kFileEntries[] = "file_entries";

// The path to a file entry that the app had permission to access.
const char kFileEntryPath[] = "path";

// Whether or not the the entry refers to a directory.
const char kFileEntryIsDirectory[] = "is_directory";

// The sequence number in the LRU of the file entry.
const char kFileEntrySequenceNumber[] = "sequence_number";

const size_t kMaxSavedFileEntries = 500;
const int kMaxSequenceNumber = kint32max;

// These might be different to the constant values in tests.
size_t g_max_saved_file_entries = kMaxSavedFileEntries;
int g_max_sequence_number = kMaxSequenceNumber;

// Persists a SavedFileEntry in ExtensionPrefs.
void AddSavedFileEntry(ExtensionPrefs* prefs,
                       const std::string& extension_id,
                       const SavedFileEntry& file_entry) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      prefs, extension_id, kFileEntries);
  base::DictionaryValue* file_entries = update.Get();
  if (!file_entries)
    file_entries = update.Create();
  DCHECK(!file_entries->GetDictionaryWithoutPathExpansion(file_entry.id, NULL));

  base::DictionaryValue* file_entry_dict = new base::DictionaryValue();
  file_entry_dict->Set(kFileEntryPath, CreateFilePathValue(file_entry.path));
  file_entry_dict->SetBoolean(kFileEntryIsDirectory, file_entry.is_directory);
  file_entry_dict->SetInteger(kFileEntrySequenceNumber,
                              file_entry.sequence_number);
  file_entries->SetWithoutPathExpansion(file_entry.id, file_entry_dict);
}

// Updates the sequence_number of a SavedFileEntry persisted in ExtensionPrefs.
void UpdateSavedFileEntry(ExtensionPrefs* prefs,
                          const std::string& extension_id,
                          const SavedFileEntry& file_entry) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      prefs, extension_id, kFileEntries);
  base::DictionaryValue* file_entries = update.Get();
  DCHECK(file_entries);
  base::DictionaryValue* file_entry_dict = NULL;
  file_entries->GetDictionaryWithoutPathExpansion(file_entry.id,
                                                  &file_entry_dict);
  DCHECK(file_entry_dict);
  file_entry_dict->SetInteger(kFileEntrySequenceNumber,
                              file_entry.sequence_number);
}

// Removes a SavedFileEntry from ExtensionPrefs.
void RemoveSavedFileEntry(ExtensionPrefs* prefs,
                          const std::string& extension_id,
                          const std::string& file_entry_id) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      prefs, extension_id, kFileEntries);
  base::DictionaryValue* file_entries = update.Get();
  if (!file_entries)
    file_entries = update.Create();
  file_entries->RemoveWithoutPathExpansion(file_entry_id, NULL);
}

// Clears all SavedFileEntry for the app from ExtensionPrefs.
void ClearSavedFileEntries(ExtensionPrefs* prefs,
                           const std::string& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kFileEntries, NULL);
}

// Returns all SavedFileEntries for the app.
std::vector<SavedFileEntry> GetSavedFileEntries(
    ExtensionPrefs* prefs,
    const std::string& extension_id) {
  std::vector<SavedFileEntry> result;
  const base::DictionaryValue* file_entries = NULL;
  if (!prefs->ReadPrefAsDictionary(extension_id, kFileEntries, &file_entries))
    return result;

  for (base::DictionaryValue::Iterator it(*file_entries); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* file_entry = NULL;
    if (!it.value().GetAsDictionary(&file_entry))
      continue;
    const base::Value* path_value;
    if (!file_entry->Get(kFileEntryPath, &path_value))
      continue;
    base::FilePath file_path;
    if (!GetValueAsFilePath(*path_value, &file_path))
      continue;
    bool is_directory = false;
    file_entry->GetBoolean(kFileEntryIsDirectory, &is_directory);
    int sequence_number = 0;
    if (!file_entry->GetInteger(kFileEntrySequenceNumber, &sequence_number))
      continue;
    if (!sequence_number)
      continue;
    result.push_back(
        SavedFileEntry(it.key(), file_path, is_directory, sequence_number));
  }
  return result;
}

}  // namespace

SavedFileEntry::SavedFileEntry() : is_directory(false), sequence_number(0) {}

SavedFileEntry::SavedFileEntry(const std::string& id,
                               const base::FilePath& path,
                               bool is_directory,
                               int sequence_number)
    : id(id),
      path(path),
      is_directory(is_directory),
      sequence_number(sequence_number) {}

class SavedFilesService::SavedFiles {
 public:
  SavedFiles(Profile* profile, const std::string& extension_id);
  ~SavedFiles();

  void RegisterFileEntry(const std::string& id,
                         const base::FilePath& file_path,
                         bool is_directory);
  void EnqueueFileEntry(const std::string& id);
  bool IsRegistered(const std::string& id) const;
  const SavedFileEntry* GetFileEntry(const std::string& id) const;
  std::vector<SavedFileEntry> GetAllFileEntries() const;

 private:
  // Compacts sequence numbers if the largest sequence number is
  // g_max_sequence_number. Outside of testing, it is set to kint32max, so this
  // will almost never do any real work.
  void MaybeCompactSequenceNumbers();

  void LoadSavedFileEntriesFromPreferences();

  Profile* profile_;
  const std::string extension_id_;

  // Contains all file entries that have been registered, keyed by ID. Owns
  // values.
  base::hash_map<std::string, SavedFileEntry*> registered_file_entries_;
  STLValueDeleter<base::hash_map<std::string, SavedFileEntry*> >
      registered_file_entries_deleter_;

  // The queue of file entries that have been retained, keyed by
  // sequence_number. Values are a subset of values in registered_file_entries_.
  // This should be kept in sync with file entries stored in extension prefs.
  std::map<int, SavedFileEntry*> saved_file_lru_;

  DISALLOW_COPY_AND_ASSIGN(SavedFiles);
};

// static
SavedFilesService* SavedFilesService::Get(Profile* profile) {
  return SavedFilesServiceFactory::GetForProfile(profile);
}

SavedFilesService::SavedFilesService(Profile* profile)
    : extension_id_to_saved_files_deleter_(&extension_id_to_saved_files_),
      profile_(profile) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

SavedFilesService::~SavedFilesService() {}

void SavedFilesService::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      const Extension* extension = host->extension();
      if (extension) {
        ClearQueueIfNoRetainPermission(extension);
        Clear(extension->id());
      }
      break;
    }

    case chrome::NOTIFICATION_APP_TERMINATING: {
      // Stop listening to NOTIFICATION_EXTENSION_HOST_DESTROYED in particular
      // as all extension hosts will be destroyed as a result of shutdown.
      registrar_.RemoveAll();
      break;
    }
  }
}

void SavedFilesService::RegisterFileEntry(const std::string& extension_id,
                                          const std::string& id,
                                          const base::FilePath& file_path,
                                          bool is_directory) {
  GetOrInsert(extension_id)->RegisterFileEntry(id, file_path, is_directory);
}

void SavedFilesService::EnqueueFileEntry(const std::string& extension_id,
                                         const std::string& id) {
  GetOrInsert(extension_id)->EnqueueFileEntry(id);
}

std::vector<SavedFileEntry> SavedFilesService::GetAllFileEntries(
    const std::string& extension_id) {
  SavedFiles* saved_files = Get(extension_id);
  if (saved_files)
    return saved_files->GetAllFileEntries();
  return GetSavedFileEntries(ExtensionPrefs::Get(profile_), extension_id);
}

bool SavedFilesService::IsRegistered(const std::string& extension_id,
                                     const std::string& id) {
  return GetOrInsert(extension_id)->IsRegistered(id);
}

const SavedFileEntry* SavedFilesService::GetFileEntry(
    const std::string& extension_id,
    const std::string& id) {
  return GetOrInsert(extension_id)->GetFileEntry(id);
}

void SavedFilesService::ClearQueueIfNoRetainPermission(
    const Extension* extension) {
  if (extensions::util::IsEphemeralApp(extension->id(), profile_) ||
      !extension->permissions_data()->active_permissions()->HasAPIPermission(
          APIPermission::kFileSystemRetainEntries)) {
    ClearQueue(extension);
  }
}

void SavedFilesService::ClearQueue(const extensions::Extension* extension) {
  ClearSavedFileEntries(ExtensionPrefs::Get(profile_), extension->id());
  Clear(extension->id());
}

SavedFilesService::SavedFiles* SavedFilesService::Get(
    const std::string& extension_id) const {
  std::map<std::string, SavedFiles*>::const_iterator it =
      extension_id_to_saved_files_.find(extension_id);
  if (it != extension_id_to_saved_files_.end())
    return it->second;

  return NULL;
}

SavedFilesService::SavedFiles* SavedFilesService::GetOrInsert(
    const std::string& extension_id) {
  SavedFiles* saved_files = Get(extension_id);
  if (saved_files)
    return saved_files;

  saved_files = new SavedFiles(profile_, extension_id);
  extension_id_to_saved_files_.insert(
      std::make_pair(extension_id, saved_files));
  return saved_files;
}

void SavedFilesService::Clear(const std::string& extension_id) {
  std::map<std::string, SavedFiles*>::iterator it =
      extension_id_to_saved_files_.find(extension_id);
  if (it != extension_id_to_saved_files_.end()) {
    delete it->second;
    extension_id_to_saved_files_.erase(it);
  }
}

SavedFilesService::SavedFiles::SavedFiles(Profile* profile,
                                          const std::string& extension_id)
    : profile_(profile),
      extension_id_(extension_id),
      registered_file_entries_deleter_(&registered_file_entries_) {
  LoadSavedFileEntriesFromPreferences();
}

SavedFilesService::SavedFiles::~SavedFiles() {}

void SavedFilesService::SavedFiles::RegisterFileEntry(
    const std::string& id,
    const base::FilePath& file_path,
    bool is_directory) {
  if (ContainsKey(registered_file_entries_, id))
    return;

  registered_file_entries_.insert(
      std::make_pair(id, new SavedFileEntry(id, file_path, is_directory, 0)));
}

void SavedFilesService::SavedFiles::EnqueueFileEntry(const std::string& id) {
  base::hash_map<std::string, SavedFileEntry*>::iterator it =
      registered_file_entries_.find(id);
  DCHECK(it != registered_file_entries_.end());

  SavedFileEntry* file_entry = it->second;
  int old_sequence_number = file_entry->sequence_number;
  if (!saved_file_lru_.empty()) {
    // Get the sequence number after the last file entry in the LRU.
    std::map<int, SavedFileEntry*>::reverse_iterator it =
        saved_file_lru_.rbegin();
    if (it->second == file_entry)
      return;

    file_entry->sequence_number = it->first + 1;
  } else {
    // The first sequence number is 1, as 0 means the entry is not in the LRU.
    file_entry->sequence_number = 1;
  }
  saved_file_lru_.insert(
      std::make_pair(file_entry->sequence_number, file_entry));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  if (old_sequence_number) {
    saved_file_lru_.erase(old_sequence_number);
    UpdateSavedFileEntry(prefs, extension_id_, *file_entry);
  } else {
    AddSavedFileEntry(prefs, extension_id_, *file_entry);
    if (saved_file_lru_.size() > g_max_saved_file_entries) {
      std::map<int, SavedFileEntry*>::iterator it = saved_file_lru_.begin();
      it->second->sequence_number = 0;
      RemoveSavedFileEntry(prefs, extension_id_, it->second->id);
      saved_file_lru_.erase(it);
    }
  }
  MaybeCompactSequenceNumbers();
}

bool SavedFilesService::SavedFiles::IsRegistered(const std::string& id) const {
  return ContainsKey(registered_file_entries_, id);
}

const SavedFileEntry* SavedFilesService::SavedFiles::GetFileEntry(
    const std::string& id) const {
  base::hash_map<std::string, SavedFileEntry*>::const_iterator it =
      registered_file_entries_.find(id);
  if (it == registered_file_entries_.end())
    return NULL;

  return it->second;
}

std::vector<SavedFileEntry> SavedFilesService::SavedFiles::GetAllFileEntries()
    const {
  std::vector<SavedFileEntry> result;
  for (base::hash_map<std::string, SavedFileEntry*>::const_iterator it =
           registered_file_entries_.begin();
       it != registered_file_entries_.end();
       ++it) {
    result.push_back(*it->second);
  }
  return result;
}

void SavedFilesService::SavedFiles::MaybeCompactSequenceNumbers() {
  DCHECK_GE(g_max_sequence_number, 0);
  DCHECK_GE(static_cast<size_t>(g_max_sequence_number),
            g_max_saved_file_entries);
  std::map<int, SavedFileEntry*>::reverse_iterator it =
      saved_file_lru_.rbegin();
  if (it == saved_file_lru_.rend())
    return;

  // Only compact sequence numbers if the last entry's sequence number is the
  // maximum value.  This should almost never be the case.
  if (it->first < g_max_sequence_number)
    return;

  int sequence_number = 0;
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  for (std::map<int, SavedFileEntry*>::iterator it = saved_file_lru_.begin();
       it != saved_file_lru_.end();
       ++it) {
    sequence_number++;
    if (it->second->sequence_number == sequence_number)
      continue;

    SavedFileEntry* file_entry = it->second;
    file_entry->sequence_number = sequence_number;
    UpdateSavedFileEntry(prefs, extension_id_, *file_entry);
    saved_file_lru_.erase(it++);
    // Provide the following element as an insert hint. While optimized
    // insertion time with the following element as a hint is only supported by
    // the spec in C++11, the implementations do support this.
    it = saved_file_lru_.insert(
        it, std::make_pair(file_entry->sequence_number, file_entry));
  }
}

void SavedFilesService::SavedFiles::LoadSavedFileEntriesFromPreferences() {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  std::vector<SavedFileEntry> saved_entries =
      GetSavedFileEntries(prefs, extension_id_);
  for (std::vector<SavedFileEntry>::iterator it = saved_entries.begin();
       it != saved_entries.end();
       ++it) {
    SavedFileEntry* file_entry = new SavedFileEntry(*it);
    registered_file_entries_.insert(std::make_pair(file_entry->id, file_entry));
    saved_file_lru_.insert(
        std::make_pair(file_entry->sequence_number, file_entry));
  }
}

// static
void SavedFilesService::SetMaxSequenceNumberForTest(int max_value) {
  g_max_sequence_number = max_value;
}

// static
void SavedFilesService::ClearMaxSequenceNumberForTest() {
  g_max_sequence_number = kMaxSequenceNumber;
}

// static
void SavedFilesService::SetLruSizeForTest(int size) {
  g_max_saved_file_entries = size;
}

// static
void SavedFilesService::ClearLruSizeForTest() {
  g_max_saved_file_entries = kMaxSavedFileEntries;
}

}  // namespace apps
