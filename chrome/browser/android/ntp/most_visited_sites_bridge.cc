// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/most_visited_sites_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/rappor/rappor_service_impl.h"
#include "jni/MostVisitedSitesBridge_jni.h"
#include "jni/MostVisitedSites_jni.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;
using ntp_tiles::MostVisitedSites;
using ntp_tiles::TileSource;
using ntp_tiles::NTPTilesVector;
using ntp_tiles::TileVisualType;

namespace {

class JavaHomePageClient : public MostVisitedSites::HomePageClient {
 public:
  JavaHomePageClient(JNIEnv* env,
                     const JavaParamRef<jobject>& obj,
                     Profile* profile);

  bool IsHomePageEnabled() const override;
  bool IsNewTabPageUsedAsHomePage() const override;
  GURL GetHomePageUrl() const override;
  void QueryHomePageTitle(TitleCallback title_callback) override;

 private:
  void OnTitleEntryFound(TitleCallback title_callback,
                         bool success,
                         const history::URLRow& row,
                         const history::VisitVector& visits);

  ScopedJavaGlobalRef<jobject> client_;
  Profile* profile_;

  // Used in loading titles.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(JavaHomePageClient);
};

JavaHomePageClient::JavaHomePageClient(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       Profile* profile)
    : client_(env, obj), profile_(profile) {
  DCHECK(profile);
}

void JavaHomePageClient::QueryHomePageTitle(TitleCallback title_callback) {
  DCHECK(!title_callback.is_null());
  GURL url = GetHomePageUrl();
  if (url.is_empty()) {
    std::move(title_callback).Run(base::nullopt);
    return;
  }
  history::HistoryService* const history_service =
      HistoryServiceFactory::GetForProfileIfExists(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service) {
    std::move(title_callback).Run(base::nullopt);
    return;
  }
  // If the client is destroyed, the tracker will cancel this task automatically
  // and the callback will not be called. Therefore, base::Unretained works.
  history_service->QueryURL(
      url,
      /*want_visits=*/false,
      base::BindOnce(&JavaHomePageClient::OnTitleEntryFound,
                     base::Unretained(this), std::move(title_callback)),
      &task_tracker_);
}

void JavaHomePageClient::OnTitleEntryFound(TitleCallback title_callback,
                                           bool success,
                                           const history::URLRow& row,
                                           const history::VisitVector& visits) {
  if (!success) {
    std::move(title_callback).Run(base::nullopt);
    return;
  }
  std::move(title_callback).Run(row.title());
}

bool JavaHomePageClient::IsHomePageEnabled() const {
  return Java_HomePageClient_isHomePageEnabled(AttachCurrentThread(), client_);
}

bool JavaHomePageClient::IsNewTabPageUsedAsHomePage() const {
  return Java_HomePageClient_isNewTabPageUsedAsHomePage(AttachCurrentThread(),
                                                        client_);
}

GURL JavaHomePageClient::GetHomePageUrl() const {
  base::android::ScopedJavaLocalRef<jstring> url =
      Java_HomePageClient_getHomePageUrl(AttachCurrentThread(), client_);
  if (url.is_null()) {
    return GURL();
  }
  return GURL(ConvertJavaStringToUTF8(url));
}

}  // namespace

class MostVisitedSitesBridge::JavaObserver : public MostVisitedSites::Observer {
 public:
  JavaObserver(JNIEnv* env, const JavaParamRef<jobject>& obj);

  void OnMostVisitedURLsAvailable(const NTPTilesVector& tiles) override;

  void OnIconMadeAvailable(const GURL& site_url) override;

 private:
  ScopedJavaGlobalRef<jobject> observer_;

  DISALLOW_COPY_AND_ASSIGN(JavaObserver);
};

MostVisitedSitesBridge::JavaObserver::JavaObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj)
    : observer_(env, obj) {}

