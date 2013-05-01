// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_likely_impl.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop_proxy.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/task_runner.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/top_sites_cache.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/most_visited_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/thumbnail_score.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_util.h"

using base::DictionaryValue;
using content::BrowserThread;
using content::NavigationController;

namespace history {

namespace {

void RunOrPostGetMostVisitedURLsCallback(
    base::TaskRunner* task_runner,
    const TopSitesLikelyImpl::GetMostVisitedURLsCallback& callback,
    const MostVisitedURLList& urls) {
  if (task_runner->RunsTasksOnCurrentThread())
    callback.Run(urls);
  else
    task_runner->PostTask(FROM_HERE, base::Bind(callback, urls));
}

}  // namespace

// How many top sites to store in the cache.
static const size_t kTopSitesNumber = 20;

// Max number of temporary images we'll cache. See comment above
// temp_images_ for details.
static const size_t kMaxTempTopImages = 8;

static const int kDaysOfHistory = 90;
// Time from startup to first HistoryService query.
static const int64 kUpdateIntervalSecs = 15;
// Intervals between requests to HistoryService.
static const int64 kMinUpdateIntervalMinutes = 1;
static const int64 kMaxUpdateIntervalMinutes = 60;

// Use 100 quality (highest quality) because we're very sensitive to
// artifacts for these small sized, highly detailed images.
static const int kTopSitesImageQuality = 100;

namespace {

// HistoryDBTask used during migration of thumbnails from history to top sites.
// When run on the history thread it collects the top sites and the
// corresponding thumbnails. When run back on the ui thread it calls into
// TopSitesLikelyImpl::FinishHistoryMigration.
class LoadThumbnailsFromHistoryTask : public HistoryDBTask {
 public:
  LoadThumbnailsFromHistoryTask(TopSites* top_sites,
                                int result_count)
      : top_sites_(top_sites),
        result_count_(result_count) {
    // l10n_util isn't thread safe, so cache for use on the db thread.
    ignore_urls_.insert(l10n_util::GetStringUTF8(IDS_CHROME_WELCOME_URL));
    ignore_urls_.insert(l10n_util::GetStringUTF8(IDS_WEBSTORE_URL));
#if defined(OS_ANDROID)
    ignore_urls_.insert(l10n_util::GetStringUTF8(IDS_MOBILE_WELCOME_URL));
#endif
  }

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    // Get the most visited urls.
    backend->QueryMostVisitedURLsImpl(result_count_,
                                      kDaysOfHistory,
                                      &data_.most_visited);

    // And fetch the thumbnails.
    for (size_t i = 0; i < data_.most_visited.size(); ++i) {
      const GURL& url = data_.most_visited[i].url;
      if (ShouldFetchThumbnailFor(url)) {
        scoped_refptr<base::RefCountedBytes> data;
        backend->GetPageThumbnailDirectly(url, &data);
        data_.url_to_thumbnail_map[url] = data;
      }
    }
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    top_sites_->FinishHistoryMigration(data_);
  }

 private:
  virtual ~LoadThumbnailsFromHistoryTask() {}

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

TopSitesLikelyImpl::TopSitesLikelyImpl(Profile* profile)
    : backend_(NULL),
      cache_(new TopSitesCache()),
      thread_safe_cache_(new TopSitesCache()),
      profile_(profile),
      last_num_urls_changed_(0),
      history_state_(HISTORY_LOADING),
      top_sites_state_(TOP_SITES_LOADING),
      loaded_(false) {
  if (!profile_)
    return;

  if (content::NotificationService::current()) {
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                   content::Source<Profile>(profile_));
    // Listen for any nav commits. We'll ignore those not related to this
    // profile when we get the notification.
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::NotificationService::AllSources());
  }
  for (size_t i = 0; i < arraysize(kPrepopulatedPages); i++) {
    int url_id = kPrepopulatedPages[i].url_id;
    prepopulated_page_urls_.push_back(
        GURL(l10n_util::GetStringUTF8(url_id)));
  }
}

