// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_JSON_PREF_STORE_H_
#define CHROME_COMMON_JSON_PREF_STORE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/common/important_file_writer.h"
#include "chrome/common/persistent_pref_store.h"

namespace base {
class MessageLoopProxy;
}

class DictionaryValue;
class FilePath;
class Value;

// A writable PrefStore implementation that is used for user preferences.
class JsonPrefStore : public PersistentPrefStore,
                      public ImportantFileWriter::DataSerializer {
 public:
  class Delegate {
   public:
    virtual void OnPrefsRead(PrefReadError error, bool no_dir) = 0;
  };

  // |file_message_loop_proxy| is the MessageLoopProxy for a thread on which
  // file I/O can be done.
  JsonPrefStore(const FilePath& pref_filename,
                base::MessageLoopProxy* file_message_loop_proxy);
  virtual ~JsonPrefStore();

  // PrefStore overrides:
  virtual ReadResult GetValue(const std::string& key,
                              const Value** result) const;
  virtual void AddObserver(PrefStore::Observer* observer);
  virtual void RemoveObserver(PrefStore::Observer* observer);

  // PersistentPrefStore overrides:
  virtual ReadResult GetMutableValue(const std::string& key, Value** result);
  virtual void SetValue(const std::string& key, Value* value);
  virtual void SetValueSilently(const std::string& key, Value* value);
  virtual void RemoveValue(const std::string& key);
  virtual bool ReadOnly() const;
  virtual PrefReadError ReadPrefs();
  // todo(altimofeev): move it to the PersistentPrefStore inteface.
  void ReadPrefs(Delegate* delegate);
  virtual bool WritePrefs();
  virtual void ScheduleWritePrefs();
  virtual void CommitPendingWrite();
  virtual void ReportValueChanged(const std::string& key);

  // This method is called after JSON file has been read. Method takes
  // ownership of the |value| pointer.
  void OnFileRead(Value* value_owned, PrefReadError error, bool no_dir);

 private:
  // ImportantFileWriter::DataSerializer overrides:
  virtual bool SerializeData(std::string* output);

  FilePath path_;

  scoped_ptr<DictionaryValue> prefs_;

  bool read_only_;

  // Helper for safely writing pref data.
  ImportantFileWriter writer_;

  ObserverList<PrefStore::Observer, true> observers_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(JsonPrefStore);
};

#endif  // CHROME_COMMON_JSON_PREF_STORE_H_
