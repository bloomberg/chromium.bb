// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_DATA_STORE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_DATA_STORE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/search/common/dictionary_data_store.h"
#include "chrome/browser/ui/app_list/search/history_data.h"

namespace base {
class DictionaryValue;
class SequencedTaskRunner;
}

namespace app_list {

namespace test {
class HistoryDataStoreTest;
}

// A simple json store to persist HistoryData.
class HistoryDataStore : public base::RefCountedThreadSafe<HistoryDataStore> {
 public:
  typedef base::Callback<void(scoped_ptr<HistoryData::Associations>)>
      OnLoadedCallback;

  // A data store with no storage backend.
  HistoryDataStore();

  // |data_store| stores the history into the file.
  explicit HistoryDataStore(scoped_refptr<DictionaryDataStore> data_store);

  // Flushes pending writes. |on_flushed| is invoked when disk write is
  // finished.
  void Flush(const DictionaryDataStore::OnFlushedCallback& on_flushed);

  // Reads the persisted data from disk asynchronously. |on_read| is called
  // with the loaded and parsed data. If there is an error, |on_read| is called
  // without data.
  void Load(const OnLoadedCallback& on_loaded);

  // Incremental changes allowed on the data store.
  void SetPrimary(const std::string& query, const std::string& result);
  void SetSecondary(const std::string& query,
                    const HistoryData::SecondaryDeque& results);
  void SetUpdateTime(const std::string& query, const base::Time& update_time);
  void Delete(const std::string& query);

 private:
  friend class base::RefCountedThreadSafe<HistoryDataStore>;
  friend class app_list::test::HistoryDataStoreTest;

  virtual ~HistoryDataStore();

  void Init(base::DictionaryValue* cached_dict);

  // Gets the dictionary for "associations" key.
  base::DictionaryValue* GetAssociationDict();

  // Gets entry dictionary for given |query|. Creates one if necessary.
  base::DictionaryValue* GetEntryDict(const std::string& query);

  void OnDictionaryLoadedCallback(OnLoadedCallback callback,
                                  scoped_ptr<base::DictionaryValue> dict);

  // |cached_dict_| and |data_store_| is mutually exclusive. |data_store_| is
  // used if it's backed by a file storage, otherwise |cache_dict_| keeps
  // on-memory data.
  scoped_ptr<base::DictionaryValue> cached_dict_;
  scoped_refptr<DictionaryDataStore> data_store_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDataStore);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_DATA_STORE_H_
