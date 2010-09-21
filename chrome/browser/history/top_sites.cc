// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/most_visited_handler.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/top_sites_database.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/thumbnail_score.h"
#include "gfx/codec/jpeg_codec.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace history {

// How many top sites to store in the cache.
static const size_t kTopSitesNumber = 20;
static const size_t kTopSitesShown = 8;
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
                                       waiting_for_results_(true),
                                       blacklist_(NULL),
                                       pinned_urls_(NULL) {
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

// static
bool TopSites::IsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableTopSites);
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
  MostVisitedURLList top_urls;
  db_->GetPageThumbnails(&top_urls, &thumbnails);
  MostVisitedURLList copy(top_urls);  // StoreMostVisited destroys the list.
  StoreMostVisited(&top_urls);
  if (AddPrepopulatedPages(&copy))
    UpdateMostVisited(copy);

  AutoLock lock(lock_);
  for (size_t i = 0; i < top_sites_.size(); i++) {
    GURL url = top_sites_[i].url;
    Images thumbnail = thumbnails[url];
    if (thumbnail.thumbnail.get() && thumbnail.thumbnail->size()) {
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
  AutoLock lock(lock_);
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

  return SetPageThumbnailEncoded(url, thumbnail_data, score);
}

bool TopSites::SetPageThumbnailEncoded(const GURL& url,
                                       const RefCountedBytes* thumbnail,
                                       const ThumbnailScore& score) {
  lock_.AssertAcquired();
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
                                  const Images& thumbnail) {
  DCHECK(db_.get());
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  db_->SetPageThumbnail(url, url_rank, thumbnail);
}

// private
bool TopSites::SetPageThumbnailNoDB(const GURL& url,
                                    const RefCountedBytes* thumbnail_data,
                                    const ThumbnailScore& score) {
  lock_.AssertAcquired();

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
  MostVisitedURLList filtered_urls;
  {
    AutoLock lock(lock_);
    if (waiting_for_results_) {
      // A request came in before we have any top sites.
      // We have to keep track of the requests ourselves.
      pending_callbacks_.insert(request);
      return;
    }
    if (request->canceled())
      return;

    ApplyBlacklistAndPinnedURLs(top_sites_, &filtered_urls);
  }
  request->ForwardResult(GetTopSitesCallback::TupleType(filtered_urls));
}

bool TopSites::GetPageThumbnail(const GURL& url, RefCountedBytes** data) const {
  AutoLock lock(lock_);
  std::map<GURL, Images>::const_iterator found =
      top_images_.find(GetCanonicalURL(url));
  if (found == top_images_.end()) {
    found = temp_thumbnails_map_.find(url);
    if (found == temp_thumbnails_map_.end())
      return false;  // No thumbnail for this URL.
  }

  Images image = found->second;
  *data = image.thumbnail.get();
  return true;
}

static int IndexOf(const MostVisitedURLList& urls, const GURL& url) {
  for (size_t i = 0; i < urls.size(); i++) {
    if (urls[i].url == url)
      return i;
  }
  return -1;
}

bool TopSites::AddPrepopulatedPages(MostVisitedURLList* urls) {
  // TODO(arv): This needs to get the data from some configurable place.
  // http://crbug.com/17630
  bool added = false;
  GURL welcome_url(l10n_util::GetStringUTF8(IDS_CHROME_WELCOME_URL));
  if (urls->size() < kTopSitesNumber && IndexOf(*urls, welcome_url) == -1) {
    MostVisitedURL url = {
      welcome_url,
      GURL("chrome://theme/IDR_NEWTAB_CHROME_WELCOME_PAGE_FAVICON"),
      l10n_util::GetStringUTF16(IDS_NEW_TAB_CHROME_WELCOME_PAGE_TITLE)
    };
    url.redirects.push_back(welcome_url);
    urls->push_back(url);
    added = true;
  }

  GURL themes_url(l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL));
  if (urls->size() < kTopSitesNumber && IndexOf(*urls, themes_url) == -1) {
    MostVisitedURL url = {
      themes_url,
      GURL("chrome://theme/IDR_NEWTAB_THEMES_GALLERY_FAVICON"),
      l10n_util::GetStringUTF16(IDS_NEW_TAB_THEMES_GALLERY_PAGE_TITLE)
    };
    url.redirects.push_back(themes_url);
    urls->push_back(url);
    added = true;
  }

  return added;
}

