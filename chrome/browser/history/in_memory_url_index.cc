// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index.h"

#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/url_index_private_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/url_database.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

using in_memory_url_index::InMemoryURLIndexCacheItem;

namespace history {

// Called by DoSaveToCacheFile to delete any old cache file at |path| when
// there is no private data to save. Runs on the FILE thread.
void DeleteCacheFile(const base::FilePath& path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::DeleteFile(path, false);
}

// Initializes a whitelist of URL schemes.
void InitializeSchemeWhitelist(std::set<std::string>* whitelist) {
  DCHECK(whitelist);
  if (!whitelist->empty())
    return;  // Nothing to do, already initialized.
  whitelist->insert(std::string(url::kAboutScheme));
  whitelist->insert(std::string(content::kChromeUIScheme));
  whitelist->insert(std::string(url::kFileScheme));
  whitelist->insert(std::string(url::kFtpScheme));
  whitelist->insert(std::string(url::kHttpScheme));
  whitelist->insert(std::string(url::kHttpsScheme));
  whitelist->insert(std::string(url::kMailToScheme));
}

// Restore/SaveCacheObserver ---------------------------------------------------

InMemoryURLIndex::RestoreCacheObserver::~RestoreCacheObserver() {}

InMemoryURLIndex::SaveCacheObserver::~SaveCacheObserver() {}

// RebuildPrivateDataFromHistoryDBTask -----------------------------------------

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    RebuildPrivateDataFromHistoryDBTask(
        InMemoryURLIndex* index,
        const std::string& languages,
        const std::set<std::string>& scheme_whitelist)
    : index_(index),
      languages_(languages),
      scheme_whitelist_(scheme_whitelist),
      succeeded_(false) {
}

bool InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::RunOnDBThread(
    HistoryBackend* backend,
    HistoryDatabase* db) {
  data_ = URLIndexPrivateData::RebuildFromHistory(db, languages_,
                                                  scheme_whitelist_);
  succeeded_ = data_.get() && !data_->Empty();
  if (!succeeded_ && data_.get())
    data_->Clear();
  return true;
}

void InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    DoneRunOnMainThread() {
  index_->DoneRebuidingPrivateDataFromHistoryDB(succeeded_, data_);
}

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    ~RebuildPrivateDataFromHistoryDBTask() {
}

// InMemoryURLIndex ------------------------------------------------------------

InMemoryURLIndex::InMemoryURLIndex(Profile* profile,
                                   const base::FilePath& history_dir,
                                   const std::string& languages,
                                   HistoryClient* history_client)
    : profile_(profile),
      history_client_(history_client),
      history_dir_(history_dir),
      languages_(languages),
      private_data_(new URLIndexPrivateData),
      restore_cache_observer_(NULL),
      save_cache_observer_(NULL),
      shutdown_(false),
      restored_(false),
      needs_to_be_cached_(false) {
  InitializeSchemeWhitelist(&scheme_whitelist_);
  if (profile) {
    // TODO(mrossetti): Register for language change notifications.
    content::Source<Profile> source(profile);
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URL_VISITED, source);
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
                   source);
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED, source);
  }
}

// Called only by unit tests.
InMemoryURLIndex::InMemoryURLIndex()
    : profile_(NULL),
      history_client_(NULL),
      private_data_(new URLIndexPrivateData),
      restore_cache_observer_(NULL),
      save_cache_observer_(NULL),
      shutdown_(false),
      restored_(false),
      needs_to_be_cached_(false) {
  InitializeSchemeWhitelist(&scheme_whitelist_);
}

InMemoryURLIndex::~InMemoryURLIndex() {
  // If there was a history directory (which there won't be for some unit tests)
  // then insure that the cache has already been saved.
  DCHECK(history_dir_.empty() || !needs_to_be_cached_);
}

void InMemoryURLIndex::Init() {
  PostRestoreFromCacheFileTask();
}

