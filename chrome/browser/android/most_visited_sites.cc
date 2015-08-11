// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/most_visited_sites.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/popular_sites.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search/suggestions/suggestions_source.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "chrome/common/chrome_switches.h"
#include "components/history/core/browser/top_sites.h"
#include "components/suggestions/suggestions_service.h"
#include "components/suggestions/suggestions_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "jni/MostVisitedSites_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::BrowserThread;
using history::TopSites;
using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;
using suggestions::SuggestionsServiceFactory;
using suggestions::SyncState;

namespace {

// Total number of tiles displayed.
const char kNumTilesHistogramName[] = "NewTabPage.NumberOfTiles";
// Tracking thumbnails.
const char kNumLocalThumbnailTilesHistogramName[] =
    "NewTabPage.NumberOfThumbnailTiles";
const char kNumEmptyTilesHistogramName[] = "NewTabPage.NumberOfGrayTiles";
const char kNumServerTilesHistogramName[] = "NewTabPage.NumberOfExternalTiles";
// Client suggestion opened.
const char kOpenedItemClientHistogramName[] = "NewTabPage.MostVisited.client";
// Server suggestion opened, no provider.
const char kOpenedItemServerHistogramName[] = "NewTabPage.MostVisited.server";
// Server suggestion opened with provider.
const char kOpenedItemServerProviderHistogramFormat[] =
    "NewTabPage.MostVisited.server%d";
// Client impression.
const char kImpressionClientHistogramName[] =
    "NewTabPage.SuggestionsImpression.client";
// Server suggestion impression, no provider.
const char kImpressionServerHistogramName[] =
    "NewTabPage.SuggestionsImpression.server";
// Server suggestion impression with provider.
const char kImpressionServerHistogramFormat[] =
    "NewTabPage.SuggestionsImpression.server%d";

scoped_ptr<SkBitmap> MaybeFetchLocalThumbnail(
    const GURL& url,
    const scoped_refptr<TopSites>& top_sites) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  scoped_refptr<base::RefCountedMemory> image;
  scoped_ptr<SkBitmap> bitmap;
  if (top_sites && top_sites->GetPageThumbnail(url, false, &image))
    bitmap.reset(gfx::JPEGCodec::Decode(image->front(), image->size()));
  return bitmap.Pass();
}

// Log an event for a given |histogram| at a given element |position|. This
// routine exists because regular histogram macros are cached thus can't be used
// if the name of the histogram will change at a given call site.
void LogHistogramEvent(const std::string& histogram, int position,
                       int num_sites) {
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram,
      1,
      num_sites,
      num_sites + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  if (counter)
    counter->Add(position);
}

// Return the current SyncState for use with the SuggestionsService.
SyncState GetSyncState(Profile* profile) {
  ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (!sync)
    return SyncState::SYNC_OR_HISTORY_SYNC_DISABLED;
  return suggestions::GetSyncState(
      sync->CanSyncStart(),
      sync->IsSyncActive() && sync->ConfigurationDone(),
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES));
}

bool ShouldShowPopularSites() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName("NTPPopularSites");
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableNTPPopularSites))
    return false;
  if (cmd_line->HasSwitch(switches::kEnableNTPPopularSites))
    return true;
  return group_name == "Enabled";
}

}  // namespace

MostVisitedSites::MostVisitedSites(Profile* profile)
    : profile_(profile), num_sites_(0), initial_load_done_(false),
      num_local_thumbs_(0), num_server_thumbs_(0), num_empty_thumbs_(0),
      scoped_observer_(this), weak_ptr_factory_(this) {
  // Register the debugging page for the Suggestions Service and the thumbnails
  // debugging page.
  content::URLDataSource::Add(profile_,
                              new suggestions::SuggestionsSource(profile_));
  content::URLDataSource::Add(profile_, new ThumbnailListSource(profile_));

  // Register this class as an observer to the sync service. It is important to
  // be notified of changes in the sync state such as initialization, sync
  // being enabled or disabled, etc.
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (profile_sync_service)
    profile_sync_service->AddObserver(this);

  if (ShouldShowPopularSites()) {
    popular_sites_.reset(new PopularSites(
        profile_->GetRequestContext(),
        base::Bind(&MostVisitedSites::OnPopularSitesAvailable,
                   base::Unretained(this))));
  }
}

