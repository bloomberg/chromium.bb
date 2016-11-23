// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/most_visited_sites_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/ntp/popular_sites.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "components/history/core/browser/top_sites.h"
#include "components/image_fetcher/image_fetcher_impl.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/popular_sites.h"
#include "components/safe_json/safe_json_parser.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "jni/MostVisitedSites_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;
using content::BrowserThread;
using ntp_tiles::metrics::MostVisitedTileType;
using ntp_tiles::MostVisitedSites;
using ntp_tiles::MostVisitedSitesSupervisor;
using ntp_tiles::NTPTileSource;
using suggestions::SuggestionsServiceFactory;

MostVisitedSitesBridge::SupervisorBridge::SupervisorBridge(Profile* profile)
    : profile_(profile),
      supervisor_observer_(nullptr),
      register_observer_(this) {
  register_observer_.Add(SupervisedUserServiceFactory::GetForProfile(profile_));
}

MostVisitedSitesBridge::SupervisorBridge::~SupervisorBridge() {}

void MostVisitedSitesBridge::SupervisorBridge::SetObserver(
    Observer* new_observer) {
  if (new_observer)
    DCHECK(!supervisor_observer_);
  else
    DCHECK(supervisor_observer_);

  supervisor_observer_ = new_observer;
}

bool MostVisitedSitesBridge::SupervisorBridge::IsBlocked(const GURL& url) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  auto* url_filter = supervised_user_service->GetURLFilterForUIThread();
  return url_filter->GetFilteringBehaviorForURL(url) ==
         SupervisedUserURLFilter::FilteringBehavior::BLOCK;
}

std::vector<MostVisitedSitesSupervisor::Whitelist>
MostVisitedSitesBridge::SupervisorBridge::whitelists() {
  std::vector<MostVisitedSitesSupervisor::Whitelist> results;
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  for (const auto& whitelist : supervised_user_service->whitelists()) {
    results.emplace_back(Whitelist{
        whitelist->title(), whitelist->entry_point(),
        whitelist->large_icon_path(),
    });
  }
  return results;
}

bool MostVisitedSitesBridge::SupervisorBridge::IsChildProfile() {
  return profile_->IsChild();
}

void MostVisitedSitesBridge::SupervisorBridge::OnURLFilterChanged() {
  if (supervisor_observer_)
    supervisor_observer_->OnBlockedSitesChanged();
}

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
  Java_MostVisitedURLsObserver_onMostVisitedURLsAvailable(
      env, observer_, ToJavaArrayOfStrings(env, titles),
      ToJavaArrayOfStrings(env, urls),
      ToJavaArrayOfStrings(env, whitelist_icon_paths),
      ToJavaIntArray(env, sources));
}

void MostVisitedSitesBridge::JavaObserver::OnIconMadeAvailable(
    const GURL& site_url) {
  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedURLsObserver_onIconMadeAvailable(
      env, observer_, ConvertUTF8ToJavaString(env, site_url.spec()));
}

MostVisitedSitesBridge::MostVisitedSitesBridge(Profile* profile)
    : supervisor_(profile),
      most_visited_(profile->GetPrefs(),
                    TopSitesFactory::GetForProfile(profile),
                    SuggestionsServiceFactory::GetForProfile(profile),
                    ChromePopularSites::NewForProfile(profile),
                    base::MakeUnique<ntp_tiles::IconCacher>(
                        FaviconServiceFactory::GetForProfile(
                            profile,
                            ServiceAccessType::IMPLICIT_ACCESS),
                        base::MakeUnique<image_fetcher::ImageFetcherImpl>(
                            base::MakeUnique<suggestions::ImageDecoderImpl>(),
                            profile->GetRequestContext())),
                    &supervisor_) {
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

void MostVisitedSitesBridge::SetMostVisitedURLsObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_observer,
    jint num_sites) {
  java_observer_.reset(new JavaObserver(env, j_observer));
  most_visited_.SetMostVisitedURLsObserver(java_observer_.get(), num_sites);
}

void MostVisitedSitesBridge::AddOrRemoveBlacklistedUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_url,
    jboolean add_url) {
  GURL url(ConvertJavaStringToUTF8(env, j_url));
  most_visited_.AddOrRemoveBlacklistedUrl(url, add_url);
}

void MostVisitedSitesBridge::RecordPageImpression(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jintArray>& jtile_types,
    const JavaParamRef<jintArray>& jsources) {
  std::vector<int> int_tile_types;
  base::android::JavaIntArrayToIntVector(env, jtile_types, &int_tile_types);
  std::vector<MostVisitedTileType> tile_types;
  for (int source : int_tile_types) {
    tile_types.push_back(static_cast<MostVisitedTileType>(source));
  }

  std::vector<int> int_sources;
  base::android::JavaIntArrayToIntVector(env, jsources, &int_sources);
  std::vector<NTPTileSource> sources;
  for (int source : int_sources) {
    sources.push_back(static_cast<NTPTileSource>(source));
  }

  int num_tiles = static_cast<int>(sources.size());
  for (int i = 0; i < num_tiles; i++) {
    ntp_tiles::metrics::RecordTileImpression(i, sources[i]);
  }
  ntp_tiles::metrics::RecordPageImpression(num_tiles);
  ntp_tiles::metrics::RecordImpressionTileTypes(tile_types, sources);
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
