// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"

#include <algorithm>
#include <set>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/top_sites_backend.h"
#include "chrome/browser/history/top_sites_cache.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/webui/most_visited_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/thumbnail_score.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace history {

// How many top sites to store in the cache.
static const size_t kTopSitesNumber = 20;

// Max number of temporary images we'll cache. See comment above
// temp_images_ for details.
static const size_t kMaxTempTopImages = 8;

static const size_t kTopSitesShown = 8;
static const int kDaysOfHistory = 90;
// Time from startup to first HistoryService query.
static const int64 kUpdateIntervalSecs = 15;
// Intervals between requests to HistoryService.
static const int64 kMinUpdateIntervalMinutes = 1;
static const int64 kMaxUpdateIntervalMinutes = 60;

// IDs of the sites we force into top sites.
static const int kPrepopulatePageIDs[] =
    { IDS_CHROME_WELCOME_URL, IDS_THEMES_GALLERY_URL };

// Favicons of the sites we force into top sites.
static const char kPrepopulateFaviconURLs[][54] =
    { "chrome://theme/IDR_NEWTAB_CHROME_WELCOME_PAGE_FAVICON",
      "chrome://theme/IDR_NEWTAB_THEMES_GALLERY_FAVICON" };

static const int kPrepopulateTitleIDs[] =
    { IDS_NEW_TAB_CHROME_WELCOME_PAGE_TITLE,
      IDS_NEW_TAB_THEMES_GALLERY_PAGE_TITLE };

namespace {

// HistoryDBTask used during migration of thumbnails from history to top sites.
// When run on the history thread it collects the top sites and the
// corresponding thumbnails. When run back on the ui thread it calls into
// TopSites::FinishHistoryMigration.
class LoadThumbnailsFromHistoryTask : public HistoryDBTask {
 public:
  LoadThumbnailsFromHistoryTask(TopSites* top_sites,
                                int result_count)
      : top_sites_(top_sites),
        result_count_(result_count) {
    // l10n_util isn't thread safe, so cache for use on the db thread.
    ignore_urls_.insert(l10n_util::GetStringUTF8(IDS_CHROME_WELCOME_URL));
    ignore_urls_.insert(l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL));
  }

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    // Get the most visited urls.
    backend->QueryMostVisitedURLsImpl(result_count_,
                                      kDaysOfHistory,
                                      &data_.most_visited);

    // And fetch the thumbnails.
    for (size_t i = 0; i < data_.most_visited.size(); ++i) {
      const GURL& url = data_.most_visited[i].url;
      if (ShouldFetchThumbnailFor(url)) {
        scoped_refptr<RefCountedBytes> data;
        backend->GetPageThumbnailDirectly(url, &data);
        data_.url_to_thumbnail_map[url] = data;
      }
    }
    return true;
  }

  virtual void DoneRunOnMainThread() {
    top_sites_->FinishHistoryMigration(data_);
  }

 private:
  bool ShouldFetchThumbnailFor(const GURL& url) {
    return ignore_urls_.find(url.spec()) == ignore_urls_.end();
  }

  // Set of URLs we don't load thumbnails for. This is created on the UI thread
  // and used on the history thread.
  std::set<std::string> ignore_urls_;

  scoped_refptr<TopSites> top_sites_;

  // Number of results to request from history.
  const int result_count_;

  ThumbnailMigration data_;

  DISALLOW_COPY_AND_ASSIGN(LoadThumbnailsFromHistoryTask);
};

}  // namespace

TopSites::TopSites(Profile* profile)
    : backend_(NULL),
      cache_(new TopSitesCache()),
      thread_safe_cache_(new TopSitesCache()),
      profile_(profile),
      last_num_urls_changed_(0),
      blacklist_(NULL),
      pinned_urls_(NULL),
      history_state_(HISTORY_LOADING),
      top_sites_state_(TOP_SITES_LOADING),
      loaded_(false) {
  if (!profile_)
    return;

  if (NotificationService::current()) {
    registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
                   Source<Profile>(profile_));
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   NotificationService::AllSources());
  }

  blacklist_ = profile_->GetPrefs()->
      GetMutableDictionary(prefs::kNTPMostVisitedURLsBlacklist);
  pinned_urls_ = profile_->GetPrefs()->
      GetMutableDictionary(prefs::kNTPMostVisitedPinnedURLs);
}

