// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_

#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/spellchecker/spellcheck_dictionary.h"
#include "chrome/common/spellcheck_common.h"
#include "sync/api/syncable_service.h"

// Defines a custom dictionary where users can add their own words. All words
// must be UTF8, between 1 and 99 bytes long, and without ASCII whitespace. The
// dictionary contains its own checksum when saved on disk. Example dictionary
// file contents:
//
//   bar
//   foo
//   checksum_v1 = ec3df4034567e59e119fcf87f2d9bad4
//
class SpellcheckCustomDictionary : public SpellcheckDictionary,
                                   public syncer::SyncableService {
 public:
  // A change to the dictionary.
  class Change {
   public:
    Change();
    Change(const Change& other);
    explicit Change(const chrome::spellcheck_common::WordList& to_add);
    ~Change();

    // Adds |word| in this change.
    void AddWord(const std::string& word);

    // Removes |word| in this change.
    void RemoveWord(const std::string& word);

    // Prepares this change to be applied to |words| by removing duplicate and
    // invalid words from words to be added, removing missing words from words
    // to be removed, and sorting both lists of words. Assumes that |words| is
    // sorted. Returns a bitmap of |ChangeSanitationResult| values.
    int Sanitize(const chrome::spellcheck_common::WordList& words);

    // Returns the words to be added in this change.
    const chrome::spellcheck_common::WordList& to_add() const;

    // Returns the words to be removed in this change.
    const chrome::spellcheck_common::WordList& to_remove() const;

    // Returns true if there are no changes to be made. Otherwise returns false.
    bool empty() const;

   private:
    // The words to be added.
    chrome::spellcheck_common::WordList to_add_;

    // The words to be removed.
    chrome::spellcheck_common::WordList to_remove_;
  };

  // Interface to implement for dictionary load and change observers.
  class Observer {
   public:
    // Called when the custom dictionary has been loaded.
    virtual void OnCustomDictionaryLoaded() = 0;

    // Called when the custom dictionary has been changed.
    virtual void OnCustomDictionaryChanged(const Change& dictionary_change) = 0;
  };

  explicit SpellcheckCustomDictionary(Profile* profile);
  virtual ~SpellcheckCustomDictionary();

  // Returns the in-memory cache of words in the custom dictionary.
  const chrome::spellcheck_common::WordList& GetWords() const;

  // Adds |word| to the dictionary, schedules a write to disk, and notifies
  // observers of the change. Returns true if |word| is valid and not a
  // duplicate. Otherwise returns false.
  bool AddWord(const std::string& word);

  // Removes |word| from the dictionary, schedules a write to disk, and notifies
  // observers of the change. Returns true if |word| was found. Otherwise
  // returns false.
  bool RemoveWord(const std::string& word);

  // Adds |observer| to be notified of dictionary events and changes.
  void AddObserver(Observer* observer);

  // Removes |observer| to stop notifications of dictionary events and changes.
  void RemoveObserver(Observer* observer);

  // Returns true if the dictionary has been loaded. Otherwise returns false.
  bool IsLoaded();

  // Returns true if the dictionary is being synced. Otherwise returns false.
  bool IsSyncing();

  // Overridden from SpellcheckDictionary:
  virtual void Load() OVERRIDE;

  // Overridden from syncer::SyncableService:
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> sync_error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

 private:
  friend class DictionarySyncIntegrationTestHelper;
  friend class SpellcheckCustomDictionaryTest;

  // Returns the list of words in the custom spellcheck dictionary at |path|.
  // Makes sure that the custom dictionary file does not have duplicates and
  // contains only valid words.
  static chrome::spellcheck_common::WordList LoadDictionaryFile(
      const FilePath& path);

  // Applies the change in |dictionary_change| to the custom spellcheck
  // dictionary. Assumes that |dictionary_change| has been sanitized.
  static void UpdateDictionaryFile(
      const Change& dictionary_change,
      const FilePath& path);

  // The reply point for PostTaskAndReplyWithResult, called when
  // LoadDictionaryFile finishes reading the dictionary file. Does not modify
  // |custom_words|, but cannot be a const-ref due to the signature of
  // PostTaskAndReplyWithResult.
  void OnLoaded(chrome::spellcheck_common::WordList custom_words);

  // Applies the |dictionary_change| to the in-memory copy of the dictionary.
  // Assumes that words in |dictionary_change| are sorted.
  void Apply(const Change& dictionary_change);

  // Schedules a write of |dictionary_change| to disk. Assumes that words in
  // |dictionary_change| are sorted.
  void Save(const Change& dictionary_change);

  // Notifies the sync service of the |dictionary_change|. Syncs up to the
  // maximum syncable words on the server. Disables syncing of this dictionary
  // if the server contains the maximum number of syncable words.
  syncer::SyncError Sync(const Change& dictionary_change);

  // Notifies observers of the dictionary change if the dictionary has been
  // changed.
  void Notify(const Change& dictionary_change);

  // In-memory cache of the custom words file.
  chrome::spellcheck_common::WordList words_;

  // A path for custom dictionary per profile.
  FilePath custom_dictionary_path_;

  // Used to create weak pointers for an instance of this class.
  base::WeakPtrFactory<SpellcheckCustomDictionary> weak_ptr_factory_;

  // Observers for changes in dictionary load status and content changes.
  ObserverList<Observer> observers_;

  // Used to send local changes to the sync infrastructure.
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Used to send sync-related errors to the sync infrastructure.
  scoped_ptr<syncer::SyncErrorFactory> sync_error_handler_;

  // True if the dictionary has been loaded. Otherwise false.
  bool is_loaded_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckCustomDictionary);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_
