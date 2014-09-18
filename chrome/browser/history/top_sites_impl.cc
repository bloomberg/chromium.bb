// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_impl.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites_cache.h"
#include "chrome/browser/history/url_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/common/thumbnail_score.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
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
    bool include_forced_urls,
    const TopSitesImpl::GetMostVisitedURLsCallback& callback,
    const MostVisitedURLList& all_urls,
    const MostVisitedURLList& nonforced_urls) {
  const MostVisitedURLList* urls =
      include_forced_urls ? &all_urls : &nonforced_urls;
  if (task_runner->RunsTasksOnCurrentThread())
    callback.Run(*urls);
  else
    task_runner->PostTask(FROM_HERE, base::Bind(callback, *urls));
}

// Compares two MostVisitedURL having a non-null |last_forced_time|.
bool ForcedURLComparator(const MostVisitedURL& first,
                         const MostVisitedURL& second) {
  DCHECK(!first.last_forced_time.is_null() &&
         !second.last_forced_time.is_null());
  return first.last_forced_time < second.last_forced_time;
}

}  // namespace

// How many non-forced top sites to store in the cache.
static const size_t kNonForcedTopSitesNumber = 20;

// How many forced top sites to store in the cache.
static const size_t kForcedTopSitesNumber = 20;

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

TopSitesImpl::TopSitesImpl(Profile* profile)
    : backend_(NULL),
      cache_(new TopSitesCache()),
      thread_safe_cache_(new TopSitesCache()),
      profile_(profile),
      last_num_urls_changed_(0),
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

void TopSitesImpl::Init(const base::FilePath& db_name) {
  // Create the backend here, rather than in the constructor, so that
  // unit tests that do not need the backend can run without a problem.
  backend_ = new TopSitesBackend;
  backend_->Init(db_name);
  backend_->GetMostVisitedThumbnails(
      base::Bind(&TopSitesImpl::OnGotMostVisitedThumbnails,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

bool TopSitesImpl::SetPageThumbnail(const GURL& url,
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
    if (!IsNonForcedFull()) {
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
    AddTemporaryThumbnail(url, thumbnail_data.get(), score);
    return true;
  }

  return SetPageThumbnailEncoded(url, thumbnail_data.get(), score);
}

bool TopSitesImpl::SetPageThumbnailToJPEGBytes(
    const GURL& url,
    const base::RefCountedMemory* memory,
    const ThumbnailScore& score) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!loaded_) {
    // TODO(sky): I need to cache these and apply them after the load
    // completes.
    return false;
  }

  bool add_temp_thumbnail = false;
  if (!IsKnownURL(url)) {
    if (!IsNonForcedFull()) {
      add_temp_thumbnail = true;
    } else {
      return false;  // This URL is not known to us.
    }
  }

  if (!HistoryService::CanAddURL(url))
    return false;  // It's not a real webpage.

  if (add_temp_thumbnail) {
    // Always remove the existing entry and then add it back. That way if we end
    // up with too many temp thumbnails we'll prune the oldest first.
    RemoveTemporaryThumbnailByURL(url);
    AddTemporaryThumbnail(url, memory, score);
    return true;
  }

  return SetPageThumbnailEncoded(url, memory, score);
}

// WARNING: this function may be invoked on any thread.
void TopSitesImpl::GetMostVisitedURLs(
    const GetMostVisitedURLsCallback& callback,
    bool include_forced_urls) {
  MostVisitedURLList filtered_urls;
  {
    base::AutoLock lock(lock_);
    if (!loaded_) {
      // A request came in before we finished loading. Store the callback and
      // we'll run it on current thread when we finish loading.
      pending_callbacks_.push_back(
          base::Bind(&RunOrPostGetMostVisitedURLsCallback,
                     base::MessageLoopProxy::current(),
                     include_forced_urls,
                     callback));
      return;
    }
    if (include_forced_urls) {
      filtered_urls = thread_safe_cache_->top_sites();
    } else {
      filtered_urls.assign(thread_safe_cache_->top_sites().begin() +
                              thread_safe_cache_->GetNumForcedURLs(),
                           thread_safe_cache_->top_sites().end());
    }
  }
  callback.Run(filtered_urls);
}

bool TopSitesImpl::GetPageThumbnail(
    const GURL& url,
    bool prefix_match,
    scoped_refptr<base::RefCountedMemory>* bytes) {
  // WARNING: this may be invoked on any thread.
  // Perform exact match.
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

  if (prefix_match) {
    // If http or https, search with |url| first, then try the other one.
    std::vector<GURL> url_list;
    url_list.push_back(url);
    if (url.SchemeIsHTTPOrHTTPS())
      url_list.push_back(ToggleHTTPAndHTTPS(url));

    for (std::vector<GURL>::iterator it = url_list.begin();
         it != url_list.end(); ++it) {
      base::AutoLock lock(lock_);

      GURL canonical_url;
      // Test whether any stored URL is a prefix of |url|.
      canonical_url = thread_safe_cache_->GetGeneralizedCanonicalURL(*it);
      if (!canonical_url.is_empty() &&
          thread_safe_cache_->GetPageThumbnail(canonical_url, bytes)) {
        return true;
      }
    }
  }

  return false;
}

bool TopSitesImpl::GetPageThumbnailScore(const GURL& url,
                                         ThumbnailScore* score) {
  // WARNING: this may be invoked on any thread.
  base::AutoLock lock(lock_);
  return thread_safe_cache_->GetPageThumbnailScore(url, score);
}

bool TopSitesImpl::GetTemporaryPageThumbnailScore(const GURL& url,
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

void TopSitesImpl::SyncWithHistory() {
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

bool TopSitesImpl::HasBlacklistedItems() const {
  const base::DictionaryValue* blacklist =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpMostVisitedURLsBlacklist);
  return blacklist && !blacklist->empty();
}

void TopSitesImpl::AddBlacklistedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::Value* dummy = base::Value::CreateNullValue();
  {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kNtpMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->SetWithoutPathExpansion(GetURLHash(url), dummy);
  }

  ResetThreadSafeCache();
  NotifyTopSitesChanged();
}

void TopSitesImpl::RemoveBlacklistedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kNtpMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->RemoveWithoutPathExpansion(GetURLHash(url), NULL);
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged();
}