void TopSites::Init(const FilePath& db_name) {
  // Create the backend here, rather than in the constructor, so that
  // unit tests that do not need the backend can run without a problem.
  backend_ = new TopSitesBackend;
  backend_->Init(db_name);
  backend_->GetMostVisitedThumbnails(
      &cancelable_consumer_,
      NewCallback(this, &TopSites::OnGotMostVisitedThumbnails));

  // History may have already finished loading by the time we're created.
  HistoryService* history = profile_->GetHistoryServiceWithoutCreating();
  if (history && history->backend_loaded()) {
    if (history->needs_top_sites_migration())
      MigrateFromHistory();
    else
      history_state_ = HISTORY_LOADED;
  }
}

bool TopSites::SetPageThumbnail(const GURL& url,
                                const SkBitmap& thumbnail,
                                const ThumbnailScore& score) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!loaded_) {
    // TODO(sky): I need to cache these and apply them after the load
    // completes.
    return false;
  }

  bool add_temp_thumbnail = false;
  if (!IsKnownURL(url)) {
    if (!IsFull()) {
      add_temp_thumbnail = true;
    } else {
      return false;  // This URL is not known to us.
    }
  }

  if (!HistoryService::CanAddURL(url))
    return false;  // It's not a real webpage.

  scoped_refptr<RefCountedBytes> thumbnail_data;
  if (!EncodeBitmap(thumbnail, &thumbnail_data))
    return false;

  if (add_temp_thumbnail) {
    // Always remove the existing entry and then add it back. That way if we end
    // up with too many temp thumbnails we'll prune the oldest first.
    RemoveTemporaryThumbnailByURL(url);
    AddTemporaryThumbnail(url, thumbnail_data, score);
    return true;
  }

  return SetPageThumbnailEncoded(url, thumbnail_data, score);
}

void TopSites::GetMostVisitedURLs(CancelableRequestConsumer* consumer,
                                  GetTopSitesCallback* callback) {
  // WARNING: this may be invoked on any thread.
  scoped_refptr<CancelableRequest<GetTopSitesCallback> > request(
      new CancelableRequest<GetTopSitesCallback>(callback));
  // This ensures cancellation of requests when either the consumer or the
  // provider is deleted. Deletion of requests is also guaranteed.
  AddRequest(request, consumer);
  MostVisitedURLList filtered_urls;
  {
    base::AutoLock lock(lock_);
    if (!loaded_) {
      // A request came in before we finished loading. Put the request in
      // pending_callbacks_ and we'll notify it when we finish loading.
      pending_callbacks_.insert(request);
      return;
    }

    filtered_urls = thread_safe_cache_->top_sites();
  }
  request->ForwardResult(GetTopSitesCallback::TupleType(filtered_urls));
}

bool TopSites::GetPageThumbnail(const GURL& url,
                                scoped_refptr<RefCountedBytes>* bytes) {
  // WARNING: this may be invoked on any thread.
  base::AutoLock lock(lock_);
  return thread_safe_cache_->GetPageThumbnail(url, bytes);
}

bool TopSites::GetPageThumbnailScore(const GURL& url,
                                     ThumbnailScore* score) {
  // WARNING: this may be invoked on any thread.
  base::AutoLock lock(lock_);
  return thread_safe_cache_->GetPageThumbnailScore(url, score);
}

// Returns the index of |url| in |urls|, or -1 if not found.
static int IndexOf(const MostVisitedURLList& urls, const GURL& url) {
  for (size_t i = 0; i < urls.size(); i++) {
    if (urls[i].url == url)
      return i;
  }
  return -1;
}

void TopSites::MigrateFromHistory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(history_state_, HISTORY_LOADING);

  history_state_ = HISTORY_MIGRATING;
  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->ScheduleDBTask(
      new LoadThumbnailsFromHistoryTask(
          this,
          num_results_to_request_from_history()),
      &cancelable_consumer_);
  MigratePinnedURLs();
}

