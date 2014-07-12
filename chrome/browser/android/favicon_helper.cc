// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/favicon_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "jni/FaviconHelper_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
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
    const favicon_base::FaviconRawBitmapResult& result) {
  JNIEnv* env = AttachCurrentThread();

  // Convert favicon_image_result to java objects.
  ScopedJavaLocalRef<jstring> j_icon_url =
      ConvertUTF8ToJavaString(env, result.icon_url.spec());
  ScopedJavaLocalRef<jobject> j_favicon_bitmap;
  if (result.is_valid()) {
    SkBitmap favicon_bitmap;
    gfx::PNGCodec::Decode(result.bitmap_data->front(),
                          result.bitmap_data->size(),
                          &favicon_bitmap);
    if (!favicon_bitmap.isNull())
      j_favicon_bitmap = gfx::ConvertToJavaBitmap(&favicon_bitmap);
  }

  // Call java side OnLocalFaviconAvailable method.
  Java_FaviconImageCallback_onFaviconAvailable(env,
                                               j_favicon_image_callback->obj(),
                                               j_favicon_bitmap.obj(),
                                               j_icon_url.obj());
}

void OnFaviconRawBitmapResultAvailable(
    ScopedJavaGlobalRef<jobject>* j_favicon_image_callback,
    const favicon_base::FaviconRawBitmapResult& favicon_bitmap_result) {
  JNIEnv* env = AttachCurrentThread();

  // Convert favicon_image_result to java objects.
  ScopedJavaLocalRef<jstring> j_icon_url =
      ConvertUTF8ToJavaString(env, favicon_bitmap_result.icon_url.spec());

  SkBitmap favicon_bitmap;
  if (favicon_bitmap_result.is_valid()) {
    gfx::PNGCodec::Decode(favicon_bitmap_result.bitmap_data->front(),
                          favicon_bitmap_result.bitmap_data->size(),
                          &favicon_bitmap);
  }
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

static jlong Init(JNIEnv* env, jclass clazz) {
  return reinterpret_cast<intptr_t>(new FaviconHelper());
}

FaviconHelper::FaviconHelper() {
  cancelable_task_tracker_.reset(new base::CancelableTaskTracker());
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
    jint j_desired_size_in_pixel,
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

  ScopedJavaGlobalRef<jobject>* j_scoped_favicon_callback =
      new ScopedJavaGlobalRef<jobject>();
  j_scoped_favicon_callback->Reset(env, j_favicon_image_callback);

  favicon_base::FaviconRawBitmapCallback callback_runner = base::Bind(
      &OnLocalFaviconAvailable, base::Owned(j_scoped_favicon_callback));

  favicon_service->GetRawFaviconForPageURL(
      GURL(ConvertJavaStringToUTF16(env, j_page_url)),
      static_cast<int>(j_icon_types),
      static_cast<int>(j_desired_size_in_pixel),
      callback_runner,
      cancelable_task_tracker_.get());

  return true;
}

void FaviconHelper::GetLargestRawFaviconForUrl(
    JNIEnv* env,
    jobject obj,
    jobject j_profile,
    jstring j_page_url,
    jintArray j_icon_types,
    jint j_min_size_threshold_px,
    jobject j_favicon_image_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);
  if (!profile)
    return;

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  DCHECK(favicon_service);
  if (!favicon_service)
    return;

  std::vector<int> icon_types;
  base::android::JavaIntArrayToIntVector(env, j_icon_types, &icon_types);

  ScopedJavaGlobalRef<jobject>* j_scoped_favicon_callback =
      new ScopedJavaGlobalRef<jobject>();
  j_scoped_favicon_callback->Reset(env, j_favicon_image_callback);

  favicon_base::FaviconRawBitmapCallback callback_runner =
      base::Bind(&OnFaviconRawBitmapResultAvailable,
                 base::Owned(j_scoped_favicon_callback));
  favicon_service->GetLargestRawFaviconForPageURL(
      GURL(ConvertJavaStringToUTF16(env, j_page_url)),
      icon_types,
      static_cast<int>(j_min_size_threshold_px),
      callback_runner,
      cancelable_task_tracker_.get());
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
  browser_sync::OpenTabsUIDelegate* open_tabs =
      sync_service->GetOpenTabsUIDelegate();
  DCHECK(open_tabs);

  if (!open_tabs->GetSyncedFaviconForPageURL(page_url, &favicon_png))
    return ScopedJavaLocalRef<jobject>();

  // Convert favicon_image_result to java objects.
  gfx::Image favicon_image = gfx::Image::CreateFrom1xPNGBytes(favicon_png);
  SkBitmap favicon_bitmap = favicon_image.AsBitmap();

  ScopedJavaLocalRef<jobject> j_favicon_bitmap;
  if (favicon_bitmap.isNull())
    return ScopedJavaLocalRef<jobject>();

  return gfx::ConvertToJavaBitmap(&favicon_bitmap);
}

FaviconHelper::~FaviconHelper() {}

static jint GetDominantColorForBitmap(JNIEnv* env,
                                      jclass clazz,
                                      jobject bitmap) {
  if (!bitmap)
    return 0;

    gfx::JavaBitmap bitmap_lock(bitmap);
    SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(bitmap_lock);
    return color_utils::CalculateKMeanColorOfBitmap(skbitmap);
}

// static
bool FaviconHelper::RegisterFaviconHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
