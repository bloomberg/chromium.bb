// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/most_visited_sites_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/rappor/rappor_service_impl.h"
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
using ntp_tiles::NTPTileSource;
using ntp_tiles::NTPTilesVector;
using ntp_tiles::metrics::MostVisitedTileType;
using ntp_tiles::metrics::TileImpression;

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
  Java_Observer_onMostVisitedURLsAvailable(
      env, observer_, ToJavaArrayOfStrings(env, titles),
      ToJavaArrayOfStrings(env, urls),
      ToJavaArrayOfStrings(env, whitelist_icon_paths),
      ToJavaIntArray(env, sources));
}

void MostVisitedSitesBridge::JavaObserver::OnIconMadeAvailable(
    const GURL& site_url) {
  JNIEnv* env = AttachCurrentThread();
  Java_Observer_onIconMadeAvailable(
      env, observer_, ConvertUTF8ToJavaString(env, site_url.spec()));
}

MostVisitedSitesBridge::MostVisitedSitesBridge(Profile* profile)
    : most_visited_(ChromeMostVisitedSitesFactory::NewForProfile(profile)) {
  // Register the thumbnails debugging page.
  // TODO(sfiera): find thumbnails a home. They don't belong here.
  content::URLDataSource::Add(profile, new ThumbnailListSource(profile));
  DCHECK(!profile->IsOffTheRecord());
}

MostVisitedSitesBridge::~MostVisitedSitesBridge() {}

void MostVisitedSitesBridge::Destroy(
    JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void MostVisitedSitesBridge::SetObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_observer,
    jint num_sites) {
  java_observer_.reset(new JavaObserver(env, j_observer));
  most_visited_->SetMostVisitedURLsObserver(java_observer_.get(), num_sites);
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
    const JavaParamRef<jintArray>& jtile_types,
    const JavaParamRef<jintArray>& jsources,
    const JavaParamRef<jobjectArray>& jtile_urls) {
  std::vector<int> int_sources;
  base::android::JavaIntArrayToIntVector(env, jsources, &int_sources);
  std::vector<int> int_tile_types;
  base::android::JavaIntArrayToIntVector(env, jtile_types, &int_tile_types);
  std::vector<std::string> string_tile_urls;
  base::android::AppendJavaStringArrayToStringVector(env, jtile_urls,
                                                     &string_tile_urls);

  DCHECK_EQ(int_sources.size(), int_tile_types.size());
  DCHECK_EQ(int_sources.size(), string_tile_urls.size());

  std::vector<TileImpression> tiles;
  for (size_t i = 0; i < int_sources.size(); i++) {
    NTPTileSource source = static_cast<NTPTileSource>(int_sources[i]);
    MostVisitedTileType tile_type =
        static_cast<MostVisitedTileType>(int_tile_types[i]);

    tiles.emplace_back(source, tile_type, GURL(string_tile_urls[i]));
  }
  ntp_tiles::metrics::RecordPageImpression(tiles,
                                           g_browser_process->rappor_service());
}

void MostVisitedSitesBridge::RecordOpenedMostVisitedItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index,
    jint tile_type,
    jint source) {
  ntp_tiles::metrics::RecordTileClick(
      index, static_cast<NTPTileSource>(source),
      static_cast<MostVisitedTileType>(tile_type));
}

// static
bool MostVisitedSitesBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& jprofile) {
  MostVisitedSitesBridge* most_visited_sites =
      new MostVisitedSitesBridge(
          ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(most_visited_sites);
}