void TopSites::FinishHistoryMigration(const ThumbnailMigration& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(history_state_, HISTORY_MIGRATING);

  history_state_ = HISTORY_LOADED;

  SetTopSites(data.most_visited);

  for (size_t i = 0; i < data.most_visited.size(); ++i) {
    URLToThumbnailMap::const_iterator image_i =
        data.url_to_thumbnail_map.find(data.most_visited[i].url);
    if (image_i != data.url_to_thumbnail_map.end()) {
      SetPageThumbnailEncoded(data.most_visited[i].url,
                              image_i->second,
                              ThumbnailScore());
    }
  }

  MoveStateToLoaded();

  ResetThreadSafeImageCache();

  // We've scheduled all the thumbnails and top sites to be written to the top
  // sites db, but it hasn't happened yet. Schedule a request on the db thread
  // that notifies us when done. When done we'll know everything was written and
  // we can tell history to finish its part of migration.
  backend_->DoEmptyRequest(
      &cancelable_consumer_,
      NewCallback(this, &TopSites::OnHistoryMigrationWrittenToDisk));
}

void TopSites::HistoryLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_NE(history_state_, HISTORY_LOADED);

  if (history_state_ != HISTORY_MIGRATING) {
    // No migration from history is needed.
    history_state_ = HISTORY_LOADED;
    if (top_sites_state_ == TOP_SITES_LOADED_WAITING_FOR_HISTORY) {
      // TopSites thought it needed migration, but it really didn't. This
      // typically happens the first time a profile is run with Top Sites
      // enabled
      SetTopSites(MostVisitedURLList());
      MoveStateToLoaded();
    }
  }
}

bool TopSites::HasBlacklistedItems() const {
  return !blacklist_->empty();
}

void TopSites::AddBlacklistedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RemovePinnedURL(url);
  Value* dummy = Value::CreateNullValue();
  blacklist_->SetWithoutPathExpansion(GetURLHash(url), dummy);

  ResetThreadSafeCache();
}

void TopSites::RemoveBlacklistedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  blacklist_->RemoveWithoutPathExpansion(GetURLHash(url), NULL);
  ResetThreadSafeCache();
}

bool TopSites::IsBlacklisted(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return blacklist_->HasKey(GetURLHash(url));
}

void TopSites::ClearBlacklistedURLs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  blacklist_->Clear();
  ResetThreadSafeCache();
}

void TopSites::AddPinnedURL(const GURL& url, size_t pinned_index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GURL old;
  if (GetPinnedURLAtIndex(pinned_index, &old))
    RemovePinnedURL(old);

  if (IsURLPinned(url))
    RemovePinnedURL(url);

  Value* index = Value::CreateIntegerValue(pinned_index);
  pinned_urls_->SetWithoutPathExpansion(GetURLString(url), index);

  ResetThreadSafeCache();
}

bool TopSites::IsURLPinned(const GURL& url) {
  int tmp;
  return pinned_urls_->GetIntegerWithoutPathExpansion(GetURLString(url), &tmp);
}

void TopSites::RemovePinnedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pinned_urls_->RemoveWithoutPathExpansion(GetURLString(url), NULL);

  ResetThreadSafeCache();
}

bool TopSites::GetPinnedURLAtIndex(size_t index, GURL* url) {
  for (DictionaryValue::key_iterator it = pinned_urls_->begin_keys();
       it != pinned_urls_->end_keys(); ++it) {
    int current_index;
    if (pinned_urls_->GetIntegerWithoutPathExpansion(*it, &current_index)) {
      if (static_cast<size_t>(current_index) == index) {
        *url = GURL(*it);
        return true;
      }
    }
  }
  return false;
}

void TopSites::Shutdown() {
  profile_ = NULL;
  // Cancel all requests so that the service doesn't callback to us after we've
  // invoked Shutdown (this could happen if we have a pending request and
  // Shutdown is invoked).
  cancelable_consumer_.CancelAllRequests();
  backend_->Shutdown();
}

// static
void TopSites::DiffMostVisited(const MostVisitedURLList& old_list,
                               const MostVisitedURLList& new_list,
                               TopSitesDelta* delta) {
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
      MostVisitedURLWithRank added;
      added.url = new_list[i];
      added.rank = i;
      delta->added.push_back(added);
    } else {
      if (found->second != i) {
        MostVisitedURLWithRank moved;
        moved.url = new_list[i];
        moved.rank = i;
        delta->moved.push_back(moved);
      }
      found->second = kAlreadyFoundMarker;
    }
  }

  // Any member without the special marker in the all_old_urls list means that
  // there wasn't a "new" URL that mapped to it, so it was deleted.
  for (std::map<GURL, size_t>::const_iterator i = all_old_urls.begin();
       i != all_old_urls.end(); ++i) {
    if (i->second != kAlreadyFoundMarker)
      delta->deleted.push_back(old_list[i->second]);
  }
}