bool TopSitesImpl::IsBlacklisted(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const base::DictionaryValue* blacklist =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpMostVisitedURLsBlacklist);
  return blacklist && blacklist->HasKey(GetURLHash(url));
}

void TopSitesImpl::ClearBlacklistedURLs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kNtpMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->Clear();
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged();
}

void TopSitesImpl::Shutdown() {
  profile_ = NULL;
  // Cancel all requests so that the service doesn't callback to us after we've
  // invoked Shutdown (this could happen if we have a pending request and
  // Shutdown is invoked).
  cancelable_task_tracker_.TryCancelAll();
  backend_->Shutdown();
}

// static
void TopSitesImpl::DiffMostVisited(const MostVisitedURLList& old_list,
                                   const MostVisitedURLList& new_list,
                                   TopSitesDelta* delta) {

  // Add all the old URLs for quick lookup. This maps URLs to the corresponding
  // index in the input.
  std::map<GURL, size_t> all_old_urls;
  size_t num_old_forced = 0;
  for (size_t i = 0; i < old_list.size(); i++) {
    if (!old_list[i].last_forced_time.is_null())
      num_old_forced++;
    DCHECK(old_list[i].last_forced_time.is_null() || i < num_old_forced)
        << "Forced URLs must all appear before non-forced URLs.";
    all_old_urls[old_list[i].url] = i;
  }

  // Check all the URLs in the new set to see which ones are new or just moved.
  // When we find a match in the old set, we'll reset its index to our special
  // marker. This allows us to quickly identify the deleted ones in a later
  // pass.
  const size_t kAlreadyFoundMarker = static_cast<size_t>(-1);
  int rank = -1;  // Forced URLs have a rank of -1.
  for (size_t i = 0; i < new_list.size(); i++) {
    // Increase the rank if we're going through forced URLs. This works because
    // non-forced URLs all come after forced URLs.
    if (new_list[i].last_forced_time.is_null())
      rank++;
    DCHECK(new_list[i].last_forced_time.is_null() == (rank != -1))
        << "Forced URLs must all appear before non-forced URLs.";
    std::map<GURL, size_t>::iterator found = all_old_urls.find(new_list[i].url);
    if (found == all_old_urls.end()) {
      MostVisitedURLWithRank added;
      added.url = new_list[i];
      added.rank = rank;
      delta->added.push_back(added);
    } else {
      DCHECK(found->second != kAlreadyFoundMarker)
          << "Same URL appears twice in the new list.";
      int old_rank = found->second >= num_old_forced ?
          found->second - num_old_forced : -1;
      if (old_rank != rank ||
          old_list[found->second].last_forced_time !=
              new_list[i].last_forced_time) {
        MostVisitedURLWithRank moved;
        moved.url = new_list[i];
        moved.rank = rank;
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

base::CancelableTaskTracker::TaskId TopSitesImpl::StartQueryForMostVisited() {
  DCHECK(loaded_);
  if (!profile_)
    return base::CancelableTaskTracker::kBadTaskId;

  HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  // |hs| may be null during unit tests.
  if (hs) {
    return hs->QueryMostVisitedURLs(
        num_results_to_request_from_history(),
        kDaysOfHistory,
        base::Bind(&TopSitesImpl::OnTopSitesAvailableFromHistory,
                   base::Unretained(this)),
        &cancelable_task_tracker_);
  }
  return base::CancelableTaskTracker::kBadTaskId;
}

bool TopSitesImpl::IsKnownURL(const GURL& url) {
  return loaded_ && cache_->IsKnownURL(url);
}

const std::string& TopSitesImpl::GetCanonicalURLString(const GURL& url) const {
  return cache_->GetCanonicalURL(url).spec();
}

bool TopSitesImpl::IsNonForcedFull() {
  return loaded_ && cache_->GetNumNonForcedURLs() >= kNonForcedTopSitesNumber;
}

bool TopSitesImpl::IsForcedFull() {
  return loaded_ && cache_->GetNumForcedURLs() >= kForcedTopSitesNumber;
}

TopSitesImpl::~TopSitesImpl() {
}

bool TopSitesImpl::SetPageThumbnailNoDB(
    const GURL& url,
    const base::RefCountedMemory* thumbnail_data,
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

  image->thumbnail = const_cast<base::RefCountedMemory*>(thumbnail_data);
  image->thumbnail_score = new_score_with_redirects;

  ResetThreadSafeImageCache();
  return true;
}

bool TopSitesImpl::SetPageThumbnailEncoded(
    const GURL& url,
    const base::RefCountedMemory* thumbnail,
    const ThumbnailScore& score) {
  if (!SetPageThumbnailNoDB(url, thumbnail, score))
    return false;

  // Update the database.
  if (!cache_->IsKnownURL(url))
    return false;

  size_t index = cache_->GetURLIndex(url);
  int url_rank = index - cache_->GetNumForcedURLs();
  const MostVisitedURL& most_visited = cache_->top_sites()[index];
  backend_->SetPageThumbnail(most_visited,
                             url_rank < 0 ? -1 : url_rank,
                             *(cache_->GetImage(most_visited.url)));
  return true;
}

// static
bool TopSitesImpl::EncodeBitmap(const gfx::Image& bitmap,
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

void TopSitesImpl::RemoveTemporaryThumbnailByURL(const GURL& url) {
  for (TempImages::iterator i = temp_images_.begin(); i != temp_images_.end();
       ++i) {
    if (i->first == url) {
      temp_images_.erase(i);
      return;
    }
  }
}

void TopSitesImpl::AddTemporaryThumbnail(
    const GURL& url,
    const base::RefCountedMemory* thumbnail,
    const ThumbnailScore& score) {
  if (temp_images_.size() == kMaxTempTopImages)
    temp_images_.erase(temp_images_.begin());

  TempImage image;
  image.first = url;
  image.second.thumbnail = const_cast<base::RefCountedMemory*>(thumbnail);
  image.second.thumbnail_score = score;
  temp_images_.push_back(image);
}

void TopSitesImpl::TimerFired() {
  StartQueryForMostVisited();
}

// static
int TopSitesImpl::GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                            const GURL& url) {
  for (size_t i = 0; i < most_visited.redirects.size(); i++) {
    if (most_visited.redirects[i] == url)
      return static_cast<int>(most_visited.redirects.size() - i - 1);
  }
  NOTREACHED() << "URL should always be found.";
  return 0;
}

MostVisitedURLList TopSitesImpl::GetPrepopulatePages() {
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

bool TopSitesImpl::loaded() const {
  return loaded_;
}

bool TopSitesImpl::AddForcedURL(const GURL& url, const base::Time& time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  size_t num_forced = cache_->GetNumForcedURLs();
  MostVisitedURLList new_list(cache_->top_sites());
  MostVisitedURL new_url;

  if (cache_->IsKnownURL(url)) {
    size_t index = cache_->GetURLIndex(url);
    // Do nothing if we currently have that URL as non-forced.
    if (new_list[index].last_forced_time.is_null())
      return false;

    // Update the |last_forced_time| of the already existing URL. Delete it and
    // reinsert it at the right location.
    new_url = new_list[index];
    new_list.erase(new_list.begin() + index);
    num_forced--;
  } else {
    new_url.url = url;
    new_url.redirects.push_back(url);
  }
  new_url.last_forced_time = time;
  // Add forced URLs and sort. Added to the end of the list of forced URLs
  // since this is almost always where it needs to go, unless the user's local
  // clock is fiddled with.
  MostVisitedURLList::iterator mid = new_list.begin() + num_forced;
  new_list.insert(mid, new_url);
  mid = new_list.begin() + num_forced;  // Mid was invalidated.
  std::inplace_merge(new_list.begin(), mid, mid + 1, ForcedURLComparator);
  SetTopSites(new_list);
  return true;
}

bool TopSitesImpl::AddPrepopulatedPages(MostVisitedURLList* urls,
                                        size_t num_forced_urls) {
  bool added = false;
  MostVisitedURLList prepopulate_urls = GetPrepopulatePages();
  for (size_t i = 0; i < prepopulate_urls.size(); ++i) {
    if (urls->size() - num_forced_urls < kNonForcedTopSitesNumber &&
        IndexOf(*urls, prepopulate_urls[i].url) == -1) {
      urls->push_back(prepopulate_urls[i]);
      added = true;
    }
  }
  return added;
}

size_t TopSitesImpl::MergeCachedForcedURLs(MostVisitedURLList* new_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Add all the new URLs for quick lookup. Take that opportunity to count the
  // number of forced URLs in |new_list|.
  std::set<GURL> all_new_urls;
  size_t num_forced = 0;
  for (size_t i = 0; i < new_list->size(); ++i) {
    for (size_t j = 0; j < (*new_list)[i].redirects.size(); j++) {
      all_new_urls.insert((*new_list)[i].redirects[j]);
    }
    if (!(*new_list)[i].last_forced_time.is_null())
      ++num_forced;
  }

  // Keep the forced URLs from |cache_| that are not found in |new_list|.
  MostVisitedURLList filtered_forced_urls;
  for (size_t i = 0; i < cache_->GetNumForcedURLs(); ++i) {
    if (all_new_urls.find(cache_->top_sites()[i].url) == all_new_urls.end())
      filtered_forced_urls.push_back(cache_->top_sites()[i]);
  }
  num_forced += filtered_forced_urls.size();

  // Prepend forced URLs and sort in order of ascending |last_forced_time|.
  new_list->insert(new_list->begin(), filtered_forced_urls.begin(),
                   filtered_forced_urls.end());
  std::inplace_merge(
      new_list->begin(), new_list->begin() + filtered_forced_urls.size(),
      new_list->begin() + num_forced, ForcedURLComparator);

  // Drop older forced URLs if the list overflows. Since forced URLs are always
  // sort in increasing order of |last_forced_time|, drop the first ones.
  if (num_forced > kForcedTopSitesNumber) {
    new_list->erase(new_list->begin(),
                    new_list->begin() + (num_forced - kForcedTopSitesNumber));
    num_forced = kForcedTopSitesNumber;
  }

  return num_forced;
}

void TopSitesImpl::ApplyBlacklist(const MostVisitedURLList& urls,
                                  MostVisitedURLList* out) {
  // Log the number of times ApplyBlacklist is called so we can compute the
  // average number of blacklisted items per user.
  const base::DictionaryValue* blacklist =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpMostVisitedURLsBlacklist);
  UMA_HISTOGRAM_BOOLEAN("TopSites.NumberOfApplyBlacklist", true);
  UMA_HISTOGRAM_COUNTS_100("TopSites.NumberOfBlacklistedItems",
      (blacklist ? blacklist->size() : 0));
  size_t num_non_forced_urls = 0;
  size_t num_forced_urls = 0;
  for (size_t i = 0; i < urls.size(); ++i) {
    if (!IsBlacklisted(urls[i].url)) {
      if (urls[i].last_forced_time.is_null()) {
        // Non-forced URL.
        if (num_non_forced_urls >= kNonForcedTopSitesNumber)
          continue;
        num_non_forced_urls++;
      } else {
        // Forced URL.
        if (num_forced_urls >= kForcedTopSitesNumber)
          continue;
        num_forced_urls++;
      }
      out->push_back(urls[i]);
    }
  }
}

std::string TopSitesImpl::GetURLHash(const GURL& url) {
  // We don't use canonical URLs here to be able to blacklist only one of
  // the two 'duplicate' sites, e.g. 'gmail.com' and 'mail.google.com'.
  return base::MD5String(url.spec());
}

base::TimeDelta TopSitesImpl::GetUpdateDelay() {
  if (cache_->top_sites().size() <= arraysize(kPrepopulatedPages))
    return base::TimeDelta::FromSeconds(30);

  int64 range = kMaxUpdateIntervalMinutes - kMinUpdateIntervalMinutes;
  int64 minutes = kMaxUpdateIntervalMinutes -
      last_num_urls_changed_ * range / cache_->top_sites().size();
  return base::TimeDelta::FromMinutes(minutes);
}

void TopSitesImpl::Observe(int type,
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
    if (profile == profile_ && !IsNonForcedFull()) {
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

void TopSitesImpl::SetTopSites(const MostVisitedURLList& new_top_sites) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MostVisitedURLList top_sites(new_top_sites);
  size_t num_forced_urls = MergeCachedForcedURLs(&top_sites);
  AddPrepopulatedPages(&top_sites, num_forced_urls);

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
          SetPageThumbnailEncoded(
              mv.url, it->second.thumbnail.get(), it->second.thumbnail_score);
          temp_images_.erase(it);
          break;
        }
      }
    }
  }

  if (top_sites.size() - num_forced_urls >= kNonForcedTopSitesNumber)
    temp_images_.clear();

  ResetThreadSafeCache();
  ResetThreadSafeImageCache();
  NotifyTopSitesChanged();

  // Restart the timer that queries history for top sites. This is done to
  // ensure we stay in sync with history.
  RestartQueryForTopSitesTimer(GetUpdateDelay());
}

int TopSitesImpl::num_results_to_request_from_history() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const base::DictionaryValue* blacklist =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpMostVisitedURLsBlacklist);
  return kNonForcedTopSitesNumber + (blacklist ? blacklist->size() : 0);
}