void TopSitesLikelyImpl::Init(const base::FilePath& db_name) {
  // Create the backend here, rather than in the constructor, so that
  // unit tests that do not need the backend can run without a problem.
  backend_ = new TopSitesBackend;
  backend_->Init(db_name);
  backend_->GetMostVisitedThumbnails(
      base::Bind(&TopSitesLikelyImpl::OnGotMostVisitedThumbnails,
                 base::Unretained(this)),
      &cancelable_task_tracker_);

  // History may have already finished loading by the time we're created.
  HistoryService* history =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile_);
  if (history && history->backend_loaded()) {
    if (history->needs_top_sites_migration())
      MigrateFromHistory();
    else
      history_state_ = HISTORY_LOADED;
  }
}

bool TopSitesLikelyImpl::SetPageThumbnail(const GURL& url,
                                    const gfx::Image& thumbnail,
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

  scoped_refptr<base::RefCountedBytes> thumbnail_data;
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

// WARNING: this function may be invoked on any thread.
void TopSitesLikelyImpl::GetMostVisitedURLs(
    const GetMostVisitedURLsCallback& callback) {
  MostVisitedURLList filtered_urls;
  {
    base::AutoLock lock(lock_);
    if (!loaded_) {
      // A request came in before we finished loading. Store the callback and
      // we'll run it on current thread when we finish loading.
      pending_callbacks_.push_back(
          base::Bind(&RunOrPostGetMostVisitedURLsCallback,
                     base::MessageLoopProxy::current(),
                     callback));
      return;
    }
    filtered_urls = thread_safe_cache_->top_sites();
  }
  callback.Run(filtered_urls);
}

bool TopSitesLikelyImpl::GetPageThumbnail(
    const GURL& url, scoped_refptr<base::RefCountedMemory>* bytes) {
  // WARNING: this may be invoked on any thread.
  {
    base::AutoLock lock(lock_);
    if (thread_safe_cache_->GetPageThumbnail(url, bytes))
      return true;
  }

  // Resource bundle is thread safe.
  for (size_t i = 0; i < arraysize(kPrepopulatedPages); i++) {
    if (url == prepopulated_page_urls_[i]) {
      *bytes = ResourceBundle::GetSharedInstance().
          LoadDataResourceBytesForScale(
              kPrepopulatedPages[i].thumbnail_id,
              ui::SCALE_FACTOR_100P);
      return true;
    }
  }

  return false;
}

bool TopSitesLikelyImpl::GetPageThumbnailScore(const GURL& url,
                                         ThumbnailScore* score) {
  // WARNING: this may be invoked on any thread.
  base::AutoLock lock(lock_);
  return thread_safe_cache_->GetPageThumbnailScore(url, score);
}

bool TopSitesLikelyImpl::GetTemporaryPageThumbnailScore(const GURL& url,
                                                  ThumbnailScore* score) {
  for (TempImages::iterator i = temp_images_.begin(); i != temp_images_.end();
       ++i) {
    if (i->first == url) {
      *score = i->second.thumbnail_score;
      return true;
    }
  }
  return false;
}


// Returns the index of |url| in |urls|, or -1 if not found.
static int IndexOf(const MostVisitedURLList& urls, const GURL& url) {
  for (size_t i = 0; i < urls.size(); i++) {
    if (urls[i].url == url)
      return i;
  }
  return -1;
}

void TopSitesLikelyImpl::MigrateFromHistory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (history_state_ != HISTORY_LOADING) {
    // This can happen if history was unloaded then loaded again.
    return;
  }

  history_state_ = HISTORY_MIGRATING;
  HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS)->ScheduleDBTask(
          new LoadThumbnailsFromHistoryTask(
              this,
              num_results_to_request_from_history()),
          &history_consumer_);
}

void TopSitesLikelyImpl::FinishHistoryMigration(
    const ThumbnailMigration& data) {
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
      base::Bind(&TopSitesLikelyImpl::OnHistoryMigrationWrittenToDisk,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

void TopSitesLikelyImpl::HistoryLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  // else case can happen if history is unloaded, then loaded again.
}

void TopSitesLikelyImpl::SyncWithHistory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (loaded_ && temp_images_.size()) {
    // If we have temporary thumbnails it means there isn't much data, and most
    // likely the user is first running Chrome. During this time we throttle
    // updating from history by 30 seconds. If the user creates a new tab page
    // during this window of time we force updating from history so that the new
    // tab page isn't so far out of date.
    timer_.Stop();
    StartQueryForMostVisited();
  }
}