CancelableRequestProvider::Handle TopSites::StartQueryForMostVisited() {
  DCHECK(loaded_);
  if (!profile_)
    return 0;

  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  // |hs| may be null during unit tests.
  if (hs) {
    return hs->QueryMostVisitedURLs(
        num_results_to_request_from_history(),
        kDaysOfHistory,
        &cancelable_consumer_,
        NewCallback(this, &TopSites::OnTopSitesAvailableFromHistory));
  }
  return 0;
}

bool TopSites::IsKnownURL(const GURL& url) {
  return loaded_ && cache_->IsKnownURL(url);
}

bool TopSites::IsFull() {
  return loaded_ && cache_->top_sites().size() >= kTopSitesNumber;
}

TopSites::~TopSites() {
}

bool TopSites::SetPageThumbnailNoDB(const GURL& url,
                                    const RefCountedBytes* thumbnail_data,
                                    const ThumbnailScore& score) {
  // This should only be invoked when we know about the url.
  DCHECK(cache_->IsKnownURL(url));

  const MostVisitedURL& most_visited =
      cache_->top_sites()[cache_->GetURLIndex(url)];
  Images* image = cache_->GetImage(url);

  // When comparing the thumbnail scores, we need to take into account the
  // redirect hops, which are not generated when the thumbnail is because the
  // redirects weren't known. We fill that in here since we know the redirects.
  ThumbnailScore new_score_with_redirects(score);
  new_score_with_redirects.redirect_hops_from_dest =
      GetRedirectDistanceForURL(most_visited, url);

  if (!ShouldReplaceThumbnailWith(image->thumbnail_score,
                                  new_score_with_redirects) &&
      image->thumbnail.get())
    return false;  // The one we already have is better.

  image->thumbnail = const_cast<RefCountedBytes*>(thumbnail_data);
  image->thumbnail_score = new_score_with_redirects;

  ResetThreadSafeImageCache();
  return true;
}

bool TopSites::SetPageThumbnailEncoded(const GURL& url,
                                       const RefCountedBytes* thumbnail,
                                       const ThumbnailScore& score) {
  if (!SetPageThumbnailNoDB(url, thumbnail, score))
    return false;

  // Update the database.
  if (!cache_->IsKnownURL(url))
    return false;

  size_t index = cache_->GetURLIndex(url);
  const MostVisitedURL& most_visited = cache_->top_sites()[index];
  backend_->SetPageThumbnail(most_visited,
                             index,
                             *(cache_->GetImage(most_visited.url)));
  return true;
}

// static
bool TopSites::EncodeBitmap(const SkBitmap& bitmap,
                            scoped_refptr<RefCountedBytes>* bytes) {
  *bytes = new RefCountedBytes();
  SkAutoLockPixels bitmap_lock(bitmap);
  std::vector<unsigned char> data;
  if (!gfx::JPEGCodec::Encode(
          reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
          gfx::JPEGCodec::FORMAT_BGRA, bitmap.width(),
          bitmap.height(),
          static_cast<int>(bitmap.rowBytes()), 90,
          &data)) {
    return false;
  }
  // As we're going to cache this data, make sure the vector is only as big as
  // it needs to be.
  (*bytes)->data = data;
  return true;
}

void TopSites::RemoveTemporaryThumbnailByURL(const GURL& url) {
  for (TempImages::iterator i = temp_images_.begin(); i != temp_images_.end();
       ++i) {
    if (i->first == url) {
      temp_images_.erase(i);
      return;
    }
  }
}

void TopSites::AddTemporaryThumbnail(const GURL& url,
                                     const RefCountedBytes* thumbnail,
                                     const ThumbnailScore& score) {
  if (temp_images_.size() == kMaxTempTopImages)
    temp_images_.erase(temp_images_.begin());

  TempImage image;
  image.first = url;
  image.second.thumbnail = const_cast<RefCountedBytes*>(thumbnail);
  image.second.thumbnail_score = score;
  temp_images_.push_back(image);
}

