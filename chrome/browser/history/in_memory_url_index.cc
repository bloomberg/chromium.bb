// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index.h"

#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/url_constants.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;
using in_memory_url_index::InMemoryURLIndexCacheItem;

namespace history {

typedef imui::InMemoryURLIndexCacheItem_WordListItem WordListItem;
typedef imui::InMemoryURLIndexCacheItem_WordMapItem_WordMapEntry WordMapEntry;
typedef imui::InMemoryURLIndexCacheItem_WordMapItem WordMapItem;
typedef imui::InMemoryURLIndexCacheItem_CharWordMapItem CharWordMapItem;
typedef imui::InMemoryURLIndexCacheItem_CharWordMapItem_CharWordMapEntry
    CharWordMapEntry;
typedef imui::InMemoryURLIndexCacheItem_WordIDHistoryMapItem
    WordIDHistoryMapItem;
typedef imui::
    InMemoryURLIndexCacheItem_WordIDHistoryMapItem_WordIDHistoryMapEntry
    WordIDHistoryMapEntry;
typedef imui::InMemoryURLIndexCacheItem_HistoryInfoMapItem HistoryInfoMapItem;
typedef imui::InMemoryURLIndexCacheItem_HistoryInfoMapItem_HistoryInfoMapEntry
    HistoryInfoMapEntry;

InMemoryURLIndex::InMemoryURLIndex(const FilePath& history_dir)
    : history_dir_(history_dir),
      private_data_(new URLIndexPrivateData),
      cached_at_shutdown_(false),
      pre_filter_item_count(0),
      post_filter_item_count(0),
      post_scoring_item_count(0) {
  InMemoryURLIndex::InitializeSchemeWhitelist(&scheme_whitelist_);
}

// Called only by unit tests.
InMemoryURLIndex::InMemoryURLIndex()
    : private_data_(new URLIndexPrivateData),
      cached_at_shutdown_(false),
      pre_filter_item_count(0),
      post_filter_item_count(0),
      post_scoring_item_count(0) {
  InMemoryURLIndex::InitializeSchemeWhitelist(&scheme_whitelist_);
}

InMemoryURLIndex::~InMemoryURLIndex() {
  // If there was a history directory (which there won't be for some unit tests)
  // then insure that the cache has already been saved.
  DCHECK(history_dir_.empty() || cached_at_shutdown_);
}

// static
void InMemoryURLIndex::InitializeSchemeWhitelist(
    std::set<std::string>* whitelist) {
  DCHECK(whitelist);
  whitelist->insert(std::string(chrome::kAboutScheme));
  whitelist->insert(std::string(chrome::kChromeUIScheme));
  whitelist->insert(std::string(chrome::kFileScheme));
  whitelist->insert(std::string(chrome::kFtpScheme));
  whitelist->insert(std::string(chrome::kHttpScheme));
  whitelist->insert(std::string(chrome::kHttpsScheme));
  whitelist->insert(std::string(chrome::kMailToScheme));
}

bool InMemoryURLIndex::Init(URLDatabase* history_db,
                            const std::string& languages) {
  // TODO(mrossetti): Register for profile/language change notifications.
  languages_ = languages;
  return ReloadFromHistory(history_db, false);
}

void InMemoryURLIndex::ShutDown() {
  // Write our cache.
  SaveToCacheFile();
  cached_at_shutdown_ = true;
}

bool InMemoryURLIndex::ReloadFromHistory(history::URLDatabase* history_db,
                                         bool clear_cache) {
  ClearPrivateData();

  if (!history_db)
    return false;

  if (clear_cache || !RestoreFromCacheFile()) {
    base::TimeTicks beginning_time = base::TimeTicks::Now();
    // The index has to be built from scratch.
    URLDatabase::URLEnumerator history_enum;
    if (!history_db->InitURLEnumeratorForSignificant(&history_enum))
      return false;
    URLRow row;
    while (history_enum.GetNextURL(&row))
      IndexRow(row);
    UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexingTime",
                        base::TimeTicks::Now() - beginning_time);
    SaveToCacheFile();
  }
  return true;
}

