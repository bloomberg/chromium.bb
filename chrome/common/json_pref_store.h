// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_JSON_PREF_STORE_H_
#define CHROME_COMMON_JSON_PREF_STORE_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/common/pref_store.h"
#include "chrome/common/important_file_writer.h"

namespace base {
class MessageLoopProxy;
}

class DictionaryValue;
class FilePath;

class JsonPrefStore : public PrefStore,
                      public ImportantFileWriter::DataSerializer {
 public:
  // |file_message_loop_proxy| is the MessageLoopProxy for a thread on which
  // file I/O can be done.
  JsonPrefStore(const FilePath& pref_filename,
                base::MessageLoopProxy* file_message_loop_proxy);
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

#endif  // CHROME_COMMON_JSON_PREF_STORE_H_

