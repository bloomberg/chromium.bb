// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/most_visited_sites.h"

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search/suggestions/suggestions_source.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "components/suggestions/suggestions_service.h"
#include "components/suggestions/suggestions_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/url_data_source.h"
#include "jni/MostVisitedSites_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::CheckException;
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
// Control group suggestion opened.
const char kOpenedItemControlHistogramName[] = "NewTabPage.MostVisited.client0";
// Server suggestion opened, no provider.
const char kOpenedItemServerHistogramName[] = "NewTabPage.MostVisited.server";
// Server suggestion opened with provider.
const char kOpenedItemServerProviderHistogramFormat[] =
    "NewTabPage.MostVisited.server%d";
// Client impression.
const char kImpressionClientHistogramName[] =
    "NewTabPage.SuggestionsImpression.client";
// Control group impression.
const char kImpressionControlHistogramName[] =
    "NewTabPage.SuggestionsImpression.client0";
// Server suggestion impression, no provider.
const char kImpressionServerHistogramName[] =
    "NewTabPage.SuggestionsImpression.server";
// Server suggestion impression with provider.
const char kImpressionServerHistogramFormat[] =
    "NewTabPage.SuggestionsImpression.server%d";

void ExtractMostVisitedTitlesAndURLs(
    const history::MostVisitedURLList& visited_list,
    std::vector<base::string16>* titles,
    std::vector<std::string>* urls,
    int num_sites) {
  size_t max = static_cast<size_t>(num_sites);
  for (size_t i = 0; i < visited_list.size() && i < max; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];

    if (visited.url.is_empty())
      break;  // This is the signal that there are no more real visited sites.

    titles->push_back(visited.title);
    urls->push_back(visited.url.spec());
  }
}

SkBitmap ExtractThumbnail(const base::RefCountedMemory& image_data) {
  scoped_ptr<SkBitmap> image(gfx::JPEGCodec::Decode(
      image_data.front(),
      image_data.size()));
  return image.get() ? *image : SkBitmap();
}

void AddForcedURLOnUIThread(scoped_refptr<history::TopSites> top_sites,
                            const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  top_sites->AddForcedURL(url, base::Time::Now());
}

// Runs on the DB thread.
void GetUrlThumbnailTask(
    std::string url_string,
    scoped_refptr<TopSites> top_sites,
    ScopedJavaGlobalRef<jobject>* j_callback,
    MostVisitedSites::LookupSuccessCallback lookup_success_ui_callback,
    base::Closure lookup_failed_ui_callback) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaGlobalRef<jobject>* j_bitmap_ref =
      new ScopedJavaGlobalRef<jobject>();

  GURL gurl(url_string);

  scoped_refptr<base::RefCountedMemory> data;
  if (top_sites->GetPageThumbnail(gurl, false, &data)) {
    SkBitmap thumbnail_bitmap = ExtractThumbnail(*data.get());
    if (!thumbnail_bitmap.empty()) {
      j_bitmap_ref->Reset(
          env,
          gfx::ConvertToJavaBitmap(&thumbnail_bitmap).obj());
    }
  } else {
    // A thumbnail is not locally available for |gurl|. Make sure it is put in
    // the list to be fetched at the next visit to this site.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(AddForcedURLOnUIThread, top_sites, gurl));

    // If appropriate, return on the UI thread to execute the proper callback.
    if (!lookup_failed_ui_callback.is_null()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, lookup_failed_ui_callback);
      delete j_bitmap_ref;
      return;
    }
  }

  // Since j_callback is owned by this callback, when the callback falls out of
  // scope it will be deleted. We need to pass ownership to the next callback.
  ScopedJavaGlobalRef<jobject>* j_callback_pass =
      new ScopedJavaGlobalRef<jobject>(*j_callback);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(lookup_success_ui_callback, base::Owned(j_bitmap_ref),
                 base::Owned(j_callback_pass)));
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
      sync->IsSyncEnabledAndLoggedIn(),
      sync->sync_initialized(),
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES));
}

}  // namespace

MostVisitedSites::MostVisitedSites(Profile* profile)
    : profile_(profile), num_sites_(0), is_control_group_(false),
      num_local_thumbs_(0), num_server_thumbs_(0), num_empty_thumbs_(0),
      weak_ptr_factory_(this) {
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
}

