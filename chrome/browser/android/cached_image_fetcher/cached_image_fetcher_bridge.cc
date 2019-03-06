// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/cached_image_fetcher/cached_image_fetcher_bridge.h"

#include <jni.h>

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "chrome/browser/cached_image_fetcher/cached_image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/image_fetcher/core/cache/cached_image_fetcher_metrics_reporter.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cached_image_fetcher_service.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "jni/CachedImageFetcherBridge_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::JavaRef;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace image_fetcher {

namespace {

// Keep in sync with postfix found in image_data_store_disk.cc.
const base::FilePath::CharType kPathPostfix[] =
    FILE_PATH_LITERAL("image_data_storage");

// TODO(wylieb): Allow java clients to map to a traffic_annotation here.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cached_image_fetcher", R"(
        semantics {
          sender: "Cached Image Fetcher Fetch"
          description:
            "Fetches and caches images for Chrome features."
          trigger:
            "Triggered when a feature requests an image fetch."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Cache can be cleared through settings."
        policy_exception_justification:
          "This feature allows many different Chrome features to fetch/cache "
          "images and thus there is no Chrome-wide policy to disable it."
      })");

}  // namespace

// static
jlong JNI_CachedImageFetcherBridge_Init(
    JNIEnv* j_env,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  SimpleFactoryKey* simple_factory_key = profile->GetSimpleFactoryKey();
  base::FilePath file_path =
      CachedImageFetcherServiceFactory::GetCachePath(simple_factory_key)
          .Append(kPathPostfix);

  CachedImageFetcherService* cif_service =
      CachedImageFetcherServiceFactory::GetForKey(simple_factory_key,
                                                  profile->GetPrefs());
  CachedImageFetcherBridge* native_cif_bridge = new CachedImageFetcherBridge(
      cif_service->GetCachedImageFetcher(), file_path);
  return reinterpret_cast<intptr_t>(native_cif_bridge);
}

CachedImageFetcherBridge::CachedImageFetcherBridge(
    ImageFetcher* cached_image_fetcher,
    base::FilePath base_file_path)
    : cached_image_fetcher_(cached_image_fetcher),
      base_file_path_(base_file_path),
      weak_ptr_factory_(this) {}

CachedImageFetcherBridge::~CachedImageFetcherBridge() = default;

void CachedImageFetcherBridge::Destroy(JNIEnv* j_env,
                                       const JavaRef<jobject>& j_this) {
  delete this;
}

ScopedJavaLocalRef<jstring> CachedImageFetcherBridge::GetFilePath(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_url) {
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  std::string file_path =
      base_file_path_.Append(ImageCache::HashUrlToKey(url)).MaybeAsASCII();
  return base::android::ConvertUTF8ToJavaString(j_env, file_path);
}

void CachedImageFetcherBridge::FetchImageData(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_url,
    const JavaRef<jstring>& j_client_name,
    const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);

  image_fetcher::ImageFetcherParams params(kTrafficAnnotation, client_name);

  // We can skip transcoding here because this method is used in java as
  // CachedImageFetcher.fetchGif, which decodes the data in a Java-only library.
  params.set_skip_transcoding(true);

  // TODO(wylieb): We checked disk in Java, so provide a way to tell
  // CachedImageFetcher to skip checking disk in native.
  cached_image_fetcher_->FetchImageData(
      GURL(url),
      base::BindOnce(&CachedImageFetcherBridge::OnImageDataFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback),
      std::move(params));
}

void CachedImageFetcherBridge::FetchImage(JNIEnv* j_env,
                                          const JavaRef<jobject>& j_this,
                                          const JavaRef<jstring>& j_url,
                                          const JavaRef<jstring>& j_client_name,
                                          const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);

  ImageFetcherParams params(kTrafficAnnotation, client_name);

  // TODO(wylieb): We checked disk in Java, so provide a way to tell
  // CachedImageFetcher to skip checking disk in native.
  cached_image_fetcher_->FetchImage(
      GURL(url),
      base::BindOnce(&CachedImageFetcherBridge::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback),
      std::move(params));
}

void CachedImageFetcherBridge::ReportEvent(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_client_name,
    const jint j_event_id) {
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);
  CachedImageFetcherEvent event =
      static_cast<CachedImageFetcherEvent>(j_event_id);
  CachedImageFetcherMetricsReporter::ReportEvent(client_name, event);
}

void CachedImageFetcherBridge::ReportCacheHitTime(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_client_name,
    const jlong start_time_millis) {
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);
  base::Time start_time = base::Time::FromJavaTime(start_time_millis);
  CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTimeJava(
      client_name, start_time);
}

void CachedImageFetcherBridge::ReportTotalFetchTimeFromNative(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_client_name,
    const jlong start_time_millis) {
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);
  base::Time start_time = base::Time::FromJavaTime(start_time_millis);
  CachedImageFetcherMetricsReporter::ReportTotalFetchFromNativeTimeJava(
      client_name, start_time);
}

void CachedImageFetcherBridge::OnImageDataFetched(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const std::string& image_data,
    const RequestMetadata& request_metadata) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_bytes = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(image_data.data()),
      image_data.size());
  RunObjectCallbackAndroid(callback, j_bytes);
}

void CachedImageFetcherBridge::OnImageFetched(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const gfx::Image& image,
    const RequestMetadata& request_metadata) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty()) {
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());
  }
  RunObjectCallbackAndroid(callback, j_bitmap);
}

}  // namespace image_fetcher