MostVisitedSites::~MostVisitedSites() {
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (profile_sync_service && profile_sync_service->HasObserver(this))
    profile_sync_service->RemoveObserver(this);
}

void MostVisitedSites::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void MostVisitedSites::OnLoadingComplete(JNIEnv* env, jobject obj) {
  RecordUMAMetrics();
}

void MostVisitedSites::SetMostVisitedURLsObserver(JNIEnv* env,
                                                  jobject obj,
                                                  jobject j_observer,
                                                  jint num_sites) {
  observer_.Reset(env, j_observer);
  num_sites_ = num_sites;

  QueryMostVisitedURLs();

  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites) {
    // TopSites updates itself after a delay. To ensure up-to-date results,
    // force an update now.
    top_sites->SyncWithHistory();

    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    scoped_observer_.Add(top_sites.get());
  }
}

void MostVisitedSites::GetURLThumbnail(JNIEnv* env,
                                       jobject obj,
                                       jstring j_url,
                                       jobject j_callback_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_ptr<ScopedJavaGlobalRef<jobject>> j_callback(
      new ScopedJavaGlobalRef<jobject>());
  j_callback->Reset(env, j_callback_obj);

  GURL url(ConvertJavaStringToUTF8(env, j_url));
  scoped_refptr<TopSites> top_sites(TopSitesFactory::GetForProfile(profile_));

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&MaybeFetchLocalThumbnail, url, top_sites),
      base::Bind(&MostVisitedSites::OnLocalThumbnailFetched,
                 weak_ptr_factory_.GetWeakPtr(), url,
                 base::Passed(&j_callback)));
}

void MostVisitedSites::OnLocalThumbnailFetched(
    const GURL& url,
    scoped_ptr<ScopedJavaGlobalRef<jobject>> j_callback,
    scoped_ptr<SkBitmap> bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!bitmap.get()) {
    // A thumbnail is not locally available for |url|. Make sure it is put in
    // the list to be fetched at the next visit to this site.
    scoped_refptr<TopSites> top_sites(TopSitesFactory::GetForProfile(profile_));
    if (top_sites)
      top_sites->AddForcedURL(url, base::Time::Now());
    SuggestionsService* suggestions_service =
        (mv_source_ == SUGGESTIONS_SERVICE)
            ? SuggestionsServiceFactory::GetForProfile(profile_)
            : nullptr;
    if (suggestions_service) {
      return suggestions_service->GetPageThumbnail(
          url, base::Bind(&MostVisitedSites::OnObtainedThumbnail,
                          weak_ptr_factory_.GetWeakPtr(), false,
                          base::Passed(&j_callback)));
    }
  }
  OnObtainedThumbnail(true, j_callback.Pass(), url, bitmap.get());
}

void MostVisitedSites::OnObtainedThumbnail(
    bool is_local_thumbnail,
    scoped_ptr<ScopedJavaGlobalRef<jobject>> j_callback,
    const GURL& url,
    const SkBitmap* bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (bitmap) {
    j_bitmap = gfx::ConvertToJavaBitmap(bitmap);
    if (is_local_thumbnail) {
      ++num_local_thumbs_;
    } else {
      ++num_server_thumbs_;
    }
  } else {
    ++num_empty_thumbs_;
  }
  Java_ThumbnailCallback_onMostVisitedURLsThumbnailAvailable(
      env, j_callback->obj(), j_bitmap.obj());
}