void InMemoryURLIndex::ShutDown() {
  registrar_.RemoveAll();
  cache_reader_tracker_.TryCancelAll();
  shutdown_ = true;
  base::FilePath path;
  if (!GetCacheFilePath(&path))
    return;
  private_data_tracker_.TryCancelAll();
  URLIndexPrivateData::WritePrivateDataToCacheFileTask(private_data_, path);
  needs_to_be_cached_ = false;
}

void InMemoryURLIndex::ClearPrivateData() {
  private_data_->Clear();
}

bool InMemoryURLIndex::GetCacheFilePath(base::FilePath* file_path) {
  if (history_dir_.empty())
    return false;
  *file_path = history_dir_.Append(FILE_PATH_LITERAL("History Provider Cache"));
  return true;
}

// Querying --------------------------------------------------------------------

ScoredHistoryMatches InMemoryURLIndex::HistoryItemsForTerms(
    const base::string16& term_string,
    size_t cursor_position,
    size_t max_matches) {
  return private_data_->HistoryItemsForTerms(
      term_string,
      cursor_position,
      max_matches,
      languages_,
      history_client_);
}

// Updating --------------------------------------------------------------------

void InMemoryURLIndex::DeleteURL(const GURL& url) {
  private_data_->DeleteURL(url);
}

void InMemoryURLIndex::Observe(int notification_type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (notification_type) {
    case chrome::NOTIFICATION_HISTORY_URL_VISITED:
      OnURLVisited(content::Details<URLVisitedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_URLS_MODIFIED:
      OnURLsModified(
          content::Details<history::URLsModifiedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_URLS_DELETED:
      OnURLsDeleted(
          content::Details<history::URLsDeletedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_LOADED:
      registrar_.Remove(this, chrome::NOTIFICATION_HISTORY_LOADED,
                        content::Source<Profile>(profile_));
      ScheduleRebuildFromHistory();
      break;
    default:
      // For simplicity, the unit tests send us all notifications, even when
      // we haven't registered for them, so don't assert here.
      break;
  }
}

void InMemoryURLIndex::OnURLVisited(const URLVisitedDetails* details) {
  HistoryService* service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           Profile::EXPLICIT_ACCESS);
  needs_to_be_cached_ |= private_data_->UpdateURL(service,
                                                  details->row,
                                                  languages_,
                                                  scheme_whitelist_,
                                                  &private_data_tracker_);
}

void InMemoryURLIndex::OnURLsModified(const URLsModifiedDetails* details) {
  HistoryService* service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           Profile::EXPLICIT_ACCESS);
  for (URLRows::const_iterator row = details->changed_urls.begin();
       row != details->changed_urls.end();
       ++row) {
    needs_to_be_cached_ |= private_data_->UpdateURL(
        service, *row, languages_, scheme_whitelist_, &private_data_tracker_);
  }
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
  // If we made changes, destroy the previous cache.  Otherwise, if we go
  // through an unclean shutdown (and therefore fail to write a new cache file),
  // when Chrome restarts and we restore from the previous cache, we'll end up
  // searching over URLs that may be deleted.  This would be wrong, and
  // surprising to the user who bothered to delete some URLs from his/her
  // history.  In this situation, deleting the cache is a better solution than
  // writing a new cache (after deleting the URLs from the in-memory structure)
  // because deleting the cache forces it to be rebuilt from history upon
  // startup.  If we instead write a new, updated cache then at the time of next
  // startup (after an unclean shutdown) we will not rebuild the in-memory data
  // structures from history but rather use the cache.  This solution is
  // mediocre because this cache may not have the most-recently-visited URLs
  // in it (URLs visited after user deleted some URLs from history), which
  // would be odd and confusing.  It's better to force a rebuild.
  base::FilePath path;
  if (needs_to_be_cached_ && GetCacheFilePath(&path)) {
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE, base::Bind(DeleteCacheFile, path));
  }
}

// Restoring from Cache --------------------------------------------------------