bool TopSitesLikelyImpl::HasBlacklistedItems() const {
  const DictionaryValue* blacklist =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpMostVisitedURLsBlacklist);
  return blacklist && !blacklist->empty();
}

void TopSitesLikelyImpl::AddBlacklistedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Value* dummy = Value::CreateNullValue();
  {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kNtpMostVisitedURLsBlacklist);
    DictionaryValue* blacklist = update.Get();
    blacklist->SetWithoutPathExpansion(GetURLHash(url), dummy);
  }

  ResetThreadSafeCache();
  NotifyTopSitesChanged();
}

void TopSitesLikelyImpl::RemoveBlacklistedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kNtpMostVisitedURLsBlacklist);
    DictionaryValue* blacklist = update.Get();
    blacklist->RemoveWithoutPathExpansion(GetURLHash(url), NULL);
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged();
}

bool TopSitesLikelyImpl::IsBlacklisted(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const DictionaryValue* blacklist =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpMostVisitedURLsBlacklist);
  return blacklist && blacklist->HasKey(GetURLHash(url));
}

void TopSitesLikelyImpl::ClearBlacklistedURLs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kNtpMostVisitedURLsBlacklist);
    DictionaryValue* blacklist = update.Get();
    blacklist->Clear();
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged();
}

void TopSitesLikelyImpl::Shutdown() {
  profile_ = NULL;
  // Cancel all requests so that the service doesn't callback to us after we've
  // invoked Shutdown (this could happen if we have a pending request and
  // Shutdown is invoked).
  history_consumer_.CancelAllRequests();
  backend_->Shutdown();
}

