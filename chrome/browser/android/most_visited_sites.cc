// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/most_visited_sites.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/browser_thread.h"
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

namespace chrome {
namespace android {

bool RegisterMostVisitedSites(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome

namespace {

class NativeCallback : public base::RefCounted<NativeCallback> {
 public:
  NativeCallback(jobject j_callback_obj, int num_results)
      : num_results_(num_results) {
    JNIEnv* env = AttachCurrentThread();
    j_callback_obj_.Reset(env, j_callback_obj);
  }

  void OnMostVisitedURLsAvailable(
      const history::MostVisitedURLList& visited_list) {
    std::vector<string16> titles;
    std::vector<std::string> urls;
    ExtractMostVisitedTitlesAndURLs(visited_list, &titles, &urls);

    JNIEnv* env = AttachCurrentThread();
    Java_MostVisitedURLsCallback_onMostVisitedURLsAvailable(
        env,
        j_callback_obj_.obj(),
        ToJavaArrayOfStrings(env, titles).obj(),
        ToJavaArrayOfStrings(env, urls).obj());
  }

 private:
  friend class base::RefCounted<NativeCallback>;
  ~NativeCallback() {}

  void ExtractMostVisitedTitlesAndURLs(
      const history::MostVisitedURLList& visited_list,
      std::vector<string16>* titles,
      std::vector<std::string>* urls) {
    for (size_t i = 0; i < visited_list.size() && i < num_results_; ++i) {
      const history::MostVisitedURL& visited = visited_list[i];

      if (visited.url.is_empty())
        break;  // This is the signal that there are no more real visited sites.

      titles->push_back(visited.title);
      urls->push_back(visited.url.spec());
    }
  }

  ScopedJavaGlobalRef<jobject> j_callback_obj_;
  size_t num_results_;
};

SkBitmap ExtractThumbnail(const base::RefCountedMemory& image_data) {
  scoped_ptr<SkBitmap> image(gfx::JPEGCodec::Decode(
      image_data.front(),
      image_data.size()));
  return image.get() ? *image : SkBitmap();
}

void OnObtainedThumbnail(
    ScopedJavaGlobalRef<jobject>* bitmap,
    ScopedJavaGlobalRef<jobject>* j_callback_ref) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  Java_ThumbnailCallback_onMostVisitedURLsThumbnailAvailable(
      env, j_callback_ref->obj(), bitmap->obj());
}

void GetUrlThumbnailTask(
    std::string url_string,
    scoped_refptr<TopSites> top_sites,
    ScopedJavaGlobalRef<jobject>* j_callback_ref) {
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

  // Since j_callback_ref is owned by this callback,
  // when the callback falls out of scope it will be deleted.
  // We need to pass ownership to the next callback.
  ScopedJavaGlobalRef<jobject>* j_callback_ref_pass =
      new ScopedJavaGlobalRef<jobject>(*j_callback_ref);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &OnObtainedThumbnail,
          base::Owned(j_bitmap_ref),base::Owned(j_callback_ref_pass)));
}

}  // namespace

void GetMostVisitedURLs(
    JNIEnv* env,
    jclass clazz,
    jobject j_profile,
    jobject j_callback_obj,
    jint num_results) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);

  DCHECK(profile);
  if (!profile)
    return;

  TopSites* top_sites = profile->GetTopSites();
  if (!top_sites)
    return;

  // TopSites updates itself after a delay. To ensure up-to-date results, force
  // an update now.
  top_sites->SyncWithHistory();

  scoped_refptr<NativeCallback> native_callback =
      new NativeCallback(j_callback_obj, static_cast<int>(num_results));
  top_sites->GetMostVisitedURLs(
      base::Bind(&NativeCallback::OnMostVisitedURLsAvailable,
                 native_callback));
}

// May be called from any thread
void GetURLThumbnail(
    JNIEnv* env,
    jclass clazz,
    jobject j_profile,
    jstring url,
    jobject j_callback_obj) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);

  DCHECK(profile);
  if (!profile)
    return;

  ScopedJavaGlobalRef<jobject>* j_callback_ref =
      new ScopedJavaGlobalRef<jobject>();
  j_callback_ref->Reset(env, j_callback_obj);

  std::string url_string = ConvertJavaStringToUTF8(env, url);
  scoped_refptr<TopSites> top_sites(profile->GetTopSites());
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE, base::Bind(
          &GetUrlThumbnailTask,
          url_string,
          top_sites, base::Owned(j_callback_ref)));
}

void BlacklistUrl(JNIEnv* env, jclass clazz, jobject j_profile, jstring j_url) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);

  DCHECK(profile);
  if (!profile)
    return;

  TopSites* top_sites = profile->GetTopSites();
  if (!top_sites)
    return;

  std::string url_string = ConvertJavaStringToUTF8(env, j_url);
  top_sites->AddBlacklistedURL(GURL(url_string));
}
