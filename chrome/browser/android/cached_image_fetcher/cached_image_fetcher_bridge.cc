// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/cached_image_fetcher/cached_image_fetcher_bridge.h"

#include <jni.h>

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/browser/cached_image_fetcher/cached_image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cached_image_fetcher.h"
#include "components/image_fetcher/core/cached_image_fetcher_service.h"
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
  base::FilePath file_path =
      CachedImageFetcherServiceFactory::GetCachePath(profile).Append(
          kPathPostfix);

  CachedImageFetcherService* cif_service =
      CachedImageFetcherServiceFactory::GetForBrowserContext(profile);
  CachedImageFetcherBridge* native_cif_bridge = new CachedImageFetcherBridge(
      cif_service->CreateCachedImageFetcher(), file_path);
  return reinterpret_cast<intptr_t>(native_cif_bridge);
}

CachedImageFetcherBridge::CachedImageFetcherBridge(
    std::unique_ptr<CachedImageFetcher> cached_image_fetcher,
    base::FilePath base_file_path)
    : cached_image_fetcher_(std::move(cached_image_fetcher)),
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

void CachedImageFetcherBridge::FetchImage(JNIEnv* j_env,
                                          const JavaRef<jobject>& j_this,
                                          const JavaRef<jstring>& j_url,
                                          const jint width_px,
                                          const jint height_px,
                                          const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  cached_image_fetcher_->SetDesiredImageFrameSize(
      gfx::Size(width_px, height_px));
  // TODO(wylieb): We checked disk in Java, so provide a way to tell
  // CachedImageFetcher to skip checking disk in native.
  cached_image_fetcher_->FetchImage(
      url, GURL(url),
      base::BindOnce(&CachedImageFetcherBridge::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback),
      kTrafficAnnotation);
}

void CachedImageFetcherBridge::OnImageFetched(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const std::string& id,
    const gfx::Image& image,
    const RequestMetadata& request_metadata) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty()) {
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());
  }
  RunObjectCallbackAndroid(callback, j_bitmap);
}

}  // namespace image_fetcher
