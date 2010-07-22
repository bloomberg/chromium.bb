// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/history/top_sites_database.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "gfx/codec/jpeg_codec.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace history {

// How many top sites to store in the cache.
static const size_t kTopSitesNumber = 20;
static const int kDaysOfHistory = 90;
// Time from startup to first HistoryService query.
static const int64 kUpdateIntervalSecs = 15;
// Intervals between requests to HistoryService.
static const int64 kMinUpdateIntervalMinutes = 1;
static const int64 kMaxUpdateIntervalMinutes = 60;


TopSites::TopSites(Profile* profile) : profile_(profile),
                                       mock_history_service_(NULL),
                                       last_num_urls_changed_(0),
                                       migration_in_progress_(false),
                                       waiting_for_results_(true) {
  registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
                 Source<Profile>(profile_));
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 NotificationService::AllSources());
}

TopSites::~TopSites() {
  timer_.Stop();
}

void TopSites::Init(const FilePath& db_name) {
  db_path_ = db_name;
  db_.reset(new TopSitesDatabaseImpl());
  if (!db_->Init(db_name)) {
    NOTREACHED() << "Failed to initialize database.";
    return;
  }

  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, NewRunnableMethod(
      this, &TopSites::ReadDatabase));

  // Start the one-shot timer.
  timer_.Start(base::TimeDelta::FromSeconds(kUpdateIntervalSecs), this,
               &TopSites::StartQueryForMostVisited);
}

void TopSites::ReadDatabase() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  std::map<GURL, Images> thumbnails;

  DCHECK(db_.get());
  {
    AutoLock lock(lock_);
    MostVisitedURLList top_urls;
    db_->GetPageThumbnails(&top_urls, &thumbnails);
    StoreMostVisited(&top_urls);
  }  // Lock is released here.

  for (size_t i = 0; i < top_sites_.size(); i++) {
    GURL url = top_sites_[i].url;
    Images thumbnail = thumbnails[url];
    if (!thumbnail.thumbnail.get() || !thumbnail.thumbnail->size()) {
      LOG(INFO) << "No thumbnail for " << url.spec();
    } else {
      SetPageThumbnailNoDB(url, thumbnail.thumbnail,
                           thumbnail.thumbnail_score);
    }
  }
}

// Public function that encodes the bitmap into RefCountedBytes and
// updates the database.
bool TopSites::SetPageThumbnail(const GURL& url,
                                const SkBitmap& thumbnail,
                                const ThumbnailScore& score) {
  bool add_temp_thumbnail = false;
  if (canonical_urls_.find(url) == canonical_urls_.end()) {
    if (top_sites_.size() < kTopSitesNumber) {
      add_temp_thumbnail = true;
    } else {
      return false;  // This URL is not known to us.
    }
  }

  if (!HistoryService::CanAddURL(url))
    return false;  // It's not a real webpage.

  scoped_refptr<RefCountedBytes> thumbnail_data = new RefCountedBytes;
  SkAutoLockPixels thumbnail_lock(thumbnail);
  bool encoded = gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(thumbnail.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_BGRA, thumbnail.width(),
      thumbnail.height(),
      static_cast<int>(thumbnail.rowBytes()), 90,
      &thumbnail_data->data);
  if (!encoded)
    return false;

  if (add_temp_thumbnail) {
    AddTemporaryThumbnail(url, thumbnail_data, score);
    return true;
  }

  return SetPageThumbnail(url, thumbnail_data, score);
}

bool TopSites::SetPageThumbnail(const GURL& url,
                                const RefCountedBytes* thumbnail,
                                const ThumbnailScore& score) {
  if (!SetPageThumbnailNoDB(url, thumbnail, score))
    return false;

  // Update the database.
  if (!db_.get())
    return true;
  std::map<GURL, size_t>::iterator found = canonical_urls_.find(url);
  if (found == canonical_urls_.end())
    return false;
  size_t index = found->second;

  MostVisitedURL& most_visited = top_sites_[index];
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, NewRunnableMethod(
      this, &TopSites::WriteThumbnailToDB,
      most_visited, index, top_images_[most_visited.url]));
  return true;
}

