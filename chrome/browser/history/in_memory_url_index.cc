// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/url_index_private_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using in_memory_url_index::InMemoryURLIndexCacheItem;

namespace history {

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    RebuildPrivateDataFromHistoryDBTask(InMemoryURLIndex* index)
    : index_(index),
      succeeded_(false) {}

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    ~RebuildPrivateDataFromHistoryDBTask() {}

bool InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::RunOnDBThread(
    HistoryBackend* backend,
    HistoryDatabase* db) {
  data_.reset(URLIndexPrivateData::RebuildFromHistory(db));
  succeeded_ = data_.get() && !data_->history_info_map_.empty();
  if (!succeeded_)
    data_.reset();
  return true;
}

void InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    DoneRunOnMainThread() {
  if (succeeded_)
    index_->DoneRebuidingPrivateDataFromHistoryDB(data_.release());
}

InMemoryURLIndex::InMemoryURLIndex(Profile* profile,
                                   const FilePath& history_dir,
                                   const std::string& languages)
    : profile_(profile),
      history_dir_(history_dir),
      private_data_(new URLIndexPrivateData),
      shutdown_(false),
      needs_to_be_cached_(false) {
  private_data_->set_languages(languages);
  if (profile) {
    // TODO(mrossetti): Register for language change notifications.
    content::Source<Profile> source(profile);
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URL_VISITED, source);
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED,
                   source);
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED, source);
  }
}

// Called only by unit tests.
InMemoryURLIndex::InMemoryURLIndex()
    : profile_(NULL),
      private_data_(new URLIndexPrivateData),
      shutdown_(false),
      needs_to_be_cached_(false) {
}

InMemoryURLIndex::~InMemoryURLIndex() {
  // If there was a history directory (which there won't be for some unit tests)
  // then insure that the cache has already been saved.
  DCHECK(history_dir_.empty() || !needs_to_be_cached_);
}

void InMemoryURLIndex::Init() {
  RestoreFromCacheFile();
}

void InMemoryURLIndex::ShutDown() {
  registrar_.RemoveAll();
  cache_reader_consumer_.CancelAllRequests();
  shutdown_ = true;
  SaveToCacheFile();
  needs_to_be_cached_ = false;
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

// Querying --------------------------------------------------------------------

ScoredHistoryMatches InMemoryURLIndex::HistoryItemsForTerms(
    const string16& term_string) {
  return private_data_->HistoryItemsForTerms(term_string);
}

// Updating --------------------------------------------------------------------

void InMemoryURLIndex::Observe(int notification_type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (notification_type) {
    case chrome::NOTIFICATION_HISTORY_URL_VISITED:
      OnURLVisited(content::Details<URLVisitedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED:
      OnURLsModified(
          content::Details<history::URLsModifiedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_URLS_DELETED:
      OnURLsDeleted(
          content::Details<history::URLsDeletedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_LOADED: {
      registrar_.Remove(this, chrome::NOTIFICATION_HISTORY_LOADED,
                        content::Source<Profile>(profile_));
      ScheduleRebuildFromHistory();
      break;
    }
    default:
      // For simplicity, the unit tests send us all notifications, even when
      // we haven't registered for them, so don't assert here.
      break;
  }
}

void InMemoryURLIndex::OnURLVisited(const URLVisitedDetails* details) {
  needs_to_be_cached_ |= private_data_->UpdateURL(details->row);
}

void InMemoryURLIndex::OnURLsModified(const URLsModifiedDetails* details) {
  for (URLRows::const_iterator row = details->changed_urls.begin();
       row != details->changed_urls.end(); ++row)
    needs_to_be_cached_ |= private_data_->UpdateURL(*row);
}

void InMemoryURLIndex::OnURLsDeleted(const URLsDeletedDetails* details) {
  if (details->all_history) {
    ClearPrivateData();
    needs_to_be_cached_ = true;
  } else {
    for (URLRows::const_iterator row = details->rows.begin();
         row != details->rows.end(); ++row)
      needs_to_be_cached_ |= private_data_->DeleteURL(row->url());
  }
}

// Restoring from Cache --------------------------------------------------------

void InMemoryURLIndex::RestoreFromCacheFile() {
  FilePath path;
  if (GetCacheFilePath(&path) && !shutdown_)
    DoRestoreFromCacheFile(path);
}

void InMemoryURLIndex::DoRestoreFromCacheFile(const FilePath& path) {
  if (private_data_->RestoreFromFile(path))
    return;

  // When unable to restore from the cache file we must rebuild from the
  // history database.
  HistoryService* service = profile_->GetHistoryServiceWithoutCreating();
  if (service && service->backend_loaded())
    ScheduleRebuildFromHistory();
  // We must wait to rebuild until the history backend has been loaded.
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_LOADED,
                 content::Source<Profile>(profile_));
}

// Restoring from the History DB -----------------------------------------------

void InMemoryURLIndex::ScheduleRebuildFromHistory() {
  HistoryService* service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  service->ScheduleDBTask(
      new InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask(this),
      &cache_reader_consumer_);
}

void InMemoryURLIndex::DoneRebuidingPrivateDataFromHistoryDB(
    URLIndexPrivateData* data) {
  scoped_ptr<URLIndexPrivateData> private_data(data);
  private_data_.swap(private_data);
  // Cache the newly rebuilt index.
  FilePath cache_file_path;
  if (GetCacheFilePath(&cache_file_path))
    private_data_->SaveToFile(cache_file_path);
}

void InMemoryURLIndex::RebuildFromHistory(HistoryDatabase* history_db) {
  private_data_.reset(URLIndexPrivateData::RebuildFromHistory(history_db));
}

// Saving to Cache -------------------------------------------------------------

void InMemoryURLIndex::SaveToCacheFile() {
  FilePath path;
  if (GetCacheFilePath(&path))
    DoSaveToCacheFile(path);
}

void InMemoryURLIndex::DoSaveToCacheFile(const FilePath& path) {
  private_data_->SaveToFile(path);
}

}  // namespace history
