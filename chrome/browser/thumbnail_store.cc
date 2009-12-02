// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail_store.h"

#include <string.h>
#include <algorithm>

#include "app/gfx/codec/jpeg_codec.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"


ThumbnailStore::ThumbnailStore()
    : cache_(NULL),
      hs_(NULL),
      url_blacklist_(NULL),
      disk_data_loaded_(false) {
}

ThumbnailStore::~ThumbnailStore() {
  // Ensure that shutdown was called.
  DCHECK(hs_ == NULL);
}

void ThumbnailStore::Init(const FilePath& db_name, Profile* profile) {
  // Load thumbnails already in the database.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ThumbnailStore::InitializeFromDB,
                        db_name, MessageLoop::current()));

  // Take ownership of a reference to the HistoryService.
  hs_ = profile->GetHistoryService(Profile::EXPLICIT_ACCESS);

  // Store a pointer to a persistent table of blacklisted URLs.
  url_blacklist_ = profile->GetPrefs()->
    GetMutableDictionary(prefs::kNTPMostVisitedURLsBlacklist);
  DCHECK(url_blacklist_);

  // Get the list of most visited URLs and redirect information from the
  // HistoryService.
  most_visited_urls_.reset(new MostVisitedMap);
  timer_.Start(base::TimeDelta::FromSeconds(kUpdateIntervalSecs), this,
      &ThumbnailStore::UpdateURLData);
  UpdateURLData();

  // Register to get notified when the history is cleared.
  registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
                 Source<Profile>(profile));
}

bool ThumbnailStore::SetPageThumbnail(const GURL& url,
                                      const SkBitmap& thumbnail,
                                      const ThumbnailScore& score,
                                      bool fetch_redirects) {
  if (!cache_.get())
    return false;

  if (!ShouldStoreThumbnailForURL(url) ||
      (cache_->find(url) != cache_->end() &&
      !ShouldReplaceThumbnailWith((*cache_)[url].score_, score)))
    return true;

  base::TimeTicks encode_start = base::TimeTicks::Now();

  // Encode the SkBitmap to jpeg.
  scoped_refptr<RefCountedBytes> jpeg_data = new RefCountedBytes;
  SkAutoLockPixels thumbnail_lock(thumbnail);
  bool encoded = gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(thumbnail.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_BGRA, thumbnail.width(),
      thumbnail.height(),
      static_cast<int>(thumbnail.rowBytes()), 90,
      &jpeg_data->data);

  base::TimeDelta delta = base::TimeTicks::Now() - encode_start;
  HISTOGRAM_TIMES("Thumbnail.Encode", delta);

  if (!encoded)
    return false;

  // Update the cache_ with the new thumbnail.
  (*cache_)[url] = CacheEntry(jpeg_data, score, true);

  // Get redirects for this URL.
  if (fetch_redirects) {
    hs_->QueryRedirectsTo(url, &consumer_,
        NewCallback(this, &ThumbnailStore::OnRedirectsForURLAvailable));
  }

  return true;
}

bool ThumbnailStore::GetPageThumbnail(
    const GURL& url,
    RefCountedBytes** data) {
  if (!cache_.get() || IsURLBlacklisted(url))
    return false;

  // Look up the |url| in the redirect list to find the final destination
  // which is the key into the |cache_|.
  history::RedirectMap::iterator it = redirect_urls_->find(url);
  if (it != redirect_urls_->end()) {
    // Return the first available thumbnail starting at the end of the
    // redirect list.
    history::RedirectList::reverse_iterator rit;
    for (rit = it->second->data.rbegin();
        rit != it->second->data.rend(); ++rit) {
      if (cache_->find(*rit) != cache_->end()) {
        *data = (*cache_)[*rit].data_.get();
        (*data)->AddRef();
        return true;
      }
    }
  }

  // TODO(meelapshah) bug 14643: check past redirect lists

  if (cache_->find(url) == cache_->end())
    return false;

  *data = (*cache_)[url].data_.get();
  (*data)->AddRef();
  return true;
}

void ThumbnailStore::Shutdown() {
  // We must release our reference to the HistoryService here to prevent
  // shutdown issues. Please refer to the comment in HistoryService::Cleanup
  // for details.
  hs_ = NULL;

  // De-register for notifications.
  registrar_.RemoveAll();

  // Stop the timer to ensure that UpdateURLData is not called during shutdown.
  timer_.Stop();

  // Write the cache to disk.  This will schedule the disk operations to be run
  // on the file_thread.  Note that Join() does not need to be called with the
  // file_thread because when the disk operation is scheduled, it will hold a
  // reference to |this| keeping this object alive.
  CleanCacheData();
}

