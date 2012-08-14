// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index.h"

#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/in_memory_url_cache_database.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/url_index_private_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace history {

// InMemoryURLIndex::Observer --------------------------------------------------

InMemoryURLIndex::Observer::Observer(InMemoryURLIndex* index)
    : index_(index) {
  DCHECK(index);
  index_->AddObserver(this);
}

InMemoryURLIndex::Observer::~Observer() {
  index_->RemoveObserver(this);
}

void InMemoryURLIndex::Observer::Loaded() {
  MessageLoop::current()->QuitNow();
}

// RebuildPrivateDataFromHistoryDBTask -----------------------------------------

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    RebuildPrivateDataFromHistoryDBTask(InMemoryURLIndex* index)
    : index_(index),
      succeeded_(false) {
}

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    ~RebuildPrivateDataFromHistoryDBTask() {
}

bool InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::RunOnDBThread(
    HistoryBackend* backend,
    HistoryDatabase* db) {
  data_ = URLIndexPrivateData::RebuildFromHistory(db, index_->private_data());
  succeeded_ = data_.get() && !data_->Empty();
  return true;
}

void InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    DoneRunOnMainThread() {
  index_->DoneRebuidingPrivateDataFromHistoryDB(succeeded_, data_);
}

// IndexUpdateItem -------------------------------------------------------------

InMemoryURLIndex::IndexUpdateItem::IndexUpdateItem(UpdateType update_type,
                                                   URLRow row)
    : update_type(update_type),
      row(row) {
}

InMemoryURLIndex::IndexUpdateItem::~IndexUpdateItem() {}

// InMemoryURLIndex ------------------------------------------------------------

InMemoryURLIndex::InMemoryURLIndex(Profile* profile,
                                   const FilePath& history_dir,
                                   const std::string& languages)
    : profile_(profile),
      languages_(languages),
      history_dir_(history_dir),
      private_data_(new URLIndexPrivateData(history_dir, languages)),
      sequence_token_(BrowserThread::GetBlockingPool()->GetSequenceToken()),
      index_available_(false),
      shutdown_(false) {
  if (profile) {
    content::Source<Profile> source(profile);
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URL_VISITED, source);
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
                   source);
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED, source);
  }
  // Note: private_data_ will be reset after rebuilding from the history
  // database but the ownership of the database passes to the new
  // private_data_ instance so there is no need to re-register for the
  // following notification at that time.
  registrar_.Add(this,
                 chrome::NOTIFICATION_IN_MEMORY_URL_CACHE_DATABASE_FAILURE,
                 content::Source<InMemoryURLCacheDatabase>(
                     private_data_->cache_db()));
  // TODO(mrossetti): Register for language change notifications.
}

// Called only by unit tests.
InMemoryURLIndex::InMemoryURLIndex(const FilePath& history_dir,
                                   const std::string& languages)
    : profile_(NULL),
      languages_(languages),
      history_dir_(history_dir),
      index_available_(false),
      shutdown_(false) {
}

InMemoryURLIndex::~InMemoryURLIndex() {}

void InMemoryURLIndex::Init(bool disable_cache) {
  if (disable_cache) {
    RebuildFromHistoryIfLoaded();
  } else {
    // It's safe to initialize the private data and the cache database without
    // using the sequenced worker pool as no other database operations will be
    // going on at the same time.
    BrowserThread::PostTaskAndReplyWithResult<bool>(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&URLIndexPrivateData::Init, private_data_, sequence_token_),
        base::Bind(&InMemoryURLIndex::OnPrivateDataInitDone, AsWeakPtr()));
  }
}

void InMemoryURLIndex::OnPrivateDataInitDone(bool succeeded) {
  if (shutdown_)
    return;
  if (succeeded)
    PostRestoreFromCacheTask();
  else
    RebuildFromHistoryIfLoaded();
}

void InMemoryURLIndex::Shutdown() {
  // Close down the cache database as quickly as possible. Any pending cache DB
  // transactions will detect that the database is no longer there and give up.
  registrar_.RemoveAll();
  cache_reader_consumer_.CancelAllRequests();
  shutdown_ = true;
  private_data_->Shutdown();
}

void InMemoryURLIndex::AddObserver(InMemoryURLIndex::Observer* observer) {
  observers_.AddObserver(observer);
}

void InMemoryURLIndex::RemoveObserver(InMemoryURLIndex::Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Querying --------------------------------------------------------------------

ScoredHistoryMatches InMemoryURLIndex::HistoryItemsForTerms(
    const string16& term_string) {
  return private_data_->HistoryItemsForTerms(term_string);
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
    case chrome::NOTIFICATION_IN_MEMORY_URL_CACHE_DATABASE_FAILURE:
      RepairCacheDatabase();
      break;
    default:
      // For simplicity, the unit tests send us all notifications, even when
      // we haven't registered for them, so don't assert here.
      break;
  }
}

void InMemoryURLIndex::OnURLVisited(const URLVisitedDetails* details) {
  if (index_available_)
    private_data_->UpdateURL(details->row);
  else
    pending_updates_.push_back(IndexUpdateItem(UPDATE_VISIT, details->row));
}

void InMemoryURLIndex::OnURLsModified(const URLsModifiedDetails* details) {
  for (URLRows::const_iterator row = details->changed_urls.begin();
       row != details->changed_urls.end(); ++row) {
    if (index_available_)
      private_data_->UpdateURL(*row);
    else
      pending_updates_.push_back(IndexUpdateItem(UPDATE_VISIT, *row));
  }
}