void TopSites::WriteThumbnailToDB(const MostVisitedURL& url,
                                  int url_rank,
                                  const TopSites::Images& thumbnail) {
  DCHECK(db_.get());
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  db_->SetPageThumbnail(url, url_rank, thumbnail);
}

// private
bool TopSites::SetPageThumbnailNoDB(const GURL& url,
                                    const RefCountedBytes* thumbnail_data,
                                    const ThumbnailScore& score) {
  AutoLock lock(lock_);

  std::map<GURL, size_t>::iterator found = canonical_urls_.find(url);
  if (found == canonical_urls_.end()) {
    if (top_sites_.size() >= kTopSitesNumber)
      return false;  // This URL is not known to us.

    // We don't have enough Top Sites - add this one too.
    MostVisitedURL mv;
    mv.url = url;
    mv.redirects.push_back(url);
    top_sites_.push_back(mv);
    size_t index = top_sites_.size() - 1;
    StoreRedirectChain(top_sites_[index].redirects, index);
    found = canonical_urls_.find(url);
  }

  MostVisitedURL& most_visited = top_sites_[found->second];
  Images& image = top_images_[most_visited.url];

  // When comparing the thumbnail scores, we need to take into account the
  // redirect hops, which are not generated when the thumbnail is because the
  // redirects weren't known. We fill that in here since we know the redirects.
  ThumbnailScore new_score_with_redirects(score);
  new_score_with_redirects.redirect_hops_from_dest =
      GetRedirectDistanceForURL(most_visited, url);

  if (!ShouldReplaceThumbnailWith(image.thumbnail_score,
                                  new_score_with_redirects) &&
      image.thumbnail.get())
    return false;  // The one we already have is better.

  // Take ownership of the thumbnail data.
  image.thumbnail = const_cast<RefCountedBytes*>(thumbnail_data);
  image.thumbnail_score = new_score_with_redirects;

  return true;
}

void TopSites::GetMostVisitedURLs(CancelableRequestConsumer* consumer,
                                  GetTopSitesCallback* callback) {

  scoped_refptr<CancelableRequest<GetTopSitesCallback> > request(
      new CancelableRequest<GetTopSitesCallback>(callback));
  // This ensures cancelation of requests when either the consumer or the
  // provider is deleted. Deletion of requests is also guaranteed.
  AddRequest(request, consumer);
  if (waiting_for_results_) {
    // A request came in before we have any top sites.
    // We have to keep track of the requests ourselves.
    pending_callbacks_.insert(request);
    return;
  }
  if (request->canceled())
    return;
  request->ForwardResult(GetTopSitesCallback::TupleType(top_sites_));
}

bool TopSites::GetPageThumbnail(const GURL& url, RefCountedBytes** data) const {
  std::map<GURL, Images>::const_iterator found = top_images_.find(url);
  if (found == top_images_.end())
    return false;  // No thumbnail for this URL.

  Images image = found->second;
  *data = image.thumbnail.get();
  return true;
}