void ThumbnailStore::OnRedirectsForURLAvailable(
    HistoryService::Handle handle,
    GURL url,
    bool success,
    history::RedirectList* redirects) {
  if (!success)
    return;

  DCHECK(redirect_urls_.get());

  // If A -> B -> C is a redirect chain, then this function would be called
  // with url=C and redirects = {B, A}. This is entered into the RedirectMap as
  // A => {B -> C}.
  if (redirects->empty()) {
    (*redirect_urls_)[url] = new RefCountedVector<GURL>;
  } else {
    const GURL start_url = redirects->back();
    std::reverse(redirects->begin(), redirects->end() - 1);
    *(redirects->end() - 1) = url;
    (*redirect_urls_)[start_url] = new RefCountedVector<GURL>(*redirects);
  }
}

history::RedirectMap::iterator ThumbnailStore::GetRedirectIteratorForURL(
    const GURL& url) const {
  for (history::RedirectMap::iterator it = redirect_urls_->begin();
      it != redirect_urls_->end(); ++it) {
    if (it->first == url ||
        (!it->second->data.empty() && it->second->data.back() == url))
      return it;
  }
  return redirect_urls_->end();
}

void ThumbnailStore::Observe(NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type.value != NotificationType::HISTORY_URLS_DELETED) {
    NOTREACHED();
    return;
  }

  Details<history::URLsDeletedDetails> url_details(details);
  // If all history was cleared, clear all of our data and reset the update
  // timer.
  if (url_details->all_history) {
    most_visited_urls_->clear();
    redirect_urls_->clear();
    cache_->clear();
    timer_.Reset();
  }
}

void ThumbnailStore::NotifyThumbnailStoreReady() {
  NotificationService::current()->Notify(
      NotificationType::THUMBNAIL_STORE_READY,
      Source<ThumbnailStore>(this),
      NotificationService::NoDetails());
}

void ThumbnailStore::UpdateURLData() {
  DCHECK(url_blacklist_);

  int result_count = ThumbnailStore::kMaxCacheSize + url_blacklist_->size();
  hs_->QueryTopURLsAndRedirects(result_count, &consumer_,
      NewCallback(this, &ThumbnailStore::OnURLDataAvailable));
}

void ThumbnailStore::OnURLDataAvailable(HistoryService::Handle handle,
                                        bool success,
                                        std::vector<GURL>* urls,
                                        history::RedirectMap* redirects) {
  if (!success)
    return;

  DCHECK(urls);
  DCHECK(redirects);

  // Each element of |urls| is the start of a redirect chain. When thumbnails
  // are stored from TabContents, the tails of the redirect chains are
  // associated with the image. Since SetPageThumbnail is called frequently, we
  // look up the tail end of each element in |urls| and insert that into the
  // MostVisitedMap. This way SetPageThumbnail can more quickly check if a
  // given url is in the most visited list.
  most_visited_urls_->clear();
  for (size_t i = 0; i < urls->size(); ++i) {
    history::RedirectMap::iterator it = redirects->find(urls->at(i));
    if (it->second->data.empty())
      (*most_visited_urls_)[urls->at(i)] = GURL();
    else
      (*most_visited_urls_)[it->second->data.back()] = urls->at(i);
  }
  redirect_urls_.reset(new history::RedirectMap(*redirects));

  if (IsReady())
    NotifyThumbnailStoreReady();

  CleanCacheData();
}

void ThumbnailStore::CleanCacheData() {
  if (!cache_.get())
    return;

  scoped_refptr<RefCountedVector<GURL> > urls_to_delete =
      new RefCountedVector<GURL>;
  Cache* data_to_save = new Cache;  // CommitCacheToDB will delete this

  // Iterate the cache, storing urls to be deleted and dirty cache entries to
  // be written to disk.
  for (Cache::iterator cache_it = cache_->begin();
       cache_it != cache_->end();) {
    history::RedirectMap::iterator redirect_it =
        GetRedirectIteratorForURL(cache_it->first);
    const GURL* url = redirect_it == redirect_urls_->end() ?
                          NULL : &redirect_it->first;

    // If this URL is blacklisted or not in the most visited list, mark it for
    // deletion. Otherwise, if the cache entry is dirty, mark it to be written
    // to disk.
    if (url == NULL || IsURLBlacklisted(*url) || !IsPopular(*url)) {
      urls_to_delete->data.push_back(cache_it->first);
      cache_->erase(cache_it++);
    } else {
      if (cache_it->second.dirty_) {
        data_to_save->insert(*cache_it);
        cache_it->second.dirty_ = false;
      }
      ++cache_it;
    }
  }

  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ThumbnailStore::CommitCacheToDB,
                        urls_to_delete, data_to_save));
}

