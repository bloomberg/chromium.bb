// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/favicon_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/FaviconHelper_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::MethodID;

namespace {

void FaviconImageCallback(
    ScopedJavaGlobalRef<jobject>* java_favicon_image_callback,
    const chrome::FaviconImageResult& favicon_image_result) {
  JNIEnv* env = AttachCurrentThread();

  // Convert favicon_image_result to java objects.
  ScopedJavaLocalRef<jstring> java_icon_url = ConvertUTF8ToJavaString(
      env, favicon_image_result.icon_url.spec());
  SkBitmap favicon_bitmap = favicon_image_result.image.AsBitmap();
  ScopedJavaLocalRef<jobject> java_favicon_bitmap;
  if (!favicon_bitmap.isNull()) {
    java_favicon_bitmap = gfx::ConvertToJavaBitmap(&favicon_bitmap);
  }

  // Call java side FaviconImageCallback method.
  Java_FaviconImageCallback_onFaviconAvailable(
      env, java_favicon_image_callback->obj(), java_favicon_bitmap.obj(),
      java_icon_url.obj());
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

jboolean FaviconHelper::GetFaviconImageForURL(
    JNIEnv* env, jobject obj, jobject jprofile, jstring page_url,
    jint icon_types, jint desired_size_in_dip,
    jobject java_favicon_image_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  if (!profile)
    return false;

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  DCHECK(favicon_service);
  if (!favicon_service)
    return false;

  FaviconService::FaviconForURLParams params(
      profile, GURL(ConvertJavaStringToUTF16(env, page_url)),
      static_cast<int>(icon_types), static_cast<int>(desired_size_in_dip));

  ScopedJavaGlobalRef<jobject>* j_scoped_favicon_callback =
      new ScopedJavaGlobalRef<jobject>();
  j_scoped_favicon_callback->Reset(env, java_favicon_image_callback);

  FaviconService::FaviconImageCallback callback_runner = base::Bind(
      &FaviconImageCallback, base::Owned(j_scoped_favicon_callback));

  favicon_service->GetFaviconImageForURL(
      params, callback_runner,
      cancelable_task_tracker_.get());

  return true;
}

FaviconHelper::~FaviconHelper() {
}

// static
bool FaviconHelper::RegisterFaviconHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