void TopSites::MigratePinnedURLs() {
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
  lock_.AssertAcquired();
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
  lock_.AssertAcquired();
  return GetCanonicalURL(url).spec();
}

std::string TopSites::GetURLHash(const GURL& url) {
  lock_.AssertAcquired();
  // We don't use canonical URLs here to be able to blacklist only one of
  // the two 'duplicate' sites, e.g. 'gmail.com' and 'mail.google.com'.
  return MD5String(url.spec());
}

void TopSites::UpdateMostVisited(MostVisitedURLList most_visited) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));

  std::vector<size_t> added;    // Indices into most_visited.
  std::vector<size_t> deleted;  // Indices into top_sites_.
  std::vector<size_t> moved;    // Indices into most_visited.

  DiffMostVisited(top_sites_, most_visited, &added, &deleted, &moved);

  // #added == #deleted; #added + #moved = total.
  last_num_urls_changed_ = added.size() + moved.size();

  {
    AutoLock lock(lock_);

    // Process the diff: delete from images and disk, add to disk.
    // Delete all the thumbnails associated with URLs that were deleted.
    for (size_t i = 0; i < deleted.size(); i++) {
      const MostVisitedURL& deleted_url = top_sites_[deleted[i]];
      std::map<GURL, Images>::iterator found =
          top_images_.find(deleted_url.url);
      if (found != top_images_.end())
        top_images_.erase(found);
    }
  }

  // Write the updates to the DB.
  if (db_.get()) {
    for (size_t i = 0; i < deleted.size(); i++) {
      const MostVisitedURL& deleted_url = top_sites_[deleted[i]];
      if (db_.get())
        db_->RemoveURL(deleted_url);
    }
    for (size_t i = 0; i < added.size(); i++) {
      const MostVisitedURL& added_url = most_visited[added[i]];
      db_->SetPageThumbnail(added_url, added[i], Images());
    }
    for (size_t i = 0; i < moved.size(); i++) {
      const MostVisitedURL& moved_url = most_visited[moved[i]];
      db_->UpdatePageRank(moved_url, moved[i]);
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
  if (!profile_)
    return;

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
  if (top_sites_[index].url.spec() ==
      l10n_util::GetStringUTF8(IDS_CHROME_WELCOME_URL) ||
      top_sites_[index].url.spec() ==
      l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL))
    return;  // Don't need thumbnails for prepopulated URLs.

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

  if (!profile_)
    return;

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

void TopSites::GenerateCanonicalURLs() {
  lock_.AssertAcquired();
  canonical_urls_.clear();
  for (size_t i = 0; i < top_sites_.size(); i++) {
    const MostVisitedURL& mv = top_sites_[i];
    StoreRedirectChain(mv.redirects, i);
  }
}

void TopSites::StoreMostVisited(MostVisitedURLList* most_visited) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  MostVisitedURLList filtered_urls;
  PendingCallbackSet callbacks;
  {
    AutoLock lock(lock_);
    top_sites_.clear();
    // Take ownership of the most visited data.
    top_sites_.swap(*most_visited);
    waiting_for_results_ = false;

    // Save the redirect information for quickly mapping to the canonical URLs.
    GenerateCanonicalURLs();

    for (size_t i = 0; i < top_sites_.size(); i++) {
      const MostVisitedURL& mv = top_sites_[i];
      std::map<GURL, Images>::iterator it = temp_thumbnails_map_.begin();
      GURL canonical_url = GetCanonicalURL(mv.url);
      for (; it != temp_thumbnails_map_.end(); it++) {
        // Must map all temp URLs to canonical ones.
        // temp_thumbnails_map_ contains non-canonical URLs, because
        // when we add a temp thumbnail, redirect chain is not known.
        // This is slow, but temp_thumbnails_map_ should have very few URLs.
        if (canonical_url == GetCanonicalURL(it->first)) {
          SetPageThumbnailEncoded(mv.url, it->second.thumbnail,
                                  it->second.thumbnail_score);
          temp_thumbnails_map_.erase(it);
          break;
        }
      }
    }
    if (top_sites_.size() >= kTopSitesNumber)
      temp_thumbnails_map_.clear();

    if (pending_callbacks_.empty())
      return;

    ApplyBlacklistAndPinnedURLs(top_sites_, &filtered_urls);
    callbacks.swap(pending_callbacks_);
  }  // lock_ is released.
  // Process callbacks outside the lock - ForwardResults may cause
  // thread switches.
  ProcessPendingCallbacks(callbacks, filtered_urls);
}

