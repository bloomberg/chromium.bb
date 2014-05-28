// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_LEVELDB_PREF_STORE_H_
#define CHROME_BROWSER_PREFS_LEVELDB_PREF_STORE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/observer_list.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/pref_value_map.h"
#include "base/timer/timer.h"

namespace base {
class DictionaryValue;
class SequencedTaskRunner;
class Value;
}

namespace leveldb {
class DB;
}

// A writable PrefStore implementation that is used for user preferences.
class LevelDBPrefStore : public PersistentPrefStore {
 public:
  // |sequenced_task_runner| is must be a shutdown-blocking task runner, ideally
  // created by GetTaskRunnerForFile() method above.
  LevelDBPrefStore(const base::FilePath& pref_filename,
                   base::SequencedTaskRunner* sequenced_task_runner);

  // PrefStore overrides:
  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const OVERRIDE;
  virtual void AddObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual bool HasObservers() const OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;

  // PersistentPrefStore overrides:
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) OVERRIDE;
  // Takes ownership of value.
  virtual void SetValue(const std::string& key, base::Value* value) OVERRIDE;
  virtual void SetValueSilently(const std::string& key,
                                base::Value* value) OVERRIDE;
  virtual void RemoveValue(const std::string& key) OVERRIDE;
  virtual bool ReadOnly() const OVERRIDE;
  virtual PrefReadError GetReadError() const OVERRIDE;
  virtual PrefReadError ReadPrefs() OVERRIDE;
  virtual void ReadPrefsAsync(ReadErrorDelegate* error_delegate) OVERRIDE;
  virtual void CommitPendingWrite() OVERRIDE;
  virtual void ReportValueChanged(const std::string& key) OVERRIDE;

 private:
  struct ReadingResults;
  class FileThreadSerializer;

  virtual ~LevelDBPrefStore();

  static scoped_ptr<ReadingResults> DoReading(const base::FilePath& path);
  static void OpenDB(const base::FilePath& path,
                     ReadingResults* reading_results);
  void OnStorageRead(scoped_ptr<ReadingResults> reading_results);

  void PersistFromUIThread();
  void RemoveFromUIThread(const std::string& key);
  void ScheduleWrite();

  void SetValueInternal(const std::string& key,
                        base::Value* value,
                        bool notify);
  void NotifyObservers(const std::string& key);
  void MarkForInsertion(const std::string& key, const std::string& value);
  void MarkForDeletion(const std::string& key);

  base::FilePath path_;

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> original_task_runner_;

  PrefValueMap prefs_;

  bool read_only_;

  ObserverList<PrefStore::Observer, true> observers_;

  scoped_ptr<ReadErrorDelegate> error_delegate_;

  bool initialized_;
  PrefReadError read_error_;

  // This object is created on the UI thread right after preferences are loaded
  // from disk. A message to delete it is sent to the FILE thread by
  // ~LevelDBPrefStore.
  scoped_ptr<FileThreadSerializer> serializer_;

  // Changes are accumulated in |keys_to_delete_| and |keys_to_set_| and are
  // stored in the database according to |timer_|.
  std::set<std::string> keys_to_delete_;
  std::map<std::string, std::string> keys_to_set_;
  base::OneShotTimer<LevelDBPrefStore> timer_;

  base::WeakPtrFactory<LevelDBPrefStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBPrefStore);
};

#endif  // CHROME_BROWSER_PREFS_LEVELDB_PREF_STORE_H_
