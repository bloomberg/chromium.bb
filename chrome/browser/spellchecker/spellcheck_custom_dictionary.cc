// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

#include <functional>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/md5.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

using content::BrowserThread;
using chrome::spellcheck_common::WordList;
using chrome::spellcheck_common::WordSet;

namespace {

// Filename extension for backup dictionary file.
const base::FilePath::CharType BACKUP_EXTENSION[] = FILE_PATH_LITERAL("backup");

// Prefix for the checksum in the dictionary file.
const char CHECKSUM_PREFIX[] = "checksum_v1 = ";

// The status of the checksum in a custom spellcheck dictionary.
enum ChecksumStatus {
  VALID_CHECKSUM,
  INVALID_CHECKSUM,
};

// The result of a dictionary sanitation. Can be used as a bitmap.
enum ChangeSanitationResult {
  // The change is valid and can be applied as-is.
  VALID_CHANGE = 0,

  // The change contained words to be added that are not valid.
  DETECTED_INVALID_WORDS = 1,

  // The change contained words to be added that are already in the dictionary.
  DETECTED_DUPLICATE_WORDS = 2,

  // The change contained words to be removed that are not in the dictionary.
  DETECTED_MISSING_WORDS = 4,
};

// Loads the file at |file_path| into the |words| container. If the file has a
// valid checksum, then returns ChecksumStatus::VALID. If the file has an
// invalid checksum, then returns ChecksumStatus::INVALID and clears |words|.
ChecksumStatus LoadFile(const base::FilePath& file_path, WordList& words) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  words.clear();
  std::string contents;
  base::ReadFileToString(file_path, &contents);
  size_t pos = contents.rfind(CHECKSUM_PREFIX);
  if (pos != std::string::npos) {
    std::string checksum = contents.substr(pos + strlen(CHECKSUM_PREFIX));
    contents = contents.substr(0, pos);
    if (checksum != base::MD5String(contents))
      return INVALID_CHECKSUM;
  }
  base::TrimWhitespaceASCII(contents, base::TRIM_ALL, &contents);
  base::SplitString(contents, '\n', &words);
  return VALID_CHECKSUM;
}

// Returns true for invalid words and false for valid words.
bool IsInvalidWord(const std::string& word) {
  std::string tmp;
  return !base::IsStringUTF8(word) ||
      word.length() >
          chrome::spellcheck_common::MAX_CUSTOM_DICTIONARY_WORD_BYTES ||
      word.empty() ||
      base::TRIM_NONE != base::TrimWhitespaceASCII(word, base::TRIM_ALL, &tmp);
}

// Loads the custom spellcheck dictionary from |path| into |custom_words|. If
// the dictionary checksum is not valid, but backup checksum is valid, then
// restores the backup and loads that into |custom_words| instead. If the backup
// is invalid too, then clears |custom_words|. Must be called on the file
// thread.
void LoadDictionaryFileReliably(WordList& custom_words,
                                const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Load the contents and verify the checksum.
  if (LoadFile(path, custom_words) == VALID_CHECKSUM)
    return;
  // Checksum is not valid. See if there's a backup.
  base::FilePath backup = path.AddExtension(BACKUP_EXTENSION);
  if (!base::PathExists(backup))
    return;
  // Load the backup and verify its checksum.
  if (LoadFile(backup, custom_words) != VALID_CHECKSUM)
    return;
  // Backup checksum is valid. Restore the backup.
  base::CopyFile(backup, path);
}

// Backs up the original dictionary, saves |custom_words| and its checksum into
// the custom spellcheck dictionary at |path|.
void SaveDictionaryFileReliably(
    const WordList& custom_words,
    const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::stringstream content;
  for (WordList::const_iterator it = custom_words.begin();
       it != custom_words.end();
       ++it) {
    content << *it << '\n';
  }
  std::string checksum = base::MD5String(content.str());
  content << CHECKSUM_PREFIX << checksum;
  base::CopyFile(path, path.AddExtension(BACKUP_EXTENSION));
  base::ImportantFileWriter::WriteFileAtomically(path, content.str());
}