void MostVisitedSites::BlacklistUrl(JNIEnv* env,
                                    jobject obj,
                                    jstring j_url) {
  GURL url(ConvertJavaStringToUTF8(env, j_url));

  switch (mv_source_) {
    case TOP_SITES: {
      scoped_refptr<TopSites> top_sites =
          TopSitesFactory::GetForProfile(profile_);
      DCHECK(top_sites);
      top_sites->AddBlacklistedURL(url);
      break;
    }

    case SUGGESTIONS_SERVICE: {
      SuggestionsService* suggestions_service =
          SuggestionsServiceFactory::GetForProfile(profile_);
      DCHECK(suggestions_service);
      suggestions_service->BlacklistURL(
          url, base::Bind(&MostVisitedSites::OnSuggestionsProfileAvailable,
                          weak_ptr_factory_.GetWeakPtr()),
          base::Closure());
      break;
    }
  }
}

void MostVisitedSites::RecordOpenedMostVisitedItem(JNIEnv* env,
                                                   jobject obj,
                                                   jint index) {
  switch (mv_source_) {
    case TOP_SITES: {
      UMA_HISTOGRAM_SPARSE_SLOWLY(kOpenedItemClientHistogramName, index);
      break;
    }
    case SUGGESTIONS_SERVICE: {
      if (server_suggestions_.suggestions_size() > index) {
        if (server_suggestions_.suggestions(index).providers_size()) {
          std::string histogram = base::StringPrintf(
              kOpenedItemServerProviderHistogramFormat,
              server_suggestions_.suggestions(index).providers(0));
          LogHistogramEvent(histogram, index, num_sites_);
        } else {
          UMA_HISTOGRAM_SPARSE_SLOWLY(kOpenedItemServerHistogramName, index);
        }
      }
      break;
    }
  }
}

void MostVisitedSites::OnStateChanged() {
  // There have been changes to the sync state. This class cares about a few
  // (just initialized, enabled/disabled or history sync state changed). Re-run
  // the query code which will use the proper state.
  QueryMostVisitedURLs();
}

// static
bool MostVisitedSites::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void MostVisitedSites::QueryMostVisitedURLs() {
  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForProfile(profile_);
  if (suggestions_service) {
    // Suggestions service is enabled, initiate a query.
    suggestions_service->FetchSuggestionsData(
        GetSyncState(profile_),
        base::Bind(&MostVisitedSites::OnSuggestionsProfileAvailable,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    InitiateTopSitesQuery();
  }
}

void MostVisitedSites::InitiateTopSitesQuery() {
  scoped_refptr<TopSites> top_sites = TopSitesFactory::GetForProfile(profile_);
  if (!top_sites)
    return;

  top_sites->GetMostVisitedURLs(
      base::Bind(&MostVisitedSites::OnMostVisitedURLsAvailable,
                 weak_ptr_factory_.GetWeakPtr()),
      false);
}

void MostVisitedSites::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& visited_list) {
  std::vector<base::string16> titles;
  std::vector<std::string> urls;
  int num_tiles = std::min(static_cast<int>(visited_list.size()), num_sites_);
  for (int i = 0; i < num_tiles; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.is_empty()) {
      num_tiles = i;
      break;  // This is the signal that there are no more real visited sites.
    }
    titles.push_back(visited.title);
    urls.push_back(visited.url.spec());
  }

  // Only log impression metrics on the initial load of the NTP.
  if (!initial_load_done_) {
    for (int i = 0; i < num_tiles; ++i) {
      UMA_HISTOGRAM_SPARSE_SLOWLY(kImpressionClientHistogramName, i);
    }
  }
  mv_source_ = TOP_SITES;
  AddPopularSites(&titles, &urls);
  NotifyMostVisitedURLsObserver(titles, urls);
}