// static
void TopSitesLikelyImpl::DiffMostVisited(const MostVisitedURLList& old_list,
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

CancelableRequestProvider::Handle
    TopSitesLikelyImpl::StartQueryForMostVisited() {
  DCHECK(loaded_);
  if (!profile_)
    return 0;

  HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  // |hs| may be null during unit tests.
  if (hs) {
    return hs->QueryMostVisitedURLs(
        num_results_to_request_from_history(),
        kDaysOfHistory,
        &history_consumer_,
        base::Bind(&TopSitesLikelyImpl::OnTopSitesAvailableFromHistory,
                   base::Unretained(this)));
  }
  return 0;
}

bool TopSitesLikelyImpl::IsKnownURL(const GURL& url) {
  return loaded_ && cache_->IsKnownURL(url);
}

bool TopSitesLikelyImpl::IsFull() {
  return loaded_ && cache_->top_sites().size() >= kTopSitesNumber;
}

TopSitesLikelyImpl::~TopSitesLikelyImpl() {
}

bool TopSitesLikelyImpl::SetPageThumbnailNoDB(
    const GURL& url,
    const base::RefCountedBytes* thumbnail_data,
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

  image->thumbnail = const_cast<base::RefCountedBytes*>(thumbnail_data);
  image->thumbnail_score = new_score_with_redirects;

  ResetThreadSafeImageCache();
  return true;
}

bool TopSitesLikelyImpl::SetPageThumbnailEncoded(
    const GURL& url,
    const base::RefCountedBytes* thumbnail,
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
bool TopSitesLikelyImpl::EncodeBitmap(const gfx::Image& bitmap,
                                scoped_refptr<base::RefCountedBytes>* bytes) {
  if (bitmap.IsEmpty())
    return false;
  *bytes = new base::RefCountedBytes();
  std::vector<unsigned char> data;
  if (!gfx::JPEG1xEncodedDataFromImage(bitmap, kTopSitesImageQuality, &data))
    return false;

  // As we're going to cache this data, make sure the vector is only as big as
  // it needs to be, as JPEGCodec::Encode() over-allocates data.capacity().
  // (In a C++0x future, we can just call shrink_to_fit() in Encode())
  (*bytes)->data() = data;
  return true;
}

void TopSitesLikelyImpl::RemoveTemporaryThumbnailByURL(const GURL& url) {
  for (TempImages::iterator i = temp_images_.begin(); i != temp_images_.end();
       ++i) {
    if (i->first == url) {
      temp_images_.erase(i);
      return;
    }
  }
}

void TopSitesLikelyImpl::AddTemporaryThumbnail(const GURL& url,
                                         const base::RefCountedBytes* thumbnail,
                                         const ThumbnailScore& score) {
  if (temp_images_.size() == kMaxTempTopImages)
    temp_images_.erase(temp_images_.begin());

  TempImage image;
  image.first = url;
  image.second.thumbnail = const_cast<base::RefCountedBytes*>(thumbnail);
  image.second.thumbnail_score = score;
  temp_images_.push_back(image);
}

void TopSitesLikelyImpl::TimerFired() {
  StartQueryForMostVisited();
}

// static
int TopSitesLikelyImpl::GetRedirectDistanceForURL(
    const MostVisitedURL& most_visited,
    const GURL& url) {
  for (size_t i = 0; i < most_visited.redirects.size(); i++) {
    if (most_visited.redirects[i] == url)
      return static_cast<int>(most_visited.redirects.size() - i - 1);
  }
  NOTREACHED() << "URL should always be found.";
  return 0;
}

MostVisitedURLList TopSitesLikelyImpl::GetPrepopulatePages() {
  MostVisitedURLList urls;
  urls.resize(arraysize(kPrepopulatedPages));
  for (size_t i = 0; i < urls.size(); ++i) {
    MostVisitedURL& url = urls[i];
    url.url = GURL(prepopulated_page_urls_[i]);
    url.redirects.push_back(url.url);
    url.title = l10n_util::GetStringUTF16(kPrepopulatedPages[i].title_id);
  }
  return urls;
}

bool TopSitesLikelyImpl::loaded() const {
  return loaded_;
}

bool TopSitesLikelyImpl::AddPrepopulatedPages(MostVisitedURLList* urls) {
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

void TopSitesLikelyImpl::ApplyBlacklist(const MostVisitedURLList& urls,
                                  MostVisitedURLList* out) {
  for (size_t i = 0; i < urls.size() && i < kTopSitesNumber; ++i) {
    if (!IsBlacklisted(urls[i].url))
      out->push_back(urls[i]);
  }
}

std::string TopSitesLikelyImpl::GetURLString(const GURL& url) {
  return cache_->GetCanonicalURL(url).spec();
}

std::string TopSitesLikelyImpl::GetURLHash(const GURL& url) {
  // We don't use canonical URLs here to be able to blacklist only one of
  // the two 'duplicate' sites, e.g. 'gmail.com' and 'mail.google.com'.
  return base::MD5String(url.spec());
}

base::TimeDelta TopSitesLikelyImpl::GetUpdateDelay() {
  if (cache_->top_sites().size() <= arraysize(kPrepopulatedPages))
    return base::TimeDelta::FromSeconds(30);

  int64 range = kMaxUpdateIntervalMinutes - kMinUpdateIntervalMinutes;
  int64 minutes = kMaxUpdateIntervalMinutes -
      last_num_urls_changed_ * range / cache_->top_sites().size();
  return base::TimeDelta::FromMinutes(minutes);
}

void TopSitesLikelyImpl::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  if (!loaded_)
    return;

  if (type == chrome::NOTIFICATION_HISTORY_URLS_DELETED) {
    content::Details<history::URLsDeletedDetails> deleted_details(details);
    if (deleted_details->all_history) {
      SetTopSites(MostVisitedURLList());
      backend_->ResetDatabase();
    } else {
      std::set<size_t> indices_to_delete;  // Indices into top_sites_.
      for (URLRows::const_iterator i = deleted_details->rows.begin();
           i != deleted_details->rows.end(); ++i) {
        if (cache_->IsKnownURL(i->url()))
          indices_to_delete.insert(cache_->GetURLIndex(i->url()));
      }

      if (indices_to_delete.empty())
        return;

      MostVisitedURLList new_top_sites(cache_->top_sites());
      for (std::set<size_t>::reverse_iterator i = indices_to_delete.rbegin();
           i != indices_to_delete.rend(); i++) {
        new_top_sites.erase(new_top_sites.begin() + *i);
      }
      SetTopSites(new_top_sites);
    }
    StartQueryForMostVisited();
  } else if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    NavigationController* controller =
        content::Source<NavigationController>(source).ptr();
    Profile* profile = Profile::FromBrowserContext(
        controller->GetWebContents()->GetBrowserContext());
    if (profile == profile_ && !IsFull()) {
      content::LoadCommittedDetails* load_details =
          content::Details<content::LoadCommittedDetails>(details).ptr();
      if (!load_details)
        return;
      const GURL& url = load_details->entry->GetURL();
      if (!cache_->IsKnownURL(url) && HistoryService::CanAddURL(url)) {
        // To avoid slamming history we throttle requests when the url updates.
        // To do otherwise negatively impacts perf tests.
        RestartQueryForTopSitesTimer(GetUpdateDelay());
      }
    }
  }
}

void TopSitesLikelyImpl::SetTopSites(const MostVisitedURLList& new_top_sites) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MostVisitedURLList top_sites(new_top_sites);
  AddPrepopulatedPages(&top_sites);

  TopSitesDelta delta;
  DiffMostVisited(cache_->top_sites(), top_sites, &delta);
  if (!delta.deleted.empty() || !delta.added.empty() || !delta.moved.empty()) {
    backend_->UpdateTopSites(delta);
  }

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
  NotifyTopSitesChanged();

  // Restart the timer that queries history for top sites. This is done to
  // ensure we stay in sync with history.
  RestartQueryForTopSitesTimer(GetUpdateDelay());
}