void TopSites::TimerFired() {
  StartQueryForMostVisited();
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
MostVisitedURLList TopSites::GetPrepopulatePages() {
  MostVisitedURLList urls;
  urls.resize(arraysize(kPrepopulatePageIDs));
  for (size_t i = 0; i < arraysize(kPrepopulatePageIDs); ++i) {
    MostVisitedURL& url = urls[i];
    url.url = GURL(l10n_util::GetStringUTF8(kPrepopulatePageIDs[i]));
    url.redirects.push_back(url.url);
    url.favicon_url = GURL(kPrepopulateFaviconURLs[i]);
    url.title = l10n_util::GetStringUTF16(kPrepopulateTitleIDs[i]);
  }
  return urls;
}

// static
bool TopSites::AddPrepopulatedPages(MostVisitedURLList* urls) {
  bool added = false;
  MostVisitedURLList prepopulate_urls = GetPrepopulatePages();
  for (size_t i = 0; i < prepopulate_urls.size(); ++i) {
    if (urls->size() < kTopSitesNumber &&
        IndexOf(*urls, prepopulate_urls[i].url) == -1) {
      urls->push_back(prepopulate_urls[i]);
      added = true;
    }
  }
  return added;
}

void TopSites::MigratePinnedURLs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::map<GURL, size_t> tmp_map;
  for (DictionaryValue::key_iterator it = pinned_urls_->begin_keys();
       it != pinned_urls_->end_keys(); ++it) {
    Value* value;
    if (!pinned_urls_->GetWithoutPathExpansion(*it, &value))
      continue;

    if (value->IsType(DictionaryValue::TYPE_DICTIONARY)) {
      DictionaryValue* dict = static_cast<DictionaryValue*>(value);
      std::string url_string;
      int index;
      if (dict->GetString("url", &url_string) &&
          dict->GetInteger("index", &index))
        tmp_map[GURL(url_string)] = index;
    }
  }
  pinned_urls_->Clear();
  for (std::map<GURL, size_t>::iterator it = tmp_map.begin();
       it != tmp_map.end(); ++it)
    AddPinnedURL(it->first, it->second);
}

void TopSites::ApplyBlacklistAndPinnedURLs(const MostVisitedURLList& urls,
                                           MostVisitedURLList* out) {
  MostVisitedURLList urls_copy;
  for (size_t i = 0; i < urls.size(); i++) {
    if (!IsBlacklisted(urls[i].url))
      urls_copy.push_back(urls[i]);
  }

  for (size_t pinned_index = 0; pinned_index < kTopSitesShown; pinned_index++) {
    GURL url;
    bool found = GetPinnedURLAtIndex(pinned_index, &url);
    if (!found)
      continue;

    DCHECK(!url.is_empty());
    int cur_index = IndexOf(urls_copy, url);
    MostVisitedURL tmp;
    if (cur_index < 0) {
      // Pinned URL not in urls.
      tmp.url = url;
    } else {
      tmp = urls_copy[cur_index];
      urls_copy.erase(urls_copy.begin() + cur_index);
    }
    if (pinned_index > out->size())
      out->resize(pinned_index);  // Add empty URLs as fillers.
    out->insert(out->begin() + pinned_index, tmp);
  }

  // Add non-pinned URLs in the empty spots.
  size_t current_url = 0;  // Index into the remaining URLs in urls_copy.
  for (size_t i = 0; i < kTopSitesShown && current_url < urls_copy.size();
       i++) {
    if (i == out->size()) {
      out->push_back(urls_copy[current_url]);
      current_url++;
    } else if (i < out->size()) {
      if ((*out)[i].url.is_empty()) {
        // Replace the filler
        (*out)[i] = urls_copy[current_url];
        current_url++;
      }
    } else {
      NOTREACHED();
    }
  }
}

std::string TopSites::GetURLString(const GURL& url) {
  return cache_->GetCanonicalURL(url).spec();
}

std::string TopSites::GetURLHash(const GURL& url) {
  // We don't use canonical URLs here to be able to blacklist only one of
  // the two 'duplicate' sites, e.g. 'gmail.com' and 'mail.google.com'.
  return MD5String(url.spec());
}

