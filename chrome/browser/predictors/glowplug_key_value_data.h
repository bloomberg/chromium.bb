// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_GLOWPLUG_KEY_VALUE_DATA_H_
#define CHROME_BROWSER_PREDICTORS_GLOWPLUG_KEY_VALUE_DATA_H_

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/predictors/glowplug_key_value_table.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "content/public/browser/browser_thread.h"

class PredictorsHandler;

namespace predictors {

// The class provides a synchronous access to the data backed by
// GlowplugKeyValueTable<T>. The current implementation caches all the data in
// the memory. The cache size is limited by max_size parameter using Compare
// function to decide which entry should be evicted.
//
// InitializeOnDBSequence() must be called on the DB sequence of the
// ResourcePrefetchPredictorTables. All other methods must be called on UI
// thread.
template <typename T, typename Compare>
class GlowplugKeyValueData {
 public:
  GlowplugKeyValueData(scoped_refptr<ResourcePrefetchPredictorTables> tables,
                       GlowplugKeyValueTable<T>* backend,
                       size_t max_size);

  // Must be called on the DB sequence of the ResourcePrefetchPredictorTables
  // before calling all other methods.
  void InitializeOnDBSequence();

  // Assigns data associated with the |key| to |data|. Returns true iff the
  // |key| exists, false otherwise. |data| pointer may be nullptr to get the
  // return value only.
  bool TryGetData(const std::string& key, T* data) const;

  // Assigns data associated with the |key| to |data|.
  void UpdateData(const std::string& key, const T& data);

  // Deletes data associated with the |keys| from the database.
  void DeleteData(const std::vector<std::string>& keys);

  // Deletes all entries from the database.
  void DeleteAllData();

 private:
  friend class ResourcePrefetchPredictorTest;
  friend class ::PredictorsHandler;

  struct EntryCompare : private Compare {
    bool operator()(const std::pair<std::string, T>& lhs,
                    const std::pair<std::string, T>& rhs) {
      return Compare::operator()(lhs.second, rhs.second);
    }
  };

  scoped_refptr<ResourcePrefetchPredictorTables> tables_;
  GlowplugKeyValueTable<T>* backend_table_;
  std::unique_ptr<std::map<std::string, T>> data_cache_;
  const size_t max_size_;
  EntryCompare entry_compare_;

  DISALLOW_COPY_AND_ASSIGN(GlowplugKeyValueData);
};

template <typename T, typename Compare>
GlowplugKeyValueData<T, Compare>::GlowplugKeyValueData(
    scoped_refptr<ResourcePrefetchPredictorTables> tables,
    GlowplugKeyValueTable<T>* backend,
    size_t max_size)
    : tables_(tables),
      backend_table_(backend),
      data_cache_(),
      max_size_(max_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

template <typename T, typename Compare>
void GlowplugKeyValueData<T, Compare>::InitializeOnDBSequence() {
  DCHECK(tables_->GetTaskRunner()->RunsTasksInCurrentSequence());
  auto data_map = base::MakeUnique<std::map<std::string, T>>();
  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<T>::GetAllData,
                     base::Unretained(backend_table_), data_map.get()));

  // To ensure invariant that data_cache_.size() <= max_size_.
  std::vector<std::string> keys_to_delete;
  while (data_map->size() > max_size_) {
    auto entry_to_delete =
        std::min_element(data_map->begin(), data_map->end(), entry_compare_);
    keys_to_delete.emplace_back(entry_to_delete->first);
    data_map->erase(entry_to_delete);
  }
  if (keys_to_delete.size() > 0) {
    tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
        &GlowplugKeyValueTable<T>::DeleteData, base::Unretained(backend_table_),
        std::vector<std::string>(keys_to_delete)));
  }

  data_cache_ = std::move(data_map);
}

template <typename T, typename Compare>
bool GlowplugKeyValueData<T, Compare>::TryGetData(const std::string& key,
                                                  T* data) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(data_cache_);
  auto it = data_cache_->find(key);
  if (it == data_cache_->end())
    return false;

  if (data)
    *data = it->second;
  return true;
}

template <typename T, typename Compare>
void GlowplugKeyValueData<T, Compare>::UpdateData(const std::string& key,
                                                  const T& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(data_cache_);
  auto it = data_cache_->find(key);
  if (it == data_cache_->end()) {
    if (data_cache_->size() == max_size_) {
      auto entry_to_delete = std::min_element(
          data_cache_->begin(), data_cache_->end(), entry_compare_);
      const std::string& key_to_delete = entry_to_delete->first;
      tables_->ScheduleDBTask(
          FROM_HERE, base::BindOnce(&GlowplugKeyValueTable<T>::DeleteData,
                                    base::Unretained(backend_table_),
                                    std::vector<std::string>({key_to_delete})));
      data_cache_->erase(entry_to_delete);
    }
    data_cache_->emplace(key, data);
  } else {
    it->second = data;
  }

  tables_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&GlowplugKeyValueTable<T>::UpdateData,
                                base::Unretained(backend_table_), key, data));
}

template <typename T, typename Compare>
void GlowplugKeyValueData<T, Compare>::DeleteData(
    const std::vector<std::string>& keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(data_cache_);
  std::vector<std::string> keys_found;
  for (const std::string& key : keys) {
    if (data_cache_->erase(key))
      keys_found.emplace_back(key);
  }

  tables_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&GlowplugKeyValueTable<T>::DeleteData,
                                base::Unretained(backend_table_), keys_found));
}

template <typename T, typename Compare>
void GlowplugKeyValueData<T, Compare>::DeleteAllData() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(data_cache_);
  data_cache_->clear();
  tables_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&GlowplugKeyValueTable<T>::DeleteAllData,
                                base::Unretained(backend_table_)));
}

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_GLOWPLUG_KEY_VALUE_DATA_H_