int TopSitesLikelyImpl::num_results_to_request_from_history() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const DictionaryValue* blacklist =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpMostVisitedURLsBlacklist);
  return kTopSitesNumber + (blacklist ? blacklist->size() : 0);
}

void TopSitesLikelyImpl::MoveStateToLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MostVisitedURLList filtered_urls;
  PendingCallbacks pending_callbacks;
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

  for (size_t i = 0; i < pending_callbacks.size(); i++)
    pending_callbacks[i].Run(filtered_urls);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOP_SITES_LOADED,
      content::Source<Profile>(profile_),
      content::Details<TopSites>(this));
}

void TopSitesLikelyImpl::ResetThreadSafeCache() {
  base::AutoLock lock(lock_);
  MostVisitedURLList cached;
  ApplyBlacklist(cache_->top_sites(), &cached);
  thread_safe_cache_->SetTopSites(cached);
}

void TopSitesLikelyImpl::ResetThreadSafeImageCache() {
  base::AutoLock lock(lock_);
  thread_safe_cache_->SetThumbnails(cache_->images());
}

void TopSitesLikelyImpl::NotifyTopSitesChanged() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOP_SITES_CHANGED,
      content::Source<TopSites>(this),
      content::NotificationService::NoDetails());
}

void TopSitesLikelyImpl::RestartQueryForTopSitesTimer(base::TimeDelta delta) {
  if (timer_.IsRunning() && ((timer_start_time_ + timer_.GetCurrentDelay()) <
                             (base::TimeTicks::Now() + delta))) {
    return;
  }

  timer_start_time_ = base::TimeTicks::Now();
  timer_.Stop();
  timer_.Start(FROM_HERE, delta, this, &TopSitesLikelyImpl::TimerFired);
}

void TopSitesLikelyImpl::OnHistoryMigrationWrittenToDisk() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!profile_)
    return;

  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  if (history)
    history->OnTopSitesReady();
}

void TopSitesLikelyImpl::OnGotMostVisitedThumbnails(
    const scoped_refptr<MostVisitedThumbnails>& thumbnails,
    const bool* need_history_migration) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(top_sites_state_, TOP_SITES_LOADING);

  if (!*need_history_migration) {
    top_sites_state_ = TOP_SITES_LOADED;

    // Set the top sites directly in the cache so that SetTopSites diffs
    // correctly.
    cache_->SetTopSites(thumbnails->most_visited);
    SetTopSites(thumbnails->most_visited);
    cache_->SetThumbnails(thumbnails->url_to_images_map);

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
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
    }
  }
}

void TopSitesLikelyImpl::OnTopSitesAvailableFromHistory(
    CancelableRequestProvider::Handle handle,
    MostVisitedURLList pages) {
  SetTopSites(pages);

  // Used only in testing.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOP_SITES_UPDATED,
      content::Source<TopSitesLikelyImpl>(this),
      content::Details<CancelableRequestProvider::Handle>(&handle));
}

}  // namespace history