base::TimeDelta TopSites::GetUpdateDelay() {
  if (cache_->top_sites().size() <= arraysize(kPrepopulateTitleIDs))
    return base::TimeDelta::FromSeconds(30);

  int64 range = kMaxUpdateIntervalMinutes - kMinUpdateIntervalMinutes;
  int64 minutes = kMaxUpdateIntervalMinutes -
      last_num_urls_changed_ * range / cache_->top_sites().size();
  return base::TimeDelta::FromMinutes(minutes);
}

// static
void TopSites::ProcessPendingCallbacks(
    const PendingCallbackSet& pending_callbacks,
    const MostVisitedURLList& urls) {
  PendingCallbackSet::const_iterator i;
  for (i = pending_callbacks.begin();
       i != pending_callbacks.end(); ++i) {
    scoped_refptr<CancelableRequest<GetTopSitesCallback> > request = *i;
    if (!request->canceled())
      request->ForwardResult(GetTopSitesCallback::TupleType(urls));
  }
}

void TopSites::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  if (!loaded_)
    return;

  if (type == NotificationType::HISTORY_URLS_DELETED) {
    Details<history::URLsDeletedDetails> deleted_details(details);
    if (deleted_details->all_history) {
      SetTopSites(MostVisitedURLList());
      backend_->ResetDatabase();
    } else {
      std::set<size_t> indices_to_delete;  // Indices into top_sites_.
      for (std::set<GURL>::iterator i = deleted_details->urls.begin();
           i != deleted_details->urls.end(); ++i) {
        if (cache_->IsKnownURL(*i))
          indices_to_delete.insert(cache_->GetURLIndex(*i));
      }

      if (indices_to_delete.empty())
        return;

      MostVisitedURLList new_top_sites(cache_->top_sites());
      for (std::set<size_t>::reverse_iterator i = indices_to_delete.rbegin();
           i != indices_to_delete.rend(); i++) {
        size_t index = *i;
        RemovePinnedURL(new_top_sites[index].url);
        new_top_sites.erase(new_top_sites.begin() + index);
      }
      SetTopSites(new_top_sites);
    }
    StartQueryForMostVisited();
  } else if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    if (!IsFull()) {
      NavigationController::LoadCommittedDetails* load_details =
          Details<NavigationController::LoadCommittedDetails>(details).ptr();
      if (!load_details)
        return;
      const GURL& url = load_details->entry->url();
      if (!cache_->IsKnownURL(url) && HistoryService::CanAddURL(url)) {
        // To avoid slamming history we throttle requests when the url updates.
        // To do otherwise negatively impacts perf tests.
        RestartQueryForTopSitesTimer(GetUpdateDelay());
      }
    }
  }
}

void TopSites::SetTopSites(const MostVisitedURLList& new_top_sites) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MostVisitedURLList top_sites(new_top_sites);
  AddPrepopulatedPages(&top_sites);

  TopSitesDelta delta;
  DiffMostVisited(cache_->top_sites(), top_sites, &delta);
  if (!delta.deleted.empty() || !delta.added.empty() || !delta.moved.empty())
    backend_->UpdateTopSites(delta);

  last_num_urls_changed_ = delta.added.size() + delta.moved.size();

  // We always do the following steps (setting top sites in cache, and resetting
  // thread safe cache ...) as this method is invoked during startup at which
  // point the caches haven't been updated yet.
  cache_->SetTopSites(top_sites);

  // See if we have any tmp thumbnails for the new sites.
  if (!temp_images_.empty()) {
    for (size_t i = 0; i < top_sites.size(); ++i) {
      const MostVisitedURL& mv = top_sites[i];
      GURL canonical_url = cache_->GetCanonicalURL(mv.url);
      // At the time we get the thumbnail redirects aren't known, so we have to
      // iterate through all the images.
      for (TempImages::iterator it = temp_images_.begin();
           it != temp_images_.end(); ++it) {
        if (canonical_url == cache_->GetCanonicalURL(it->first)) {
          SetPageThumbnailEncoded(mv.url,
                                  it->second.thumbnail,
                                  it->second.thumbnail_score);
          temp_images_.erase(it);
          break;
        }
      }
    }
  }

  if (top_sites.size() >= kTopSitesNumber)
    temp_images_.clear();

  ResetThreadSafeCache();
  ResetThreadSafeImageCache();

  // Restart the timer that queries history for top sites. This is done to
  // ensure we stay in sync with history.
  RestartQueryForTopSitesTimer(GetUpdateDelay());
}