// Removes duplicate and invalid words from |to_add| word list and sorts it.
// Looks for duplicates in both |to_add| and |existing| word lists. Returns a
// bitmap of |ChangeSanitationResult| values.
int SanitizeWordsToAdd(const WordSet& existing, WordList& to_add) {
  // Do not add duplicate words.
  std::sort(to_add.begin(), to_add.end());
  WordList new_words = base::STLSetDifference<WordList>(to_add, existing);
  new_words.erase(std::unique(new_words.begin(), new_words.end()),
                  new_words.end());
  int result = VALID_CHANGE;
  if (to_add.size() != new_words.size())
    result |= DETECTED_DUPLICATE_WORDS;
  // Do not add invalid words.
  size_t size = new_words.size();
  new_words.erase(std::remove_if(new_words.begin(),
                                 new_words.end(),
                                 IsInvalidWord),
                  new_words.end());
  if (size != new_words.size())
    result |= DETECTED_INVALID_WORDS;
  // Save the sanitized words to be added.
  std::swap(to_add, new_words);
  return result;
}

// Removes word from |to_remove| that are missing from |existing| word list and
// sorts |to_remove|. Returns a bitmap of |ChangeSanitationResult| values.
int SanitizeWordsToRemove(const WordSet& existing, WordList& to_remove) {
  // Do not remove words that are missing from the dictionary.
  std::sort(to_remove.begin(), to_remove.end());
  WordList found_words;
  std::set_intersection(existing.begin(),
                        existing.end(),
                        to_remove.begin(),
                        to_remove.end(),
                        std::back_inserter(found_words));
  int result = VALID_CHANGE;
  if (to_remove.size() > found_words.size())
    result |= DETECTED_MISSING_WORDS;
  // Save the sanitized words to be removed.
  std::swap(to_remove, found_words);
  return result;
}

}  // namespace


SpellcheckCustomDictionary::Change::Change() {
}

SpellcheckCustomDictionary::Change::Change(
    const SpellcheckCustomDictionary::Change& other)
    : to_add_(other.to_add()),
      to_remove_(other.to_remove()) {
}

SpellcheckCustomDictionary::Change::Change(const WordList& to_add)
    : to_add_(to_add) {
}

SpellcheckCustomDictionary::Change::~Change() {
}

void SpellcheckCustomDictionary::Change::AddWord(const std::string& word) {
  to_add_.push_back(word);
}

void SpellcheckCustomDictionary::Change::RemoveWord(const std::string& word) {
  to_remove_.push_back(word);
}

int SpellcheckCustomDictionary::Change::Sanitize(const WordSet& words) {
  int result = VALID_CHANGE;
  if (!to_add_.empty())
    result |= SanitizeWordsToAdd(words, to_add_);
  if (!to_remove_.empty())
    result |= SanitizeWordsToRemove(words, to_remove_);
  return result;
}

const WordList& SpellcheckCustomDictionary::Change::to_add() const {
  return to_add_;
}

const WordList& SpellcheckCustomDictionary::Change::to_remove() const {
  return to_remove_;
}

bool SpellcheckCustomDictionary::Change::empty() const {
  return to_add_.empty() && to_remove_.empty();
}

SpellcheckCustomDictionary::SpellcheckCustomDictionary(
    const base::FilePath& path)
    : custom_dictionary_path_(),
      is_loaded_(false),
      weak_ptr_factory_(this) {
  custom_dictionary_path_ =
      path.Append(chrome::kCustomDictionaryFileName);
}

SpellcheckCustomDictionary::~SpellcheckCustomDictionary() {
}

const WordSet& SpellcheckCustomDictionary::GetWords() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return words_;
}