MostVisitedSites::~MostVisitedSites() {
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

  history::TopSites* top_sites = profile_->GetTopSites();
  if (top_sites) {
    // TopSites updates itself after a delay. To ensure up-to-date results,
    // force an update now.
    top_sites->SyncWithHistory();

    // Register for notification when TopSites changes so that we can update
    // ourself.
    registrar_.Add(this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
                   content::Source<history::TopSites>(top_sites));
  }
}

// Called from the UI Thread.
void MostVisitedSites::GetURLThumbnail(JNIEnv* env,
                                       jobject obj,
                                       jstring url,
                                       jobject j_callback_obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ScopedJavaGlobalRef<jobject>* j_callback =
      new ScopedJavaGlobalRef<jobject>();
  j_callback->Reset(env, j_callback_obj);

  std::string url_string = ConvertJavaStringToUTF8(env, url);
  scoped_refptr<TopSites> top_sites(profile_->GetTopSites());

  // If the Suggestions service is enabled and in use, create a callback to
  // fetch a server thumbnail from it, in case the local thumbnail is not found.
  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForProfile(profile_);
  bool use_suggestions_service = suggestions_service &&
      mv_source_ == SUGGESTIONS_SERVICE;
  base::Closure lookup_failed_callback = use_suggestions_service ?
      base::Bind(&MostVisitedSites::GetSuggestionsThumbnailOnUIThread,
                 weak_ptr_factory_.GetWeakPtr(),
                 suggestions_service, url_string,
                 base::Owned(new ScopedJavaGlobalRef<jobject>(*j_callback))) :
      base::Closure();
  LookupSuccessCallback lookup_success_callback =
      base::Bind(&MostVisitedSites::OnObtainedThumbnail,
                 weak_ptr_factory_.GetWeakPtr());

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
          base::Bind(
              &GetUrlThumbnailTask, url_string, top_sites,
              base::Owned(j_callback), lookup_success_callback,
              lookup_failed_callback));
}

void MostVisitedSites::BlacklistUrl(JNIEnv* env,
                                    jobject obj,
                                    jstring j_url) {
  std::string url = ConvertJavaStringToUTF8(env, j_url);

  switch (mv_source_) {
    case TOP_SITES: {
      TopSites* top_sites = profile_->GetTopSites();
      DCHECK(top_sites);
      top_sites->AddBlacklistedURL(GURL(url));
      break;
    }

    case SUGGESTIONS_SERVICE: {
      SuggestionsService* suggestions_service =
          SuggestionsServiceFactory::GetForProfile(profile_);
      DCHECK(suggestions_service);
      suggestions_service->BlacklistURL(
          GURL(url),
          base::Bind(
              &MostVisitedSites::OnSuggestionsProfileAvailable,
              weak_ptr_factory_.GetWeakPtr(),
              base::Owned(new ScopedJavaGlobalRef<jobject>(observer_))));
      break;
    }
  }
}

void MostVisitedSites::RecordOpenedMostVisitedItem(JNIEnv* env,
                                                   jobject obj,
                                                   jint index) {
  switch (mv_source_) {
    case TOP_SITES: {
      const std::string histogram = is_control_group_ ?
          kOpenedItemControlHistogramName : kOpenedItemClientHistogramName;
      LogHistogramEvent(histogram, index, num_sites_);
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

void MostVisitedSites::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOP_SITES_CHANGED);

  if (mv_source_ == TOP_SITES) {
    // The displayed suggestions are invalidated.
    QueryMostVisitedURLs();
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
        base::Bind(
          &MostVisitedSites::OnSuggestionsProfileAvailable,
          weak_ptr_factory_.GetWeakPtr(),
          base::Owned(new ScopedJavaGlobalRef<jobject>(observer_))));
  } else {
    InitiateTopSitesQuery();
  }
}

void MostVisitedSites::InitiateTopSitesQuery() {
  TopSites* top_sites = profile_->GetTopSites();
  if (!top_sites)
    return;

  top_sites->GetMostVisitedURLs(
      base::Bind(
          &MostVisitedSites::OnMostVisitedURLsAvailable,
          weak_ptr_factory_.GetWeakPtr(),
          base::Owned(new ScopedJavaGlobalRef<jobject>(observer_)),
          num_sites_),
      false);
}