void MostVisitedSites::OnSuggestionsProfileAvailable(
    const SuggestionsProfile& suggestions_profile) {
  int num_tiles = suggestions_profile.suggestions_size();
  // With no server suggestions, fall back to local Most Visited.
  if (num_tiles == 0) {
    InitiateTopSitesQuery();
    return;
  }
  if (num_sites_ < num_tiles)
    num_tiles = num_sites_;

  std::vector<base::string16> titles;
  std::vector<std::string> urls;
  for (int i = 0; i < num_tiles; ++i) {
    const ChromeSuggestion& suggestion = suggestions_profile.suggestions(i);
    titles.push_back(base::UTF8ToUTF16(suggestion.title()));
    urls.push_back(suggestion.url());
    // Only log impression metrics on the initial NTP load.
    if (!initial_load_done_) {
      if (suggestion.providers_size()) {
        std::string histogram = base::StringPrintf(
            kImpressionServerHistogramFormat, suggestion.providers(0));
        LogHistogramEvent(histogram, i, num_sites_);
      } else {
        UMA_HISTOGRAM_SPARSE_SLOWLY(kImpressionServerHistogramName, i);
      }
    }
  }
  mv_source_ = SUGGESTIONS_SERVICE;
  // Keep a copy of the suggestions for eventual logging.
  server_suggestions_ = suggestions_profile;
  AddPopularSites(&titles, &urls);
  NotifyMostVisitedURLsObserver(titles, urls);
}

void MostVisitedSites::AddPopularSites(std::vector<base::string16>* titles,
                                       std::vector<std::string>* urls) {
  if (!popular_sites_)
    return;

  DCHECK_EQ(titles->size(), urls->size());
  if (static_cast<int>(titles->size()) >= num_sites_)
    return;

  for (const PopularSites::Site& popular_site : popular_sites_->sites()) {
    // Skip popular sites that are already in the suggestions.
    auto it = std::find_if(urls->begin(), urls->end(),
        [&popular_site](const std::string& url) {
          return GURL(url).host() == popular_site.url.host();
        });
    if (it != urls->end())
      continue;

    titles->push_back(popular_site.title);
    urls->push_back(popular_site.url.spec());
    if (static_cast<int>(titles->size()) >= num_sites_)
      break;
  }
}

void MostVisitedSites::NotifyMostVisitedURLsObserver(
    const std::vector<base::string16>& titles,
    const std::vector<std::string>& urls) {
  DCHECK_EQ(titles.size(), urls.size());
  if (!initial_load_done_)
    UMA_HISTOGRAM_SPARSE_SLOWLY(kNumTilesHistogramName, titles.size());
  initial_load_done_ = true;
  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedURLsObserver_onMostVisitedURLsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj());
}

void MostVisitedSites::OnPopularSitesAvailable(bool success) {
  if (!success) {
    LOG(WARNING) << "Download of popular sites failed";
    return;
  }

  if (observer_.is_null())
    return;

  std::vector<std::string> urls;
  std::vector<std::string> favicon_urls;
  for (const PopularSites::Site& popular_site : popular_sites_->sites()) {
    urls.push_back(popular_site.url.spec());
    favicon_urls.push_back(popular_site.favicon_url.spec());
  }
  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedURLsObserver_onPopularURLsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, urls).obj(),
      ToJavaArrayOfStrings(env, favicon_urls).obj());

  QueryMostVisitedURLs();
}

void MostVisitedSites::RecordUMAMetrics() {
  UMA_HISTOGRAM_SPARSE_SLOWLY(kNumLocalThumbnailTilesHistogramName,
                              num_local_thumbs_);
  num_local_thumbs_ = 0;
  UMA_HISTOGRAM_SPARSE_SLOWLY(kNumEmptyTilesHistogramName, num_empty_thumbs_);
  num_empty_thumbs_ = 0;
  UMA_HISTOGRAM_SPARSE_SLOWLY(kNumServerTilesHistogramName, num_server_thumbs_);
  num_server_thumbs_ = 0;
}

void MostVisitedSites::TopSitesLoaded(history::TopSites* top_sites) {
}

void MostVisitedSites::TopSitesChanged(history::TopSites* top_sites,
                                       ChangeReason change_reason) {
  if (mv_source_ == TOP_SITES) {
    // The displayed suggestions are invalidated.
    QueryMostVisitedURLs();
  }
}

static jlong Init(JNIEnv* env, jobject obj, jobject jprofile) {
  MostVisitedSites* most_visited_sites =
      new MostVisitedSites(ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(most_visited_sites);
}
