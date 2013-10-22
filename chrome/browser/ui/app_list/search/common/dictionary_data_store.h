// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_DICTIONARY_DATA_STORE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_DICTIONARY_DATA_STORE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class SequencedTaskRunner;
}

namespace app_list {

// A simple JSON store to persist a dictionary.
class DictionaryDataStore
    : public base::RefCountedThreadSafe<DictionaryDataStore>,
      public base::ImportantFileWriter::DataSerializer {
 public:
  typedef base::Callback<void(
      scoped_ptr<base::DictionaryValue>)> OnLoadedCallback;
  typedef base::Closure OnFlushedCallback;

  explicit DictionaryDataStore(const base::FilePath& data_file);

  // Flushes pending writes.
  void Flush(const OnFlushedCallback& on_flushed);

  // Reads the persisted data from disk asynchronously. |on_read| is called
  // with the loaded and parsed data. If there is an error, |on_read| is called
  // without data.
  void Load(const OnLoadedCallback& on_loaded);

  // Schedule a job to persist the cached dictionary.
  void ScheduleWrite();

  // Used to get a pointer to the cached dictionary. Changes to this dictionary
  // will not be persisted unless ScheduleWrite() is called.
  base::DictionaryValue* cached_dict() { return cached_dict_.get(); }

 private:
  friend class base::RefCountedThreadSafe<DictionaryDataStore>;

  virtual ~DictionaryDataStore();

  // Reads data from backing file.
  scoped_ptr<base::DictionaryValue> LoadOnBlockingPool();

  // ImportantFileWriter::DataSerializer overrides:
  virtual bool SerializeData(std::string* data) OVERRIDE;

  base::FilePath data_file_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  scoped_ptr<base::ImportantFileWriter> writer_;

  // Cached JSON dictionary to serve read and incremental change calls.
  scoped_ptr<base::DictionaryValue> cached_dict_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryDataStore);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_DICTIONARY_DATA_STORE_H_