bool SpellcheckCustomDictionary::AddWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Change dictionary_change;
  dictionary_change.AddWord(word);
  int result = dictionary_change.Sanitize(GetWords());
  Apply(dictionary_change);
  Notify(dictionary_change);
  Sync(dictionary_change);
  Save(dictionary_change);
  return result == VALID_CHANGE;
}

bool SpellcheckCustomDictionary::RemoveWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Change dictionary_change;
  dictionary_change.RemoveWord(word);
  int result = dictionary_change.Sanitize(GetWords());
  Apply(dictionary_change);
  Notify(dictionary_change);
  Sync(dictionary_change);
  Save(dictionary_change);
  return result == VALID_CHANGE;
}

bool SpellcheckCustomDictionary::HasWord(const std::string& word) const {
  return !!words_.count(word);
}

void SpellcheckCustomDictionary::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void SpellcheckCustomDictionary::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

bool SpellcheckCustomDictionary::IsLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return is_loaded_;
}

bool SpellcheckCustomDictionary::IsSyncing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return !!sync_processor_.get();
}

void SpellcheckCustomDictionary::Load() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SpellcheckCustomDictionary::LoadDictionaryFile,
                 custom_dictionary_path_),
      base::Bind(&SpellcheckCustomDictionary::OnLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

syncer::SyncMergeResult SpellcheckCustomDictionary::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!sync_processor_.get());
  DCHECK(!sync_error_handler_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_handler.get());
  DCHECK_EQ(syncer::DICTIONARY, type);
  sync_processor_ = sync_processor.Pass();
  sync_error_handler_ = sync_error_handler.Pass();

  // Build a list of words to add locally.
  WordList to_add_locally;
  for (syncer::SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end();
       ++it) {
    DCHECK_EQ(syncer::DICTIONARY, it->GetDataType());
    to_add_locally.push_back(it->GetSpecifics().dictionary().word());
  }

  // Add remote words locally.
  Change to_change_locally(to_add_locally);
  to_change_locally.Sanitize(GetWords());
  Apply(to_change_locally);
  Notify(to_change_locally);
  Save(to_change_locally);

  // Add as many as possible local words remotely.
  std::sort(to_add_locally.begin(), to_add_locally.end());
  WordList to_add_remotely = base::STLSetDifference<WordList>(words_,
                                                              to_add_locally);

  // Send local changes to the sync server.
  Change to_change_remotely(to_add_remotely);
  syncer::SyncMergeResult result(type);
  result.set_error(Sync(to_change_remotely));
  return result;
}

void SpellcheckCustomDictionary::StopSyncing(syncer::ModelType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(syncer::DICTIONARY, type);
  sync_processor_.reset();
  sync_error_handler_.reset();
}

syncer::SyncDataList SpellcheckCustomDictionary::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(syncer::DICTIONARY, type);
  syncer::SyncDataList data;
  std::string word;
  size_t i = 0;
  for (WordSet::const_iterator it = words_.begin();
       it != words_.end() &&
           i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS;
       ++it, ++i) {
    word = *it;
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    data.push_back(syncer::SyncData::CreateLocalData(word, word, specifics));
  }
  return data;
}

syncer::SyncError SpellcheckCustomDictionary::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Change dictionary_change;
  for (syncer::SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end();
       ++it) {
    DCHECK(it->IsValid());
    std::string word = it->sync_data().GetSpecifics().dictionary().word();
    switch (it->change_type()) {
      case syncer::SyncChange::ACTION_ADD:
        dictionary_change.AddWord(word);
        break;
      case syncer::SyncChange::ACTION_DELETE:
        dictionary_change.RemoveWord(word);
        break;
      default:
        return sync_error_handler_->CreateAndUploadError(
            FROM_HERE,
            "Processing sync changes failed on change type " +
                syncer::SyncChange::ChangeTypeToString(it->change_type()));
    }
  }

  dictionary_change.Sanitize(GetWords());
  Apply(dictionary_change);
  Notify(dictionary_change);
  Save(dictionary_change);

  return syncer::SyncError();
}