void MostVisitedSitesBridge::JavaObserver::OnMostVisitedURLsAvailable(
    const NTPTilesVector& tiles) {
  JNIEnv* env = AttachCurrentThread();
  std::vector<base::string16> titles;
  std::vector<std::string> urls;
  std::vector<std::string> whitelist_icon_paths;
  std::vector<int> sources;

  titles.reserve(tiles.size());
  urls.reserve(tiles.size());
  whitelist_icon_paths.reserve(tiles.size());
  sources.reserve(tiles.size());
  for (const auto& tile : tiles) {
    titles.emplace_back(tile.title);
    urls.emplace_back(tile.url.spec());
    whitelist_icon_paths.emplace_back(tile.whitelist_icon_path.value());
    sources.emplace_back(static_cast<int>(tile.source));
  }
  Java_MostVisitedSitesBridge_onMostVisitedURLsAvailable(
      env, observer_, ToJavaArrayOfStrings(env, titles),
      ToJavaArrayOfStrings(env, urls),
      ToJavaArrayOfStrings(env, whitelist_icon_paths),
      ToJavaIntArray(env, sources));
}

void MostVisitedSitesBridge::JavaObserver::OnIconMadeAvailable(
    const GURL& site_url) {
  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedSitesBridge_onIconMadeAvailable(
      env, observer_, ConvertUTF8ToJavaString(env, site_url.spec()));
}

MostVisitedSitesBridge::MostVisitedSitesBridge(Profile* profile)
    : most_visited_(ChromeMostVisitedSitesFactory::NewForProfile(profile)),
      profile_(profile) {
  // Register the thumbnails debugging page.
  // TODO(sfiera): find thumbnails a home. They don't belong here.
  content::URLDataSource::Add(profile, new ThumbnailListSource(profile));
  DCHECK(!profile->IsOffTheRecord());
}

MostVisitedSitesBridge::~MostVisitedSitesBridge() {}

void MostVisitedSitesBridge::Destroy(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  delete this;
}

void MostVisitedSitesBridge::OnHomePageStateChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  most_visited_->OnHomePageStateChanged();
}

void MostVisitedSitesBridge::SetObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_observer,
    jint num_sites) {
  java_observer_.reset(new JavaObserver(env, j_observer));
  most_visited_->SetMostVisitedURLsObserver(java_observer_.get(), num_sites);
}

void MostVisitedSitesBridge::SetHomePageClient(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_client) {
  most_visited_->SetHomePageClient(
      base::MakeUnique<JavaHomePageClient>(env, j_client, profile_));
}

void MostVisitedSitesBridge::AddOrRemoveBlacklistedUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_url,
    jboolean add_url) {
  GURL url(ConvertJavaStringToUTF8(env, j_url));
  most_visited_->AddOrRemoveBlacklistedUrl(url, add_url);
}

void MostVisitedSitesBridge::RecordPageImpression(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint jtiles_count) {
  ntp_tiles::metrics::RecordPageImpression(jtiles_count);
}

void MostVisitedSitesBridge::RecordTileImpression(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint jindex,
    jint jtype,
    jint jsource,
    const JavaParamRef<jstring>& jurl) {
  GURL url(ConvertJavaStringToUTF8(env, jurl));
  TileSource source = static_cast<TileSource>(jsource);
  TileVisualType type = static_cast<TileVisualType>(jtype);

  ntp_tiles::metrics::RecordTileImpression(jindex, source, type, url,
                                           g_browser_process->rappor_service());
}

void MostVisitedSitesBridge::RecordOpenedMostVisitedItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index,
    jint tile_type,
    jint source) {
  ntp_tiles::metrics::RecordTileClick(index, static_cast<TileSource>(source),
                                      static_cast<TileVisualType>(tile_type));
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& jprofile) {
  MostVisitedSitesBridge* most_visited_sites =
      new MostVisitedSitesBridge(ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(most_visited_sites);
}