void TopSites::UpdateMostVisited(MostVisitedURLList most_visited) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  // TODO(brettw) filter for blacklist!

  if (!top_sites_.empty()) {
    std::vector<size_t> added;    // Indices into most_visited.
    std::vector<size_t> deleted;  // Indices into top_sites_.
    std::vector<size_t> moved;    // Indices into most_visited.
    DiffMostVisited(top_sites_, most_visited, &added, &deleted, &moved);

    // #added == #deleted; #added + #moved = total.
    last_num_urls_changed_ = added.size() + moved.size();

    // Process the diff: delete from images and disk, add to disk.
    // Delete all the thumbnails associated with URLs that were deleted.
    for (size_t i = 0; i < deleted.size(); i++) {
      const MostVisitedURL& deleted_url = top_sites_[deleted[i]];
      std::map<GURL, Images>::iterator found =
          top_images_.find(deleted_url.url);
      if (found != top_images_.end())
        top_images_.erase(found);

      // Delete from disk.
      if (db_.get())
        db_->RemoveURL(deleted_url);
    }

    if (db_.get()) {
      // Write both added and moved urls.
      for (size_t i = 0; i < added.size(); i++) {
        MostVisitedURL& added_url = most_visited[added[i]];
        db_->SetPageThumbnail(added_url, added[i], Images());
      }
      for (size_t i = 0; i < moved.size(); i++) {
        MostVisitedURL moved_url = most_visited[moved[i]];
        db_->UpdatePageRank(moved_url, moved[i]);
      }
    }
  }

  StoreMostVisited(&most_visited);
  if (migration_in_progress_) {
    // Copy all thumnbails from the history service.
    for (size_t i = 0; i < top_sites_.size(); i++) {
      GURL& url = top_sites_[i].url;
      Images& img = top_images_[url];
      if (!img.thumbnail.get() || !img.thumbnail->size()) {
        StartQueryForThumbnail(i);
      }
    }
  }

  // If we are not expecting any thumbnails, migration is done.
  if (migration_in_progress_ && migration_pending_urls_.empty())
    OnMigrationDone();

  timer_.Stop();
  timer_.Start(GetUpdateDelay(), this,
               &TopSites::StartQueryForMostVisited);
}

void TopSites::OnMigrationDone() {
  migration_in_progress_ = false;
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  // |hs| may be null during unit tests.
  if (!hs)
    return;
  hs->OnTopSitesReady();
}

void TopSites::AddTemporaryThumbnail(const GURL& url,
                                     const RefCountedBytes* thumbnail,
                                     const ThumbnailScore& score) {
  Images& img = temp_thumbnails_map_[url];
  img.thumbnail = const_cast<RefCountedBytes*>(thumbnail);
  img.thumbnail_score = score;
}

void TopSites::StartQueryForThumbnail(size_t index) {
  DCHECK(migration_in_progress_);
  migration_pending_urls_.insert(top_sites_[index].url);

  if (mock_history_service_) {
    // Testing with a mockup.
    // QueryMostVisitedURLs is not virtual, so we have to duplicate the code.
    // This calls SetClientData.
    mock_history_service_->GetPageThumbnail(
        top_sites_[index].url,
        &cancelable_consumer_,
        NewCallback(this, &TopSites::OnThumbnailAvailable),
        index);
    return;
  }

  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  // |hs| may be null during unit tests.
  if (!hs)
    return;
  HistoryService::Handle handle =
      hs->GetPageThumbnail(top_sites_[index].url,
                           &cancelable_consumer_,
                           NewCallback(this, &TopSites::OnThumbnailAvailable));
  cancelable_consumer_.SetClientData(hs, handle, index);
}

void TopSites::StoreMostVisited(MostVisitedURLList* most_visited) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  // Take ownership of the most visited data.
  top_sites_.clear();
  top_sites_.swap(*most_visited);
  waiting_for_results_ = false;

  // Save the redirect information for quickly mapping to the canonical URLs.
  canonical_urls_.clear();
  for (size_t i = 0; i < top_sites_.size(); i++) {
    const MostVisitedURL& mv = top_sites_[i];
    StoreRedirectChain(mv.redirects, i);

    std::map<GURL, Images>::iterator it = temp_thumbnails_map_.begin();
    GURL canonical_url = GetCanonicalURL(mv.url);
    for (; it != temp_thumbnails_map_.end(); it++) {
      // Must map all temp URLs to canonical ones.
      // temp_thumbnails_map_ contains non-canonical URLs, because
      // when we add a temp thumbnail, redirect chain is not known.
      // This is slow, but temp_thumbnails_map_ should have very few URLs.
      if (canonical_url == GetCanonicalURL(it->first)) {
        SetPageThumbnail(mv.url, it->second.thumbnail,
                         it->second.thumbnail_score);
        temp_thumbnails_map_.erase(it);
        break;
      }
    }
  }
  if (top_sites_.size() >= kTopSitesNumber)
    temp_thumbnails_map_.clear();
}