void InMemoryURLIndex::ClearPrivateData() {
  private_data_->Clear();
  search_term_cache_.clear();
}

bool InMemoryURLIndex::RestoreFromCacheFile() {
  // TODO(mrossetti): Figure out how to determine if the cache is up-to-date.
  // That is: ensure that the database has not been modified since the cache
  // was last saved. DB file modification date is inadequate. There are no
  // SQLite table checksums automatically stored.
  // FIXME(mrossetti): Move File IO to another thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::TimeTicks beginning_time = base::TimeTicks::Now();
  FilePath file_path;
  if (!GetCacheFilePath(&file_path) || !file_util::PathExists(file_path))
    return false;
  std::string data;
  if (!file_util::ReadFileToString(file_path, &data)) {
    LOG(WARNING) << "Failed to read InMemoryURLIndex cache from "
                 << file_path.value();
    return false;
  }

  InMemoryURLIndexCacheItem index_cache;
  if (!index_cache.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse InMemoryURLIndex cache data read from "
                 << file_path.value();
    return false;
  }

  if (!RestorePrivateData(index_cache)) {
    ClearPrivateData();  // Back to square one -- must build from scratch.
    return false;
  }

  UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexRestoreCacheTime",
                      base::TimeTicks::Now() - beginning_time);
  UMA_HISTOGRAM_COUNTS("History.InMemoryURLHistoryItems",
                       private_data_->history_id_word_map_.size());
  UMA_HISTOGRAM_COUNTS("History.InMemoryURLCacheSize", data.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLWords",
                             private_data_->word_map_.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLChars",
                             private_data_->char_word_map_.size());
  return true;
}

bool InMemoryURLIndex::SaveToCacheFile() {
  // TODO(mrossetti): Move File IO to another thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  FilePath file_path;
  if (!GetCacheFilePath(&file_path))
    return false;

  base::TimeTicks beginning_time = base::TimeTicks::Now();
  InMemoryURLIndexCacheItem index_cache;
  SavePrivateData(&index_cache);
  std::string data;
  if (!index_cache.SerializeToString(&data)) {
    LOG(WARNING) << "Failed to serialize the InMemoryURLIndex cache.";
    return false;
  }

  int size = data.size();
  if (file_util::WriteFile(file_path, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << file_path.value();
    return false;
  }
  UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexSaveCacheTime",
                      base::TimeTicks::Now() - beginning_time);
  return true;
}

bool InMemoryURLIndex::GetCacheFilePath(FilePath* file_path) {
  if (history_dir_.empty())
    return false;
  *file_path = history_dir_.Append(FILE_PATH_LITERAL("History Provider Cache"));
  return true;
}

// Cache Management ------------------------------------------------------------

void InMemoryURLIndex::SavePrivateData(InMemoryURLIndexCacheItem* cache) const {
  DCHECK(cache);
  cache->set_timestamp(base::Time::Now().ToInternalValue());
  // history_item_count_ is no longer used but rather than change the protobuf
  // definition use a placeholder. This will go away with the switch to SQLite.
  cache->set_history_item_count(0);
  SaveWordList(cache);
  SaveWordMap(cache);
  SaveCharWordMap(cache);
  SaveWordIDHistoryMap(cache);
  SaveHistoryInfoMap(cache);
}

bool InMemoryURLIndex::RestorePrivateData(
    const InMemoryURLIndexCacheItem& cache) {
  last_saved_ = base::Time::FromInternalValue(cache.timestamp());
  return RestoreWordList(cache) && RestoreWordMap(cache) &&
      RestoreCharWordMap(cache) && RestoreWordIDHistoryMap(cache) &&
      RestoreHistoryInfoMap(cache);
}

void InMemoryURLIndex::SaveWordList(InMemoryURLIndexCacheItem* cache) const {
  if (private_data_->word_list_.empty())
    return;
  WordListItem* list_item = cache->mutable_word_list();
  list_item->set_word_count(private_data_->word_list_.size());
  for (String16Vector::const_iterator iter = private_data_->word_list_.begin();
       iter != private_data_->word_list_.end(); ++iter)
    list_item->add_word(UTF16ToUTF8(*iter));
}