void TopSites::StoreRedirectChain(const RedirectList& redirects,
                                  size_t destination) {
  lock_.AssertAcquired();
  if (redirects.empty()) {
    NOTREACHED();
    return;
  }

  // Map all the redirected URLs to the destination.
  for (size_t i = 0; i < redirects.size(); i++) {
    // If this redirect is already known, don't replace it with a new one.
    if (canonical_urls_.find(redirects[i]) == canonical_urls_.end())
      canonical_urls_[redirects[i]] = destination;
  }
}

GURL TopSites::GetCanonicalURL(const GURL& url) const {
  lock_.AssertAcquired();
  std::map<GURL, size_t>::const_iterator found = canonical_urls_.find(url);
  if (found == canonical_urls_.end())
    return url;  // Unknown URL - return unchanged.
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
        kTopSitesNumber + blacklist_->size(),
        kDaysOfHistory,
        &cancelable_consumer_,
        NewCallback(this, &TopSites::OnTopSitesAvailable));
  } else {
    if (!profile_)
      return;

    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    // |hs| may be null during unit tests.
    if (hs) {
      hs->QueryMostVisitedURLs(
          kTopSitesNumber + blacklist_->size(),
          kDaysOfHistory,
          &cancelable_consumer_,
          NewCallback(this, &TopSites::OnTopSitesAvailable));
    } else {
      LOG(INFO) << "History Service not available.";
    }
  }
}

void TopSites::StartMigration() {
  LOG(INFO) << "Starting migration to TopSites.";
  migration_in_progress_ = true;
  StartQueryForMostVisited();
  MigratePinnedURLs();
}

bool TopSites::HasBlacklistedItems() const {
  AutoLock lock(lock_);
  return !blacklist_->empty();
}

void TopSites::AddBlacklistedURL(const GURL& url) {
  AutoLock lock(lock_);
  RemovePinnedURLLocked(url);
  Value* dummy = Value::CreateNullValue();
  blacklist_->SetWithoutPathExpansion(GetURLHash(url), dummy);
}

bool TopSites::IsBlacklisted(const GURL& url) {
  lock_.AssertAcquired();
  bool result = blacklist_->HasKey(GetURLHash(url));
  return result;
}

void TopSites::RemoveBlacklistedURL(const GURL& url) {
  AutoLock lock(lock_);
  blacklist_->RemoveWithoutPathExpansion(GetURLHash(url), NULL);
}

void TopSites::ClearBlacklistedURLs() {
  blacklist_->Clear();
}

void TopSites::AddPinnedURL(const GURL& url, size_t pinned_index) {
  GURL old;
  if (GetPinnedURLAtIndex(pinned_index, &old)) {
    RemovePinnedURL(old);
  }

  if (IsURLPinned(url)) {
    RemovePinnedURL(url);
  }

  Value* index = Value::CreateIntegerValue(pinned_index);
  AutoLock lock(lock_);
  pinned_urls_->SetWithoutPathExpansion(GetURLString(url), index);
}

void TopSites::RemovePinnedURL(const GURL& url) {
  AutoLock lock(lock_);
  RemovePinnedURLLocked(url);
}

