// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/favicon_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "jni/FaviconHelper_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace {

void OnLocalFaviconAvailable(
    ScopedJavaGlobalRef<jobject>* j_favicon_image_callback,
    const chrome::FaviconImageResult& favicon_image_result) {
  JNIEnv* env = AttachCurrentThread();

  // Convert favicon_image_result to java objects.
  ScopedJavaLocalRef<jstring> j_icon_url =
      ConvertUTF8ToJavaString(env, favicon_image_result.icon_url.spec());
  SkBitmap favicon_bitmap = favicon_image_result.image.AsBitmap();
  ScopedJavaLocalRef<jobject> j_favicon_bitmap;
  if (!favicon_bitmap.isNull())
    j_favicon_bitmap = gfx::ConvertToJavaBitmap(&favicon_bitmap);

  // Call java side OnLocalFaviconAvailable method.
  Java_FaviconImageCallback_onFaviconAvailable(env,
                                               j_favicon_image_callback->obj(),
                                               j_favicon_bitmap.obj(),
                                               j_icon_url.obj());
}

}  // namespace

static jint Init(JNIEnv* env, jclass clazz) {
  return reinterpret_cast<jint>(new FaviconHelper());
}

FaviconHelper::FaviconHelper() {
  cancelable_task_tracker_.reset(new CancelableTaskTracker());
}

void FaviconHelper::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

jboolean FaviconHelper::GetLocalFaviconImageForURL(
    JNIEnv* env,
    jobject obj,
    jobject j_profile,
    jstring j_page_url,
    jint j_icon_types,
    jint j_desired_size_in_dip,
    jobject j_favicon_image_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);
  if (!profile)
    return false;

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  DCHECK(favicon_service);
  if (!favicon_service)
    return false;

  FaviconService::FaviconForURLParams params(
      GURL(ConvertJavaStringToUTF16(env, j_page_url)),
      static_cast<int>(j_icon_types),
      static_cast<int>(j_desired_size_in_dip));

  ScopedJavaGlobalRef<jobject>* j_scoped_favicon_callback =
      new ScopedJavaGlobalRef<jobject>();
  j_scoped_favicon_callback->Reset(env, j_favicon_image_callback);

  FaviconService::FaviconImageCallback callback_runner = base::Bind(
      &OnLocalFaviconAvailable, base::Owned(j_scoped_favicon_callback));

  favicon_service->GetFaviconImageForURL(
      params, callback_runner,
      cancelable_task_tracker_.get());

  return true;
}

ScopedJavaLocalRef<jobject> FaviconHelper::GetSyncedFaviconImageForURL(
    JNIEnv* env,
    jobject obj,
    jobject jprofile,
    jstring j_page_url) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);

  std::string page_url = ConvertJavaStringToUTF8(env, j_page_url);

  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  DCHECK(sync_service);

  scoped_refptr<base::RefCountedMemory> favicon_png;
  browser_sync::SessionModelAssociator* associator =
      sync_service->GetSessionModelAssociator();
  DCHECK(associator);

  if (!associator->GetSyncedFaviconForPageURL(page_url, &favicon_png))
    return ScopedJavaLocalRef<jobject>();

    // Convert favicon_image_result to java objects.
  gfx::Image favicon_image = gfx::Image::CreateFrom1xPNGBytes(
      favicon_png->front(), favicon_png->size());
  SkBitmap favicon_bitmap = favicon_image.AsBitmap();

  ScopedJavaLocalRef<jobject> j_favicon_bitmap;
  if (favicon_bitmap.isNull())
    return ScopedJavaLocalRef<jobject>();

  return gfx::ConvertToJavaBitmap(&favicon_bitmap);
}

jint FaviconHelper::GetDominantColorForBitmap(JNIEnv* env,
                                              jobject obj,
                                              jobject bitmap) {
  if (!bitmap)
    return 0;

  gfx::JavaBitmap bitmap_lock(bitmap);
  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(bitmap_lock);
  skbitmap.setImmutable();
  scoped_refptr<base::RefCountedMemory> png_data =
      gfx::Image::CreateFrom1xBitmap(skbitmap).As1xPNGBytes();
  uint32_t max_brightness = 665;
  uint32_t min_darkness = 100;
  color_utils::GridSampler sampler;
  return color_utils::CalculateKMeanColorOfPNG(
      png_data, min_darkness, max_brightness, &sampler);
}

FaviconHelper::~FaviconHelper() {}

// static
bool FaviconHelper::RegisterFaviconHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