void InMemoryURLIndex::PostRestoreFromCacheFileTask() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  TRACE_EVENT0("browser", "InMemoryURLIndex::PostRestoreFromCacheFileTask");

  base::FilePath path;
  if (!GetCacheFilePath(&path) || shutdown_) {
    restored_ = true;
    if (restore_cache_observer_)
      restore_cache_observer_->OnCacheRestoreFinished(false);
    return;
  }

  content::BrowserThread::PostTaskAndReplyWithResult
      <scoped_refptr<URLIndexPrivateData> >(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&URLIndexPrivateData::RestoreFromFile, path, languages_),
      base::Bind(&InMemoryURLIndex::OnCacheLoadDone, AsWeakPtr()));
}

void InMemoryURLIndex::OnCacheLoadDone(
    scoped_refptr<URLIndexPrivateData> private_data) {
  if (private_data.get() && !private_data->Empty()) {
    private_data_tracker_.TryCancelAll();
    private_data_ = private_data;
    restored_ = true;
    if (restore_cache_observer_)
      restore_cache_observer_->OnCacheRestoreFinished(true);
  } else if (profile_) {
    // When unable to restore from the cache file delete the cache file, if
    // it exists, and then rebuild from the history database if it's available,
    // otherwise wait until the history database loaded and then rebuild.
    base::FilePath path;
    if (!GetCacheFilePath(&path) || shutdown_)
      return;
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE, base::Bind(DeleteCacheFile, path));
    HistoryService* service =
        HistoryServiceFactory::GetForProfileWithoutCreating(profile_);
    if (service && service->backend_loaded()) {
      ScheduleRebuildFromHistory();
    } else {
      registrar_.Add(this, chrome::NOTIFICATION_HISTORY_LOADED,
                     content::Source<Profile>(profile_));
    }
  }
}

// Restoring from the History DB -----------------------------------------------

void InMemoryURLIndex::ScheduleRebuildFromHistory() {
  HistoryService* service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           Profile::EXPLICIT_ACCESS);
  service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask(
              this, languages_, scheme_whitelist_)),
      &cache_reader_tracker_);
}

void InMemoryURLIndex::DoneRebuidingPrivateDataFromHistoryDB(
    bool succeeded,
    scoped_refptr<URLIndexPrivateData> private_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (succeeded) {
    private_data_tracker_.TryCancelAll();
    private_data_ = private_data;
    PostSaveToCacheFileTask();  // Cache the newly rebuilt index.
  } else {
    private_data_->Clear();  // Dump the old private data.
    // There is no need to do anything with the cache file as it was deleted
    // when the rebuild from the history operation was kicked off.
  }
  restored_ = true;
  if (restore_cache_observer_)
    restore_cache_observer_->OnCacheRestoreFinished(succeeded);
}

void InMemoryURLIndex::RebuildFromHistory(HistoryDatabase* history_db) {
  private_data_tracker_.TryCancelAll();
  private_data_ = URLIndexPrivateData::RebuildFromHistory(history_db,
                                                          languages_,
                                                          scheme_whitelist_);
}

// Saving to Cache -------------------------------------------------------------

void InMemoryURLIndex::PostSaveToCacheFileTask() {
  base::FilePath path;
  if (!GetCacheFilePath(&path))
    return;
  // If there is anything in our private data then make a copy of it and tell
  // it to save itself to a file.
  if (private_data_.get() && !private_data_->Empty()) {
    // Note that ownership of the copy of our private data is passed to the
    // completion closure below.
    scoped_refptr<URLIndexPrivateData> private_data_copy =
        private_data_->Duplicate();
    content::BrowserThread::PostTaskAndReplyWithResult<bool>(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&URLIndexPrivateData::WritePrivateDataToCacheFileTask,
                   private_data_copy, path),
        base::Bind(&InMemoryURLIndex::OnCacheSaveDone, AsWeakPtr()));
  } else {
    // If there is no data in our index then delete any existing cache file.
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(DeleteCacheFile, path));
  }
}

void InMemoryURLIndex::OnCacheSaveDone(bool succeeded) {
  if (save_cache_observer_)
    save_cache_observer_->OnCacheSaveFinished(succeeded);
}

}  // namespace history