void TopSites::RemovePinnedURLLocked(const GURL& url) {
  lock_.AssertAcquired();
  pinned_urls_->RemoveWithoutPathExpansion(GetURLString(url), NULL);
}

bool TopSites::IsURLPinned(const GURL& url) {
  AutoLock lock(lock_);
  int tmp;
  bool result = pinned_urls_->GetIntegerWithoutPathExpansion(
      GetURLString(url), &tmp);
  return result;
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

// static
void TopSites::DeleteTopSites(scoped_refptr<TopSites>& ptr) {
  if (!ptr.get() || !MessageLoop::current())
    return;
  if (ChromeThread::IsWellKnownThread(ChromeThread::UI)) {
    ptr = NULL;
  } else {
    // Need to roll our own UI thread.
    ChromeThread ui_loop(ChromeThread::UI, MessageLoop::current());
    ptr = NULL;
    MessageLoop::current()->RunAllPending();
  }
}

void TopSites::ClearProfile() {
  profile_ = NULL;
}

base::TimeDelta TopSites::GetUpdateDelay() {
  AutoLock lock(lock_);
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
  AddPrepopulatedPages(&pages);
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, NewRunnableMethod(
      this, &TopSites::UpdateMostVisited, pages));
}

// static
void TopSites::ProcessPendingCallbacks(PendingCallbackSet pending_callbacks,
                                       const MostVisitedURLList& urls) {
  PendingCallbackSet::iterator i;
  for (i = pending_callbacks.begin();
       i != pending_callbacks.end(); ++i) {
    scoped_refptr<CancelableRequest<GetTopSitesCallback> > request = *i;
    if (!request->canceled())
      request->ForwardResult(GetTopSitesCallback::TupleType(urls));
  }
  pending_callbacks.clear();
}

void TopSites::OnThumbnailAvailable(CancelableRequestProvider::Handle handle,
                                    scoped_refptr<RefCountedBytes> thumbnail) {
  size_t index;
  if (mock_history_service_) {
    index = handle;
  } else {
    if (!profile_)
      return;

    HistoryService* hs = profile_ ->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (!hs)
      return;
    index = cancelable_consumer_.GetClientData(hs, handle);
  }
  DCHECK(static_cast<size_t>(index) < top_sites_.size());

  if (migration_in_progress_)
    migration_pending_urls_.erase(top_sites_[index].url);

  if (thumbnail.get() && thumbnail->size()) {
    const MostVisitedURL& url = top_sites_[index];
    AutoLock lock(lock_);
    SetPageThumbnailEncoded(url.url, thumbnail, ThumbnailScore());
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
  AutoLock lock(lock_);
  if (type == NotificationType::HISTORY_URLS_DELETED) {
    Details<history::URLsDeletedDetails> deleted_details(details);
    if (deleted_details->all_history) {
      top_sites_.clear();
      ChromeThread::PostTask(ChromeThread::DB, FROM_HERE,
                             NewRunnableMethod(this, &TopSites::ResetDatabase));
    } else {
      std::set<size_t> indices_to_delete;  // Indices into top_sites_.
      std::set<GURL>::iterator it;
      for (it = deleted_details->urls.begin();
           it != deleted_details->urls.end(); ++it) {
        std::map<GURL,size_t>::const_iterator found = canonical_urls_.find(*it);
        if (found != canonical_urls_.end())
          indices_to_delete.insert(found->second);
      }

      for (std::set<size_t>::reverse_iterator i = indices_to_delete.rbegin();
           i != indices_to_delete.rend(); i++) {
        size_t index = *i;
        RemovePinnedURLLocked(top_sites_[index].url);
        top_sites_.erase(top_sites_.begin() + index);
      }
    }
    // Canonical URLs are not valid any more.
    GenerateCanonicalURLs();
    StartQueryForMostVisited();
  } else if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    if (top_sites_.size() < kTopSitesNumber) {
      NavigationController::LoadCommittedDetails* load_details =
          Details<NavigationController::LoadCommittedDetails>(details).ptr();
      if (!load_details)
        return;
      GURL url = load_details->entry->url();
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