void TopSitesImpl::MoveStateToLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MostVisitedURLList filtered_urls_all;
  MostVisitedURLList filtered_urls_nonforced;
  PendingCallbacks pending_callbacks;
  {
    base::AutoLock lock(lock_);

    if (loaded_)
      return;  // Don't do anything if we're already loaded.
    loaded_ = true;

    // Now that we're loaded we can service the queued up callbacks. Copy them
    // here and service them outside the lock.
    if (!pending_callbacks_.empty()) {
      // We always filter out forced URLs because callers of GetMostVisitedURLs
      // are not interested in them.
      filtered_urls_all = thread_safe_cache_->top_sites();
      filtered_urls_nonforced.assign(thread_safe_cache_->top_sites().begin() +
                                       thread_safe_cache_->GetNumForcedURLs(),
                                     thread_safe_cache_->top_sites().end());
      pending_callbacks.swap(pending_callbacks_);
    }
  }

  for (size_t i = 0; i < pending_callbacks.size(); i++)
    pending_callbacks[i].Run(filtered_urls_all, filtered_urls_nonforced);

  NotifyTopSitesLoaded();
}

void TopSitesImpl::ResetThreadSafeCache() {
  base::AutoLock lock(lock_);
  MostVisitedURLList cached;
  ApplyBlacklist(cache_->top_sites(), &cached);
  thread_safe_cache_->SetTopSites(cached);
}

void TopSitesImpl::ResetThreadSafeImageCache() {
  base::AutoLock lock(lock_);
  thread_safe_cache_->SetThumbnails(cache_->images());
}

void TopSitesImpl::RestartQueryForTopSitesTimer(base::TimeDelta delta) {
  if (timer_.IsRunning() && ((timer_start_time_ + timer_.GetCurrentDelay()) <
                             (base::TimeTicks::Now() + delta))) {
    return;
  }

  timer_start_time_ = base::TimeTicks::Now();
  timer_.Stop();
  timer_.Start(FROM_HERE, delta, this, &TopSitesImpl::TimerFired);
}

void TopSitesImpl::OnGotMostVisitedThumbnails(
    const scoped_refptr<MostVisitedThumbnails>& thumbnails) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
}

void TopSitesImpl::OnTopSitesAvailableFromHistory(
    const MostVisitedURLList* pages) {
  DCHECK(pages);
  SetTopSites(*pages);
}

}  // namespace history