void ThumbnailStore::CommitCacheToDB(
    scoped_refptr<RefCountedVector<GURL> > urls_to_delete,
    Cache* data) {
  scoped_ptr<Cache> data_to_save(data);
  if (!db_.is_open())
    return;

  base::TimeTicks db_start = base::TimeTicks::Now();

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  // Delete old thumbnails.
  if (urls_to_delete.get()) {
    for (std::vector<GURL>::iterator it = urls_to_delete->data.begin();
        it != urls_to_delete->data.end(); ++it) {
      sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
          "DELETE FROM thumbnails WHERE url=?"));
      if (!statement)
        return;
      statement.BindString(0, it->spec());
      if (!statement.Run())
        NOTREACHED();
    }
  }

  // Update cached thumbnails.
  if (data_to_save.get()) {
    for (Cache::iterator it = data_to_save->begin();
         it != data_to_save->end(); ++it) {
      sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
          "INSERT OR REPLACE INTO thumbnails "
          "(url, boring_score, good_clipping, "
          "at_top, time_taken, data) "
          "VALUES (?,?,?,?,?,?)"));
      statement.BindString(0, it->first.spec());
      statement.BindDouble(1, it->second.score_.boring_score);
      statement.BindBool(2, it->second.score_.good_clipping);
      statement.BindBool(3, it->second.score_.at_top);
      statement.BindInt64(4,
          it->second.score_.time_at_snapshot.ToInternalValue());
      statement.BindBlob(5, &it->second.data_->data[0],
                          static_cast<int>(it->second.data_->data.size()));
      if (!statement.Run())
        DLOG(WARNING) << "Unable to insert thumbnail for URL";
    }
  }

  transaction.Commit();

  base::TimeDelta delta = base::TimeTicks::Now() - db_start;
  HISTOGRAM_TIMES("ThumbnailStore.WriteDBToDisk", delta);
}

void ThumbnailStore::InitializeFromDB(const FilePath& db_name,
                                      MessageLoop* cb_loop) {
  db_.set_page_size(4096);
  db_.set_cache_size(64);
  db_.set_exclusive_locking();
  if (!db_.Open(db_name))
    return;

  if (!db_.DoesTableExist("thumbnails")) {
    if (!db_.Execute("CREATE TABLE thumbnails ("
          "url LONGVARCHAR PRIMARY KEY,"
          "boring_score DOUBLE DEFAULT 1.0,"
          "good_clipping INTEGER DEFAULT 0,"
          "at_top INTEGER DEFAULT 0,"
          "time_taken INTEGER DEFAULT 0,"
          "data BLOB)"))
      return;
  }

  if (cb_loop)
    GetAllThumbnailsFromDisk(cb_loop);
}

void ThumbnailStore::GetAllThumbnailsFromDisk(MessageLoop* cb_loop) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT * FROM thumbnails"));
  if (!statement)
    return;

  Cache* cache = new Cache;
  while (statement.Step()) {
    // The URL
    GURL url(statement.ColumnString(0));

    // The score.
    ThumbnailScore score(statement.ColumnDouble(1),      // Boring score
                         statement.ColumnBool(2),        // Good clipping
                         statement.ColumnBool(3),        // At top
                         base::Time::FromInternalValue(
                            statement.ColumnInt64(4)));  // Time taken

    // The image.
    scoped_refptr<RefCountedBytes> data = new RefCountedBytes;
    statement.ColumnBlobAsVector(5, &data->data);
    (*cache)[url] = CacheEntry(data, score, false);
  }

  cb_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ThumbnailStore::OnDiskDataAvailable, cache));
}

void ThumbnailStore::OnDiskDataAvailable(Cache* cache) {
  if (cache)
    cache_.reset(cache);

  disk_data_loaded_ = true;
  if (IsReady())
    NotifyThumbnailStoreReady();
}

bool ThumbnailStore::ShouldStoreThumbnailForURL(const GURL& url) const {
  if (!cache_.get())
    return false;

  if (IsURLBlacklisted(url) || cache_->size() >= kMaxCacheSize)
    return false;

  return IsPopular(url);
}

bool ThumbnailStore::IsURLBlacklisted(const GURL& url) const {
  if (url_blacklist_)
    return url_blacklist_->HasKey(GetDictionaryKeyForURL(url.spec()));
  return false;
}

std::wstring ThumbnailStore::GetDictionaryKeyForURL(
    const std::string& url) const {
  return ASCIIToWide(MD5String(url));
}

bool ThumbnailStore::IsPopular(const GURL& url) const {
  return most_visited_urls_->size() < kMaxCacheSize ||
         most_visited_urls_->find(url) != most_visited_urls_->end();
}