bool InMemoryURLIndex::RestoreWordList(const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_word_list())
    return false;
  const WordListItem& list_item(cache.word_list());
  uint32 expected_item_count = list_item.word_count();
  uint32 actual_item_count = list_item.word_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<std::string>& words(list_item.word());
  for (RepeatedPtrField<std::string>::const_iterator iter = words.begin();
       iter != words.end(); ++iter)
    private_data_->word_list_.push_back(UTF8ToUTF16(*iter));
  return true;
}

void InMemoryURLIndex::SaveWordMap(InMemoryURLIndexCacheItem* cache) const {
  if (private_data_->word_map_.empty())
    return;
  WordMapItem* map_item = cache->mutable_word_map();
  map_item->set_item_count(private_data_->word_map_.size());
  for (WordMap::const_iterator iter = private_data_->word_map_.begin();
       iter != private_data_->word_map_.end(); ++iter) {
    WordMapEntry* map_entry = map_item->add_word_map_entry();
    map_entry->set_word(UTF16ToUTF8(iter->first));
    map_entry->set_word_id(iter->second);
  }
}

bool InMemoryURLIndex::RestoreWordMap(const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_word_map())
    return false;
  const WordMapItem& list_item(cache.word_map());
  uint32 expected_item_count = list_item.item_count();
  uint32 actual_item_count = list_item.word_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<WordMapEntry>& entries(list_item.word_map_entry());
  for (RepeatedPtrField<WordMapEntry>::const_iterator iter = entries.begin();
       iter != entries.end(); ++iter)
    private_data_->word_map_[UTF8ToUTF16(iter->word())] = iter->word_id();
  return true;
}

void InMemoryURLIndex::SaveCharWordMap(InMemoryURLIndexCacheItem* cache) const {
  if (private_data_->char_word_map_.empty())
    return;
  CharWordMapItem* map_item = cache->mutable_char_word_map();
  map_item->set_item_count(private_data_->char_word_map_.size());
  for (CharWordIDMap::const_iterator iter =
          private_data_->char_word_map_.begin();
       iter != private_data_->char_word_map_.end(); ++iter) {
    CharWordMapEntry* map_entry = map_item->add_char_word_map_entry();
    map_entry->set_char_16(iter->first);
    const WordIDSet& word_id_set(iter->second);
    map_entry->set_item_count(word_id_set.size());
    for (WordIDSet::const_iterator set_iter = word_id_set.begin();
         set_iter != word_id_set.end(); ++set_iter)
      map_entry->add_word_id(*set_iter);
  }
}

bool InMemoryURLIndex::RestoreCharWordMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_char_word_map())
    return false;
  const CharWordMapItem& list_item(cache.char_word_map());
  uint32 expected_item_count = list_item.item_count();
  uint32 actual_item_count = list_item.char_word_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<CharWordMapEntry>&
      entries(list_item.char_word_map_entry());
  for (RepeatedPtrField<CharWordMapEntry>::const_iterator iter =
       entries.begin(); iter != entries.end(); ++iter) {
    expected_item_count = iter->item_count();
    actual_item_count = iter->word_id_size();
    if (actual_item_count == 0 || actual_item_count != expected_item_count)
      return false;
    char16 uni_char = static_cast<char16>(iter->char_16());
    WordIDSet word_id_set;
    const RepeatedField<int32>& word_ids(iter->word_id());
    for (RepeatedField<int32>::const_iterator jiter = word_ids.begin();
         jiter != word_ids.end(); ++jiter)
      word_id_set.insert(*jiter);
    private_data_->char_word_map_[uni_char] = word_id_set;
  }
  return true;
}