void TopSites::StoreRedirectChain(const RedirectList& redirects,
                                  size_t destination) {
  if (redirects.empty()) {
    NOTREACHED();
    return;
  }

  // Map all the redirected URLs to the destination.
  for (size_t i = 0; i < redirects.size(); i++)
    canonical_urls_[redirects[i]] = destination;
}

GURL TopSites::GetCanonicalURL(const GURL& url) const {
  std::map<GURL, size_t>::const_iterator found = canonical_urls_.find(url);
  if (found == canonical_urls_.end())
    return GURL();  // Don't know anything about this URL.
  return top_sites_[found->second].url;
}

// static
int TopSites::GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                        const GURL& url) {
  for (size_t i = 0; i < most_visited.redirects.size(); i++) {
    if (most_visited.redirects[i] == url)
      return static_cast<int>(most_visited.redirects.size() - i - 1);
  }
  NOTREACHED() << "URL should always be found.";
  return 0;
}

// static
void TopSites::DiffMostVisited(const MostVisitedURLList& old_list,
                               const MostVisitedURLList& new_list,
                               std::vector<size_t>* added_urls,
                               std::vector<size_t>* deleted_urls,
                               std::vector<size_t>* moved_urls) {
  added_urls->clear();
  deleted_urls->clear();
  moved_urls->clear();

  // Add all the old URLs for quick lookup. This maps URLs to the corresponding
  // index in the input.
  std::map<GURL, size_t> all_old_urls;
  for (size_t i = 0; i < old_list.size(); i++)
    all_old_urls[old_list[i].url] = i;

  // Check all the URLs in the new set to see which ones are new or just moved.
  // When we find a match in the old set, we'll reset its index to our special
  // marker. This allows us to quickly identify the deleted ones in a later
  // pass.
  const size_t kAlreadyFoundMarker = static_cast<size_t>(-1);
  for (size_t i = 0; i < new_list.size(); i++) {
    std::map<GURL, size_t>::iterator found = all_old_urls.find(new_list[i].url);
    if (found == all_old_urls.end()) {
      added_urls->push_back(i);
    } else {
      if (found->second != i)
        moved_urls->push_back(i);
      found->second = kAlreadyFoundMarker;
    }
  }

  // Any member without the special marker in the all_old_urls list means that
  // there wasn't a "new" URL that mapped to it, so it was deleted.
  for (std::map<GURL, size_t>::const_iterator i = all_old_urls.begin();
       i != all_old_urls.end(); ++i) {
    if (i->second != kAlreadyFoundMarker)
      deleted_urls->push_back(i->second);
  }
}

void TopSites::StartQueryForMostVisited() {
  if (mock_history_service_) {
    // Testing with a mockup.
    // QueryMostVisitedURLs is not virtual, so we have to duplicate the code.
    mock_history_service_->QueryMostVisitedURLs(
        kTopSitesNumber,
        kDaysOfHistory,
        &cancelable_consumer_,
        NewCallback(this, &TopSites::OnTopSitesAvailable));
  } else {
    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    // |hs| may be null during unit tests.
    if (hs) {
      hs->QueryMostVisitedURLs(
          kTopSitesNumber,
          kDaysOfHistory,
          &cancelable_consumer_,
          NewCallback(this, &TopSites::OnTopSitesAvailable));
    } else {
      LOG(INFO) << "History Service not available.";
    }
  }
}

void TopSites::StartMigration() {
  migration_in_progress_ = true;
  StartQueryForMostVisited();
}