void MostVisitedSites::OnMostVisitedURLsAvailable(
    ScopedJavaGlobalRef<jobject>* j_observer,
    int num_sites,
    const history::MostVisitedURLList& visited_list) {
  std::vector<base::string16> titles;
  std::vector<std::string> urls;
  ExtractMostVisitedTitlesAndURLs(visited_list, &titles, &urls, num_sites);

  mv_source_ = TOP_SITES;

  int num_tiles = urls.size();
  UMA_HISTOGRAM_SPARSE_SLOWLY(kNumTilesHistogramName, num_tiles);
  const std::string histogram = is_control_group_ ?
      kImpressionControlHistogramName : kImpressionClientHistogramName;
  for (int i = 0; i < num_tiles; ++i) {
    LogHistogramEvent(histogram, i, num_sites_);
  }

  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedURLsObserver_onMostVisitedURLsAvailable(
      env,
      j_observer->obj(),
      ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj());
}

void MostVisitedSites::OnSuggestionsProfileAvailable(
    ScopedJavaGlobalRef<jobject>* j_observer,
    const SuggestionsProfile& suggestions_profile) {
  int size = suggestions_profile.suggestions_size();

  // Determine if the user is in a control group (they would have received
  // suggestions, but are in a group where they shouldn't).
  is_control_group_ = size && SuggestionsService::IsControlGroup();

  // If no suggestions data is available or the user is in a control group,
  // initiate Top Sites query.
  if (is_control_group_ || !size) {
    InitiateTopSitesQuery();
    return;
  }

  std::vector<base::string16> titles;
  std::vector<std::string> urls;

  int i = 0;
  for (; i < size && i < num_sites_; ++i) {
    const ChromeSuggestion& suggestion = suggestions_profile.suggestions(i);
    titles.push_back(base::UTF8ToUTF16(suggestion.title()));
    urls.push_back(suggestion.url());
    if (suggestion.providers_size()) {
      std::string histogram = base::StringPrintf(
          kImpressionServerHistogramFormat, suggestion.providers(0));
      LogHistogramEvent(histogram, i, num_sites_);
    } else {
      UMA_HISTOGRAM_SPARSE_SLOWLY(kImpressionServerHistogramName, i);
    }
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY(kNumTilesHistogramName, i);

  mv_source_ = SUGGESTIONS_SERVICE;
  // Keep a copy of the suggestions for eventual logging.
  server_suggestions_ = suggestions_profile;

  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedURLsObserver_onMostVisitedURLsAvailable(
      env,
      j_observer->obj(),
      ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj());
}

void MostVisitedSites::OnObtainedThumbnail(
    ScopedJavaGlobalRef<jobject>* bitmap,
    ScopedJavaGlobalRef<jobject>* j_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  if (bitmap->obj()) {
    num_local_thumbs_++;
  } else {
    num_empty_thumbs_++;
  }
  Java_ThumbnailCallback_onMostVisitedURLsThumbnailAvailable(
      env, j_callback->obj(), bitmap->obj());
}

void MostVisitedSites::GetSuggestionsThumbnailOnUIThread(
    SuggestionsService* suggestions_service,
    const std::string& url_string,
    ScopedJavaGlobalRef<jobject>* j_callback) {
  suggestions_service->GetPageThumbnail(
      GURL(url_string),
      base::Bind(&MostVisitedSites::OnSuggestionsThumbnailAvailable,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(new ScopedJavaGlobalRef<jobject>(*j_callback))));
}

void MostVisitedSites::OnSuggestionsThumbnailAvailable(
    ScopedJavaGlobalRef<jobject>* j_callback,
    const GURL& url,
    const SkBitmap* bitmap) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaGlobalRef<jobject>* j_bitmap_ref =
      new ScopedJavaGlobalRef<jobject>();
  if (bitmap) {
    num_server_thumbs_++;
    j_bitmap_ref->Reset(
        env,
        gfx::ConvertToJavaBitmap(bitmap).obj());
  } else {
    num_empty_thumbs_++;
  }

  Java_ThumbnailCallback_onMostVisitedURLsThumbnailAvailable(
      env, j_callback->obj(), j_bitmap_ref->obj());
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

static jlong Init(JNIEnv* env, jobject obj, jobject jprofile) {
  MostVisitedSites* most_visited_sites =
      new MostVisitedSites(ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(most_visited_sites);
}
