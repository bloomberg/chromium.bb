// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/manifest.h"
#include "jni/ShortcutHelper_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/color_analysis.h"
#include "url/gurl.h"

using content::Manifest;

jlong Initialize(JNIEnv* env, jobject obj, jobject java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  ShortcutHelper* shortcut_helper = new ShortcutHelper(env, obj, web_contents);
  return reinterpret_cast<intptr_t>(shortcut_helper);
}

ShortcutHelper::ShortcutHelper(JNIEnv* env,
                               jobject obj,
                               content::WebContents* web_contents)
    : add_shortcut_pending_(false),
      data_fetcher_(new ShortcutDataFetcher(web_contents, this)) {
  java_ref_.Reset(env, obj);
}

ShortcutHelper::~ShortcutHelper() {
  data_fetcher_->set_weak_observer(nullptr);
  data_fetcher_ = nullptr;
}

void ShortcutHelper::OnUserTitleAvailable(const base::string16& user_title) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_user_title =
      base::android::ConvertUTF16ToJavaString(env, user_title);
  Java_ShortcutHelper_onUserTitleAvailable(env,
                                           java_ref_.obj(),
                                           j_user_title.obj());
}

void ShortcutHelper::OnDataAvailable(const ShortcutInfo& info,
                                     const SkBitmap& icon) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (icon.getSize())
    java_bitmap = gfx::ConvertToJavaBitmap(&icon);

  Java_ShortcutHelper_onIconAvailable(env,
                                      java_ref_.obj(),
                                      java_bitmap.obj());

  if (add_shortcut_pending_)
    AddShortcut(info, icon);
}

void ShortcutHelper::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

SkBitmap ShortcutHelper::FinalizeLauncherIcon(const SkBitmap& bitmap,
                                              const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Determine a single color to use for the favicon if the favicon that is
  // returned it is too low quality.
  SkColor color = color_utils::CalculateKMeanColorOfBitmap(bitmap);
  int dominant_red = SkColorGetR(color);
  int dominant_green = SkColorGetG(color);
  int dominant_blue = SkColorGetB(color);

  // Make the icon acceptable for the Android launcher.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (bitmap.getSize())
    java_bitmap = gfx::ConvertToJavaBitmap(&bitmap);

  base::android::ScopedJavaLocalRef<jobject> ref =
      Java_ShortcutHelper_finalizeLauncherIcon(env,
                                               java_url.obj(),
                                               java_bitmap.obj(),
                                               dominant_red,
                                               dominant_green,
                                               dominant_blue);
  return gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(ref.obj()));
}

void ShortcutHelper::AddShortcut(JNIEnv* env,
                                 jobject obj,
                                 jstring j_user_title) {
  add_shortcut_pending_ = true;

  base::string16 user_title =
      base::android::ConvertJavaStringToUTF16(env, j_user_title);
  if (!user_title.empty())
    data_fetcher_->shortcut_info().user_title = user_title;

  if (data_fetcher_->is_ready()) {
    // If the fetcher isn't ready yet, the shortcut will be added when it is
    // via OnDataAvailable();
    AddShortcut(data_fetcher_->shortcut_info(), data_fetcher_->shortcut_icon());
  }
}

void ShortcutHelper::AddShortcut(const ShortcutInfo& info,
                                 const SkBitmap& icon) {
  DCHECK(add_shortcut_pending_);
  if (!add_shortcut_pending_)
    return;
  add_shortcut_pending_ = false;

  RecordAddToHomescreen();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ShortcutHelper::AddShortcutInBackgroundWithSkBitmap,
                 info,
                 icon));
}

bool ShortcutHelper::RegisterShortcutHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void ShortcutHelper::AddShortcutInBackgroundWithSkBitmap(
    const ShortcutInfo& info,
    const SkBitmap& icon_bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Send the data to the Java side to create the shortcut.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_user_title =
      base::android::ConvertUTF16ToJavaString(env, info.user_title);
  ScopedJavaLocalRef<jstring> java_name =
      base::android::ConvertUTF16ToJavaString(env, info.name);
  ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, info.short_name);
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (icon_bitmap.getSize())
    java_bitmap = gfx::ConvertToJavaBitmap(&icon_bitmap);

  Java_ShortcutHelper_addShortcut(
      env,
      base::android::GetApplicationContext(),
      java_url.obj(),
      java_user_title.obj(),
      java_name.obj(),
      java_short_name.obj(),
      java_bitmap.obj(),
      info.display == content::Manifest::DISPLAY_MODE_STANDALONE,
      info.orientation,
      info.source,
      info.theme_color);
}

void ShortcutHelper::RecordAddToHomescreen() {
  // Record that the shortcut has been added, so no banners will be shown
  // for this app.
  content::WebContents* web_contents = data_fetcher_->web_contents();
  if (!web_contents)
    return;

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents, web_contents->GetURL(),
      data_fetcher_->shortcut_info().url.spec(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      base::Time::Now());
}