void InMemoryURLIndex::OnURLsDeleted(const URLsDeletedDetails* details) {
  if (details->all_history) {
    PostResetPrivateDataTask();
  } else {
    for (URLRows::const_iterator row = details->rows.begin();
         row != details->rows.end(); ++row) {
      if (index_available_)
        DeleteURL(row->url());
      else
        pending_updates_.push_back(IndexUpdateItem(DELETE_VISIT, *row));
    }
  }
}

void InMemoryURLIndex::FlushPendingUpdates() {
  for (PendingUpdates::iterator i = pending_updates_.begin();
       i != pending_updates_.end(); ++i) {
    if (i->update_type == UPDATE_VISIT)
      private_data_->UpdateURL(i->row);
    else if (i->update_type == DELETE_VISIT)
      DeleteURL(i->row.url());
  }
  pending_updates_.clear();
}

// Restoring from Cache --------------------------------------------------------

void InMemoryURLIndex::PostRestoreFromCacheTask() {
  // It's safe to restore from the cache database without using the sequenced
  // worker pool as no other database operations will be going on at the same
  // time.
  BrowserThread::PostTaskAndReplyWithResult<bool>(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&URLIndexPrivateData::RestoreFromCacheTask, private_data_),
      base::Bind(&InMemoryURLIndex::OnCacheRestoreDone, AsWeakPtr()));
}

void InMemoryURLIndex::OnCacheRestoreDone(bool succeeded) {
  if (shutdown_) {
    NotifyHasLoaded();
    return;
  }
  if (succeeded) {
    FlushPendingUpdates();
    index_available_ = true;
    NotifyHasLoaded();
  } else if (profile_) {
    RebuildFromHistoryIfLoaded();
  }
}

void InMemoryURLIndex::NotifyHasLoaded() {
  FOR_EACH_OBSERVER(InMemoryURLIndex::Observer, observers_, Loaded());
}

// Restoring from the History DB -----------------------------------------------

void InMemoryURLIndex::RebuildFromHistoryIfLoaded() {
  // When unable to restore from the cache database, rebuild from the history
  // database, if it's available, otherwise wait until the history database
  // has loaded and then rebuild the index.
  HistoryService* service =
      HistoryServiceFactory::GetForProfileIfExists(profile_,
                                                   Profile::EXPLICIT_ACCESS);
  if (service && service->backend_loaded()) {
    ScheduleRebuildFromHistory();
  } else {
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_LOADED,
                   content::Source<Profile>(profile_));
  }
}

void InMemoryURLIndex::ScheduleRebuildFromHistory() {
  // It's possible that we were waiting on history to finish loading when
  // the profile was told to shut down.
  if (shutdown_)
    return;
  // Reset availability here as this function is called directly by unit tests.
  index_available_ = false;
  HistoryService* service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           Profile::EXPLICIT_ACCESS);
  // Do not update the cache database while rebuilding.
  private_data_->set_cache_enabled(false);
  service->ScheduleDBTask(
      new InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask(this),
      &cache_reader_consumer_);
}

void InMemoryURLIndex::DoneRebuidingPrivateDataFromHistoryDB(
    bool succeeded,
    scoped_refptr<URLIndexPrivateData> private_data) {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (shutdown_)
    return;
  if (succeeded) {
    private_data_ = private_data;
    private_data_->set_cache_enabled(true);
    PostRefreshCacheTask();  // Cache the newly rebuilt index.
  } else {
    private_data_->set_cache_enabled(true);
    PostResetPrivateDataTask();
  }
}

// Reset Cache -----------------------------------------------------------------

void InMemoryURLIndex::PostResetPrivateDataTask() {
  index_available_ = false;
  scoped_refptr<base::SequencedTaskRunner> runner =
      BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(sequence_token_);
  DCHECK(runner.get());
  runner->PostTaskAndReply(FROM_HERE,
      base::Bind(&URLIndexPrivateData::Reset, private_data_),
      base::Bind(&InMemoryURLIndex::OnCacheRefreshOrResetDone, AsWeakPtr()));
}

// Refresh Cache ---------------------------------------------------------------

void InMemoryURLIndex::PostRefreshCacheTask() {
  scoped_refptr<base::SequencedTaskRunner> runner =
      BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(sequence_token_);
  DCHECK(runner.get());
  runner->PostTaskAndReply(FROM_HERE,
      base::Bind(&URLIndexPrivateData::RefreshCacheTask, private_data_),
      base::Bind(&InMemoryURLIndex::OnCacheRefreshOrResetDone, AsWeakPtr()));
}

void InMemoryURLIndex::OnCacheRefreshOrResetDone() {
  if (shutdown_) {
    NotifyHasLoaded();
    return;
  }
  FlushPendingUpdates();
  index_available_ = true;
  NotifyHasLoaded();
}

// Repair Cache ----------------------------------------------------------------

void InMemoryURLIndex::RepairCacheDatabase() {
  // The database will disable itself when it detects an error so re-enable the
  // database and try to refresh it from scratch. If that fails then the
  // database will be left in a disabled state and will be rebuilt from the
  // history database the next time the profile is opened.
  private_data_->set_cache_enabled(true);
  PostRefreshCacheTask();  // Cache the newly rebuilt index.
}

}  // namespace history
