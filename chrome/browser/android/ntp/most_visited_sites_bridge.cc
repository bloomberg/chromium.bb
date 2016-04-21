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
#include "chrome/browser/profiles/profile_android.h"
#include "jni/MostVisitedSites_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace {

void CallJavaWithBitmap(
    std::unique_ptr<ScopedJavaGlobalRef<jobject>> j_callback,
    bool is_local_thumbnail,
    const SkBitmap* bitmap) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (bitmap)
    j_bitmap = gfx::ConvertToJavaBitmap(bitmap);
  Java_ThumbnailCallback_onMostVisitedURLsThumbnailAvailable(
      env, j_callback->obj(), j_bitmap.obj(), is_local_thumbnail);
}

}  // namespace

class MostVisitedSitesBridge::Observer
    : public MostVisitedSitesObserver {
 public:
  Observer(JNIEnv* env, const JavaParamRef<jobject>& obj);

  void OnMostVisitedURLsAvailable(
      const std::vector<base::string16>& titles,
      const std::vector<std::string>& urls,
      const std::vector<std::string>& whitelist_icon_paths) override;

  void OnPopularURLsAvailable(
      const std::vector<std::string>& urls,
      const std::vector<std::string>& favicon_urls,
      const std::vector<std::string>& large_icon_urls) override;

 private:
  ScopedJavaGlobalRef<jobject> observer_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

MostVisitedSitesBridge::Observer::Observer(
    JNIEnv* env, const JavaParamRef<jobject>& obj)
    : observer_(env, obj) {}

void MostVisitedSitesBridge::Observer::OnMostVisitedURLsAvailable(
    const std::vector<base::string16>& titles,
    const std::vector<std::string>& urls,
    const std::vector<std::string>& whitelist_icon_paths) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK_EQ(titles.size(), urls.size());
  DCHECK_EQ(titles.size(), whitelist_icon_paths.size());
  Java_MostVisitedURLsObserver_onMostVisitedURLsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj(),
      ToJavaArrayOfStrings(env, whitelist_icon_paths).obj());
}

void MostVisitedSitesBridge::Observer::OnPopularURLsAvailable(
    const std::vector<std::string>& urls,
    const std::vector<std::string>& favicon_urls,
    const std::vector<std::string>& large_icon_urls) {
  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedURLsObserver_onPopularURLsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, urls).obj(),
      ToJavaArrayOfStrings(env, favicon_urls).obj(),
      ToJavaArrayOfStrings(env, large_icon_urls).obj());
}

MostVisitedSitesBridge::MostVisitedSitesBridge(Profile* profile)
    : most_visited_(profile) {}

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
  observer_.reset(new Observer(env, j_observer));
  most_visited_.SetMostVisitedURLsObserver(observer_.get(), num_sites);
}

void MostVisitedSitesBridge::GetURLThumbnail(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jobject>& j_callback_obj) {
  std::unique_ptr<ScopedJavaGlobalRef<jobject>> j_callback(
      new ScopedJavaGlobalRef<jobject>(env, j_callback_obj));
  auto callback = base::Bind(&CallJavaWithBitmap, base::Passed(&j_callback));
  GURL url(ConvertJavaStringToUTF8(env, j_url));
  most_visited_.GetURLThumbnail(url, callback);
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
    const JavaParamRef<jintArray>& jtile_types) {
  std::vector<int> tile_types;
  base::android::JavaIntArrayToIntVector(env, jtile_types, &tile_types);
  most_visited_.RecordTileTypeMetrics(tile_types);
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