int TopSites::num_results_to_request_from_history() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return kTopSitesNumber + blacklist_->size();
}

void TopSites::MoveStateToLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MostVisitedURLList filtered_urls;
  PendingCallbackSet pending_callbacks;
  {
    base::AutoLock lock(lock_);

    if (loaded_)
      return;  // Don't do anything if we're already loaded.
    loaded_ = true;

    // Now that we're loaded we can service the queued up callbacks. Copy them
    // here and service them outside the lock.
    if (!pending_callbacks_.empty()) {
      filtered_urls = thread_safe_cache_->top_sites();
      pending_callbacks.swap(pending_callbacks_);
    }
  }

  ProcessPendingCallbacks(pending_callbacks, filtered_urls);

  NotificationService::current()->Notify(NotificationType::TOP_SITES_LOADED,
                                         Source<Profile>(profile_),
                                         Details<TopSites>(this));
}

void TopSites::ResetThreadSafeCache() {
  base::AutoLock lock(lock_);
  MostVisitedURLList cached;
  ApplyBlacklistAndPinnedURLs(cache_->top_sites(), &cached);
  thread_safe_cache_->SetTopSites(cached);
}

void TopSites::ResetThreadSafeImageCache() {
  base::AutoLock lock(lock_);
  thread_safe_cache_->SetThumbnails(cache_->images());
  thread_safe_cache_->RemoveUnreferencedThumbnails();
}

void TopSites::RestartQueryForTopSitesTimer(base::TimeDelta delta) {
  if (timer_.IsRunning() && ((timer_start_time_ + timer_.GetCurrentDelay()) <
                             (base::TimeTicks::Now() + delta))) {
    return;
  }

  timer_start_time_ = base::TimeTicks::Now();
  timer_.Stop();
  timer_.Start(delta, this, &TopSites::TimerFired);
}

void TopSites::OnHistoryMigrationWrittenToDisk(TopSitesBackend::Handle handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!profile_)
    return;

  HistoryService* history =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (history)
    history->OnTopSitesReady();
}

void TopSites::OnGotMostVisitedThumbnails(
    CancelableRequestProvider::Handle handle,
    scoped_refptr<MostVisitedThumbnails> data,
    bool may_need_history_migration) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(top_sites_state_, TOP_SITES_LOADING);

  if (!may_need_history_migration) {
    top_sites_state_ = TOP_SITES_LOADED;

    // Set the top sites directly in the cache so that SetTopSites diffs
    // correctly.
    cache_->SetTopSites(data->most_visited);
    SetTopSites(data->most_visited);
    cache_->SetThumbnails(data->url_to_images_map);

    ResetThreadSafeImageCache();

    MoveStateToLoaded();

    // Start a timer that refreshes top sites from history.
    RestartQueryForTopSitesTimer(
        base::TimeDelta::FromSeconds(kUpdateIntervalSecs));
  } else {
    // The top sites file didn't exist or is the wrong version. We need to wait
    // for history to finish loading to know if we really needed to migrate.
    if (history_state_ == HISTORY_LOADED) {
      top_sites_state_ = TOP_SITES_LOADED;
      SetTopSites(MostVisitedURLList());
      MoveStateToLoaded();
    } else {
      top_sites_state_ = TOP_SITES_LOADED_WAITING_FOR_HISTORY;
      // Ask for history just in case it hasn't been loaded yet. When history
      // finishes loading we'll do migration and/or move to loaded.
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    }
  }
}

void TopSites::OnTopSitesAvailableFromHistory(
    CancelableRequestProvider::Handle handle,
    MostVisitedURLList pages) {
  SetTopSites(pages);

  // Used only in testing.
  NotificationService::current()->Notify(
      NotificationType::TOP_SITES_UPDATED,
      Source<TopSites>(this),
      Details<CancelableRequestProvider::Handle>(&handle));
}

}  // namespace history
