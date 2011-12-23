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

using in_memory_url_index::InMemoryURLIndexCacheItem;

namespace history {

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

}  // namespace history