void InMemoryURLIndex::SaveWordIDHistoryMap(InMemoryURLIndexCacheItem* cache)
    const {
  if (private_data_->word_id_history_map_.empty())
    return;
  WordIDHistoryMapItem* map_item = cache->mutable_word_id_history_map();
  map_item->set_item_count(private_data_->word_id_history_map_.size());
  for (WordIDHistoryMap::const_iterator iter =
           private_data_->word_id_history_map_.begin();
       iter != private_data_->word_id_history_map_.end(); ++iter) {
    WordIDHistoryMapEntry* map_entry =
        map_item->add_word_id_history_map_entry();
    map_entry->set_word_id(iter->first);
    const HistoryIDSet& history_id_set(iter->second);
    map_entry->set_item_count(history_id_set.size());
    for (HistoryIDSet::const_iterator set_iter = history_id_set.begin();
         set_iter != history_id_set.end(); ++set_iter)
      map_entry->add_history_id(*set_iter);
  }
}

bool InMemoryURLIndex::RestoreWordIDHistoryMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_word_id_history_map())
    return false;
  const WordIDHistoryMapItem& list_item(cache.word_id_history_map());
  uint32 expected_item_count = list_item.item_count();
  uint32 actual_item_count = list_item.word_id_history_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<WordIDHistoryMapEntry>&
      entries(list_item.word_id_history_map_entry());
  for (RepeatedPtrField<WordIDHistoryMapEntry>::const_iterator iter =
       entries.begin(); iter != entries.end(); ++iter) {
    expected_item_count = iter->item_count();
    actual_item_count = iter->history_id_size();
    if (actual_item_count == 0 || actual_item_count != expected_item_count)
      return false;
    WordID word_id = iter->word_id();
    HistoryIDSet history_id_set;
    const RepeatedField<int64>& history_ids(iter->history_id());
    for (RepeatedField<int64>::const_iterator jiter = history_ids.begin();
         jiter != history_ids.end(); ++jiter) {
      history_id_set.insert(*jiter);
      private_data_->AddToHistoryIDWordMap(*jiter, word_id);
    }
    private_data_->word_id_history_map_[word_id] = history_id_set;
  }
  return true;
}

void InMemoryURLIndex::SaveHistoryInfoMap(
    InMemoryURLIndexCacheItem* cache) const {
  if (private_data_->history_info_map_.empty())
    return;
  HistoryInfoMapItem* map_item = cache->mutable_history_info_map();
  map_item->set_item_count(private_data_->history_info_map_.size());
  for (HistoryInfoMap::const_iterator iter =
           private_data_->history_info_map_.begin();
       iter != private_data_->history_info_map_.end(); ++iter) {
    HistoryInfoMapEntry* map_entry = map_item->add_history_info_map_entry();
    map_entry->set_history_id(iter->first);
    const URLRow& url_row(iter->second);
    // Note: We only save information that contributes to the index so there
    // is no need to save search_term_cache_ (not persistent),
    // languages_, etc.
    map_entry->set_visit_count(url_row.visit_count());
    map_entry->set_typed_count(url_row.typed_count());
    map_entry->set_last_visit(url_row.last_visit().ToInternalValue());
    map_entry->set_url(url_row.url().spec());
    map_entry->set_title(UTF16ToUTF8(url_row.title()));
  }
}

bool InMemoryURLIndex::RestoreHistoryInfoMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_history_info_map())
    return false;
  const HistoryInfoMapItem& list_item(cache.history_info_map());
  uint32 expected_item_count = list_item.item_count();
  uint32 actual_item_count = list_item.history_info_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<HistoryInfoMapEntry>&
      entries(list_item.history_info_map_entry());
  for (RepeatedPtrField<HistoryInfoMapEntry>::const_iterator iter =
       entries.begin(); iter != entries.end(); ++iter) {
    HistoryID history_id = iter->history_id();
    GURL url(iter->url());
    URLRow url_row(url, history_id);
    url_row.set_visit_count(iter->visit_count());
    url_row.set_typed_count(iter->typed_count());
    url_row.set_last_visit(base::Time::FromInternalValue(iter->last_visit()));
    if (iter->has_title()) {
      string16 title(UTF8ToUTF16(iter->title()));
      url_row.set_title(title);
    }
    private_data_->history_info_map_[history_id] = url_row;
  }
  return true;
}

}  // namespace history
