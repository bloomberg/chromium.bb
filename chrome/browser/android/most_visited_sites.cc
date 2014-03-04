// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/most_visited_sites.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "chrome/browser/search/suggestions/suggestions_service.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
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

namespace {

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

void OnMostVisitedURLsAvailable(
    ScopedJavaGlobalRef<jobject>* j_observer,
    int num_sites,
    const history::MostVisitedURLList& visited_list) {
  std::vector<base::string16> titles;
  std::vector<std::string> urls;
  ExtractMostVisitedTitlesAndURLs(visited_list, &titles, &urls, num_sites);

  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedURLsObserver_onMostVisitedURLsAvailable(
      env,
      j_observer->obj(),
      ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj());
}

SkBitmap ExtractThumbnail(const base::RefCountedMemory& image_data) {
  scoped_ptr<SkBitmap> image(gfx::JPEGCodec::Decode(
      image_data.front(),
      image_data.size()));
  return image.get() ? *image : SkBitmap();
}

void OnObtainedThumbnail(
    ScopedJavaGlobalRef<jobject>* bitmap,
    ScopedJavaGlobalRef<jobject>* j_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  Java_ThumbnailCallback_onMostVisitedURLsThumbnailAvailable(
      env, j_callback->obj(), bitmap->obj());
}

void GetUrlThumbnailTask(
    std::string url_string,
    scoped_refptr<TopSites> top_sites,
    ScopedJavaGlobalRef<jobject>* j_callback) {
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
  }

  // Since j_callback is owned by this callback, when the callback falls out of
  // scope it will be deleted. We need to pass ownership to the next callback.
  ScopedJavaGlobalRef<jobject>* j_callback_pass =
      new ScopedJavaGlobalRef<jobject>(*j_callback);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &OnObtainedThumbnail,
          base::Owned(j_bitmap_ref), base::Owned(j_callback_pass)));
}

}  // namespace

MostVisitedSites::MostVisitedSites(Profile* profile)
    : profile_(profile), num_sites_(0), weak_ptr_factory_(this) {
}

MostVisitedSites::~MostVisitedSites() {
}

void MostVisitedSites::Destroy(JNIEnv* env, jobject obj) {
  delete this;
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

// May be called from any thread
void MostVisitedSites::GetURLThumbnail(JNIEnv* env,
                                       jobject obj,
                                       jstring url,
                                       jobject j_callback_obj) {
  ScopedJavaGlobalRef<jobject>* j_callback =
      new ScopedJavaGlobalRef<jobject>();
  j_callback->Reset(env, j_callback_obj);

  std::string url_string = ConvertJavaStringToUTF8(env, url);
  scoped_refptr<TopSites> top_sites(profile_->GetTopSites());
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE, base::Bind(
          &GetUrlThumbnailTask,
          url_string,
          top_sites, base::Owned(j_callback)));
}

void MostVisitedSites::BlacklistUrl(JNIEnv* env,
                                    jobject obj,
                                    jstring j_url) {
  TopSites* top_sites = profile_->GetTopSites();
  if (!top_sites)
    return;

  std::string url_string = ConvertJavaStringToUTF8(env, j_url);
  top_sites->AddBlacklistedURL(GURL(url_string));
}

void MostVisitedSites::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOP_SITES_CHANGED);

  // Most visited urls changed, query again.
  QueryMostVisitedURLs();
}

// static
bool MostVisitedSites::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void MostVisitedSites::QueryMostVisitedURLs() {
  SuggestionsServiceFactory* suggestions_service_factory =
      SuggestionsServiceFactory::GetInstance();
  SuggestionsService* suggestions_service =
      suggestions_service_factory->GetForProfile(profile_);
  if (suggestions_service) {
    // Suggestions service is enabled, initiate a query.
    suggestions_service->FetchSuggestionsData(
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
          &OnMostVisitedURLsAvailable,
          base::Owned(new ScopedJavaGlobalRef<jobject>(observer_)),
          num_sites_),
      false);
}

void MostVisitedSites::OnSuggestionsProfileAvailable(
    ScopedJavaGlobalRef<jobject>* j_observer,
    const SuggestionsProfile& suggestions_profile) {
  size_t size = suggestions_profile.suggestions_size();
  if (size == 0) {
    // No suggestions data available, initiate Top Sites query.
    InitiateTopSitesQuery();
    return;
  }

  std::vector<base::string16> titles;
  std::vector<std::string> urls;
  for (size_t i = 0; i < size; ++i) {
    const ChromeSuggestion& suggestion = suggestions_profile.suggestions(i);
    titles.push_back(base::UTF8ToUTF16(suggestion.title()));
    urls.push_back(suggestion.url());
  }

  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedURLsObserver_onMostVisitedURLsAvailable(
      env,
      j_observer->obj(),
      ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj());
}

static jlong Init(JNIEnv* env, jobject obj, jobject jprofile) {
  MostVisitedSites* most_visited_sites =
      new MostVisitedSites(ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(most_visited_sites);
}
