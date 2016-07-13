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
#include "chrome/browser/android/ntp/popular_sites.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/popular_sites.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "jni/MostVisitedSites_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;
using content::BrowserThread;
using ntp_tiles::MostVisitedSites;
using ntp_tiles::MostVisitedSitesSupervisor;
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
  auto url_filter = supervised_user_service->GetURLFilterForUIThread();
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

  void OnMostVisitedURLsAvailable(
      const MostVisitedSites::SuggestionsVector& suggestions) override;

  void OnPopularURLsAvailable(
      const MostVisitedSites::PopularSitesVector& sites) override;

 private:
  ScopedJavaGlobalRef<jobject> observer_;

  DISALLOW_COPY_AND_ASSIGN(JavaObserver);
};

MostVisitedSitesBridge::JavaObserver::JavaObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj)
    : observer_(env, obj) {}

void MostVisitedSitesBridge::JavaObserver::OnMostVisitedURLsAvailable(
    const MostVisitedSites::SuggestionsVector& suggestions) {
  JNIEnv* env = AttachCurrentThread();
  std::vector<base::string16> titles;
  std::vector<std::string> urls;
  std::vector<std::string> whitelist_icon_paths;
  std::vector<int> sources;
  std::vector<int> provider_indexes;

  titles.reserve(suggestions.size());
  urls.reserve(suggestions.size());
  whitelist_icon_paths.reserve(suggestions.size());
  sources.reserve(suggestions.size());
  provider_indexes.reserve(suggestions.size());
  for (const auto& suggestion : suggestions) {
    titles.emplace_back(suggestion.title);
    urls.emplace_back(suggestion.url.spec());
    whitelist_icon_paths.emplace_back(suggestion.whitelist_icon_path.value());
    sources.emplace_back(suggestion.source);
    provider_indexes.emplace_back(suggestion.provider_index);
  }
  Java_MostVisitedURLsObserver_onMostVisitedURLsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj(),
      ToJavaArrayOfStrings(env, whitelist_icon_paths).obj(),
      ToJavaIntArray(env, sources).obj(),
      ToJavaIntArray(env, provider_indexes).obj());
}

void MostVisitedSitesBridge::JavaObserver::OnPopularURLsAvailable(
    const MostVisitedSites::PopularSitesVector& sites) {
  JNIEnv* env = AttachCurrentThread();
  std::vector<std::string> urls;
  std::vector<std::string> favicon_urls;
  std::vector<std::string> large_icon_urls;
  for (const auto& site : sites) {
    urls.emplace_back(site.url.spec());
    favicon_urls.emplace_back(site.favicon_url.spec());
    large_icon_urls.emplace_back(site.large_icon_url.spec());
  }
  Java_MostVisitedURLsObserver_onPopularURLsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, urls).obj(),
      ToJavaArrayOfStrings(env, favicon_urls).obj(),
      ToJavaArrayOfStrings(env, large_icon_urls).obj());
}

MostVisitedSitesBridge::MostVisitedSitesBridge(Profile* profile)
    : supervisor_(profile),
      most_visited_(BrowserThread::GetBlockingPool(),
                    profile->GetPrefs(),
                    TemplateURLServiceFactory::GetForProfile(profile),
                    g_browser_process->variations_service(),
                    profile->GetRequestContext(),
                    ChromePopularSites::GetDirectory(),
                    TopSitesFactory::GetForProfile(profile),
                    SuggestionsServiceFactory::GetForProfile(profile),
                    &supervisor_) {
  // Register the thumbnails debugging page.
  // TODO(sfiera): find thumbnails a home. They don't belong here.
  content::URLDataSource::Add(profile, new ThumbnailListSource(profile));
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

void MostVisitedSitesBridge::RecordTileTypeMetrics(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jintArray>& jtile_types,
    const JavaParamRef<jintArray>& jsources,
    const JavaParamRef<jintArray>& jprovider_indices) {
  std::vector<int> tile_types;
  std::vector<int> sources;
  std::vector<int> provider_indices;

  base::android::JavaIntArrayToIntVector(env, jtile_types, &tile_types);
  base::android::JavaIntArrayToIntVector(env, jsources, &sources);
  base::android::JavaIntArrayToIntVector(env, jprovider_indices,
                                         &provider_indices);

  most_visited_.RecordTileTypeMetrics(tile_types, sources, provider_indices);
}

void MostVisitedSitesBridge::RecordOpenedMostVisitedItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index,
    jint tile_type) {
  most_visited_.RecordOpenedMostVisitedItem(index, tile_type);
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