// static
WordList SpellcheckCustomDictionary::LoadDictionaryFile(
    const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WordList words;
  LoadDictionaryFileReliably(words, path);
  if (!words.empty() && VALID_CHANGE != SanitizeWordsToAdd(WordSet(), words))
    SaveDictionaryFileReliably(words, path);
  SpellCheckHostMetrics::RecordCustomWordCountStats(words.size());
  return words;
}

// static
void SpellcheckCustomDictionary::UpdateDictionaryFile(
    const SpellcheckCustomDictionary::Change& dictionary_change,
    const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (dictionary_change.empty())
    return;

  WordList custom_words;
  LoadDictionaryFileReliably(custom_words, path);

  // Add words.
  custom_words.insert(custom_words.end(),
                      dictionary_change.to_add().begin(),
                      dictionary_change.to_add().end());

  // Remove words.
  std::sort(custom_words.begin(), custom_words.end());
  WordList remaining =
      base::STLSetDifference<WordList>(custom_words,
                                       dictionary_change.to_remove());
  std::swap(custom_words, remaining);

  SaveDictionaryFileReliably(custom_words, path);
}

void SpellcheckCustomDictionary::OnLoaded(WordList custom_words) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Change dictionary_change(custom_words);
  dictionary_change.Sanitize(GetWords());
  Apply(dictionary_change);
  Sync(dictionary_change);
  is_loaded_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnCustomDictionaryLoaded());
}

void SpellcheckCustomDictionary::Apply(
    const SpellcheckCustomDictionary::Change& dictionary_change) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!dictionary_change.to_add().empty()) {
    words_.insert(dictionary_change.to_add().begin(),
                  dictionary_change.to_add().end());
  }
  if (!dictionary_change.to_remove().empty()) {
    WordSet updated_words =
        base::STLSetDifference<WordSet>(words_,
                                        dictionary_change.to_remove());
    std::swap(words_, updated_words);
  }
}

void SpellcheckCustomDictionary::Save(
    const SpellcheckCustomDictionary::Change& dictionary_change) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SpellcheckCustomDictionary::UpdateDictionaryFile,
                 dictionary_change,
                 custom_dictionary_path_));
}

syncer::SyncError SpellcheckCustomDictionary::Sync(
    const SpellcheckCustomDictionary::Change& dictionary_change) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  syncer::SyncError error;
  if (!IsSyncing() || dictionary_change.empty())
    return error;

  // The number of words on the sync server should not exceed the limits.
  int server_size = static_cast<int>(words_.size()) -
      static_cast<int>(dictionary_change.to_add().size());
  int max_upload_size = std::max(
      0,
      static_cast<int>(
          chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS) -
          server_size);
  int upload_size = std::min(
      static_cast<int>(dictionary_change.to_add().size()),
      max_upload_size);

  syncer::SyncChangeList sync_change_list;
  int i = 0;

  for (WordList::const_iterator it = dictionary_change.to_add().begin();
       it != dictionary_change.to_add().end() && i < upload_size;
       ++it, ++i) {
    std::string word = *it;
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    sync_change_list.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }

  for (WordList::const_iterator it = dictionary_change.to_remove().begin();
       it != dictionary_change.to_remove().end();
       ++it) {
    std::string word = *it;
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    sync_change_list.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }

  // Send the changes to the sync processor.
  error = sync_processor_->ProcessSyncChanges(FROM_HERE, sync_change_list);
  if (error.IsSet())
    return error;

  // Turn off syncing of this dictionary if the server already has the maximum
  // number of words.
  if (words_.size() > chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS)
    StopSyncing(syncer::DICTIONARY);

  return error;
}

void SpellcheckCustomDictionary::Notify(
    const SpellcheckCustomDictionary::Change& dictionary_change) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!IsLoaded() || dictionary_change.empty())
    return;
  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OnCustomDictionaryChanged(dictionary_change));
}