base::TimeDelta TopSites::GetUpdateDelay() {
  if (top_sites_.size() == 0)
    return base::TimeDelta::FromSeconds(30);

  int64 range = kMaxUpdateIntervalMinutes - kMinUpdateIntervalMinutes;
  int64 minutes = kMaxUpdateIntervalMinutes -
      last_num_urls_changed_ * range / top_sites_.size();
  return base::TimeDelta::FromMinutes(minutes);
}

void TopSites::OnTopSitesAvailable(
    CancelableRequestProvider::Handle handle,
    MostVisitedURLList pages) {
  if (!pending_callbacks_.empty()) {
    PendingCallbackSet copy(pending_callbacks_);
    PendingCallbackSet::iterator i;
    for (i = pending_callbacks_.begin();
         i != pending_callbacks_.end(); ++i) {
      scoped_refptr<CancelableRequest<GetTopSitesCallback> > request = *i;
      if (!request->canceled())
        request->ForwardResult(GetTopSitesCallback::TupleType(pages));
    }
    pending_callbacks_.clear();
  }

  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, NewRunnableMethod(
      this, &TopSites::UpdateMostVisited, pages));
}

void TopSites::OnThumbnailAvailable(CancelableRequestProvider::Handle handle,
                                    scoped_refptr<RefCountedBytes> thumbnail) {
  size_t index;
  if (mock_history_service_) {
    index = handle;
  } else {
    HistoryService* hs = profile_ ->GetHistoryService(Profile::EXPLICIT_ACCESS);
    index = cancelable_consumer_.GetClientData(hs, handle);
  }
  DCHECK(static_cast<size_t>(index) < top_sites_.size());

  if (migration_in_progress_)
    migration_pending_urls_.erase(top_sites_[index].url);

  if (thumbnail.get() && thumbnail->size()) {
    const MostVisitedURL& url = top_sites_[index];
    SetPageThumbnail(url.url, thumbnail, ThumbnailScore());
  }

  if (migration_in_progress_ && migration_pending_urls_.empty() &&
      !mock_history_service_)
    OnMigrationDone();
}

void TopSites::SetMockHistoryService(MockHistoryService* mhs) {
  mock_history_service_ = mhs;
}

void TopSites::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  if (type == NotificationType::HISTORY_URLS_DELETED) {
    Details<history::URLsDeletedDetails> deleted_details(details);
    if (deleted_details->all_history) {
      top_sites_.clear();
      ChromeThread::PostTask(ChromeThread::DB, FROM_HERE,
                             NewRunnableMethod(this, &TopSites::ResetDatabase));
    } else {
      std::set<GURL>::iterator it;
      for (it = deleted_details->urls.begin();
           it != deleted_details->urls.end(); ++it) {
        for (size_t i = 0; i < top_sites_.size(); i++) {
          if (top_sites_[i].url == *it) {
            top_sites_.erase(top_sites_.begin() + i);
            break;
          }
        }
      }
    }
    StartQueryForMostVisited();
  } else if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    if (top_sites_.size() < kTopSitesNumber) {
      const NavigationController::LoadCommittedDetails& load_details =
          *Details<NavigationController::LoadCommittedDetails>(details).ptr();
      GURL url = load_details.entry->url();
      if (canonical_urls_.find(url) == canonical_urls_.end() &&
           HistoryService::CanAddURL(url)) {
        // Add this page to the known pages in case the thumbnail comes
        // in before we get the results.
        MostVisitedURL mv;
        mv.url = url;
        mv.redirects.push_back(url);
        top_sites_.push_back(mv);
        size_t index = top_sites_.size() - 1;
        StoreRedirectChain(top_sites_[index].redirects, index);
      }
      StartQueryForMostVisited();
    }
  }
}

void TopSites::ResetDatabase() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  db_.reset(new TopSitesDatabaseImpl());
  file_util::Delete(db_path_, false);
  if (!db_->Init(db_path_)) {
    NOTREACHED() << "Failed to initialize database.";
    return;
  }
}

}  // namespace history
