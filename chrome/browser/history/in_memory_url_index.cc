// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/history/url_database.h"

using in_memory_url_index::InMemoryURLIndexCacheItem;

namespace history {

InMemoryURLIndex::InMemoryURLIndex(const FilePath& history_dir)
    : history_dir_(history_dir),
      private_data_(new URLIndexPrivateData),
      cached_at_shutdown_(false) {
}

// Called only by unit tests.
InMemoryURLIndex::InMemoryURLIndex()
    : private_data_(new URLIndexPrivateData),
      cached_at_shutdown_(false) {
}

InMemoryURLIndex::~InMemoryURLIndex() {
  // If there was a history directory (which there won't be for some unit tests)
  // then insure that the cache has already been saved.
  DCHECK(history_dir_.empty() || cached_at_shutdown_);
}

bool InMemoryURLIndex::Init(URLDatabase* history_db,
                            const std::string& languages) {
  // TODO(mrossetti): Register for profile/language change notifications.
  private_data_->set_languages(languages);
  FilePath cache_file_path;
  return (GetCacheFilePath(&cache_file_path) &&
          private_data_->RestoreFromFile(cache_file_path)) ||
      ReloadFromHistory(history_db);
}

bool InMemoryURLIndex::ReloadFromHistory(history::URLDatabase* history_db) {
  if (!private_data_->ReloadFromHistory(history_db))
    return false;
  FilePath cache_file_path;
  if (GetCacheFilePath(&cache_file_path))
    private_data_->SaveToFile(cache_file_path);
  return true;
}

void InMemoryURLIndex::ShutDown() {
  // Write our cache.
  FilePath cache_file_path;
  if (!GetCacheFilePath(&cache_file_path))
    return;
  private_data_->SaveToFile(cache_file_path);
  cached_at_shutdown_ = true;
}

void InMemoryURLIndex::ClearPrivateData() {
  private_data_->Clear();
}

bool InMemoryURLIndex::GetCacheFilePath(FilePath* file_path) {
  if (history_dir_.empty())
    return false;
  *file_path = history_dir_.Append(FILE_PATH_LITERAL("History Provider Cache"));
  return true;
}

// Querying and Updating -------------------------------------------------------

ScoredHistoryMatches InMemoryURLIndex::HistoryItemsForTerms(
    const string16& term_string) {
  return private_data_->HistoryItemsForTerms(term_string);
}

void InMemoryURLIndex::UpdateURL(URLID row_id, const URLRow& row) {
  private_data_->UpdateURL(row_id, row);
}

void InMemoryURLIndex::DeleteURL(URLID row_id) {
  private_data_->DeleteURL(row_id);
}

}  // namespace history
