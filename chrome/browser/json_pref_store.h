// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_JSON_PREF_STORE_H_
#define CHROME_BROWSER_JSON_PREF_STORE_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/important_file_writer.h"
#include "chrome/browser/pref_store.h"

class DictionaryValue;
class FilePath;

class JsonPrefStore : public PrefStore,
                      public ImportantFileWriter::DataSerializer {
 public:
  explicit JsonPrefStore(const FilePath& pref_filename);
  virtual ~JsonPrefStore();

  // PrefStore methods:
  virtual bool ReadOnly() { return read_only_; }

  virtual DictionaryValue* prefs() { return prefs_.get(); }

  virtual PrefReadError ReadPrefs();

  virtual bool WritePrefs();

  virtual void ScheduleWritePrefs();

  // ImportantFileWriter::DataSerializer methods:
  virtual bool SerializeData(std::string* data);

 private:
  FilePath path_;

  scoped_ptr<DictionaryValue> prefs_;

  bool read_only_;

  // Helper for safely writing pref data.
  ImportantFileWriter writer_;
};

#endif  // CHROME_BROWSER_JSON_PREF_STORE_H_
