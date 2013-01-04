// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/navigation_popup.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/ui_resources.h"
#include "jni/NavigationPopup_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

NavigationPopup::NavigationPopup(JNIEnv* env, jobject obj)
    : weak_jobject_(env, obj) {
}

NavigationPopup::~NavigationPopup() {
}

void NavigationPopup::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void NavigationPopup::FetchFaviconForUrl(JNIEnv* env,
                                         jobject obj,
                                         jstring jurl) {
  Profile* profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;
  GURL url(base::android::ConvertJavaStringToUTF16(env, jurl));
  // TODO(tedchoc): Request higher favicons based on screen density instead of
  //                hardcoding kFaviconSize.
  favicon_service->GetFaviconImageForURL(
      FaviconService::FaviconForURLParams(profile,
                                          url,
                                          history::FAVICON,
                                          gfx::kFaviconSize),
      base::Bind(&NavigationPopup::OnFaviconDataAvailable,
                 base::Unretained(this),
                 url),
      &cancelable_task_tracker_);
}

void NavigationPopup::OnFaviconDataAvailable(
    GURL navigation_entry_url,
    const history::FaviconImageResult& image_result) {
  gfx::Image image(image_result.image);
  if (image.IsEmpty()) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    image = rb.GetImageNamed(IDR_DEFAULT_FAVICON);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_jobject_.get(env));
  if (!obj.obj())
    return;

  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, navigation_entry_url.spec()));

  Java_NavigationPopup_onFaviconUpdated(
      env,
      obj.obj(),
      jurl.obj(),
      gfx::ConvertToJavaBitmap(image.ToSkBitmap()).obj());
}

static jstring GetHistoryUrl(JNIEnv* env, jclass clazz) {
  return ConvertUTF8ToJavaString(env, chrome::kChromeUIHistoryURL).Release();
}

static jint Init(JNIEnv* env, jobject obj) {
  NavigationPopup* popup = new NavigationPopup(env, obj);
  return reinterpret_cast<jint>(popup);
}

// static
bool NavigationPopup::RegisterNavigationPopup(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
