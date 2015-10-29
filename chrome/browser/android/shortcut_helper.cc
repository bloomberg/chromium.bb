// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/manifest/manifest_icon_downloader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/ShortcutHelper_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/color_analysis.h"
#include "url/gurl.h"

using content::Manifest;

namespace {

static int kIdealHomescreenIconSize = -1;
static int kMinimumHomescreenIconSize = -1;
static int kIdealSplashImageSize = -1;
static int kMinimumSplashImageSize = -1;

static int kDefaultRGBIconValue = 145;

// Retrieves and caches the ideal and minimum sizes of the Home screen icon
// and the splash screen image.
void GetHomescreenIconAndSplashImageSizes() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jintArray> java_size_array =
      Java_ShortcutHelper_getHomeScreenIconAndSplashImageSizes(env,
          base::android::GetApplicationContext());
  std::vector<int> sizes;
  base::android::JavaIntArrayToIntVector(
      env, java_size_array.obj(), &sizes);

  // Check that the size returned is what is expected.
  DCHECK(sizes.size() == 4);

  // This ordering must be kept up to date with the Java ShortcutHelper.
  kIdealHomescreenIconSize = sizes[0];
  kMinimumHomescreenIconSize = sizes[1];
  kIdealSplashImageSize = sizes[2];
  kMinimumSplashImageSize = sizes[3];

  // Try to ensure that the data returned is sane.
  DCHECK(kMinimumHomescreenIconSize <= kIdealHomescreenIconSize);
  DCHECK(kMinimumSplashImageSize <= kIdealSplashImageSize);
}

} // anonymous namespace

// static
void ShortcutHelper::AddShortcutInBackgroundWithSkBitmap(
    const ShortcutInfo& info,
    const std::string& webapp_id,
    const SkBitmap& icon_bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Send the data to the Java side to create the shortcut.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_webapp_id =
      base::android::ConvertUTF8ToJavaString(env, webapp_id);
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
      java_webapp_id.obj(),
      java_url.obj(),
      java_user_title.obj(),
      java_name.obj(),
      java_short_name.obj(),
      java_bitmap.obj(),
      info.display == blink::WebDisplayModeStandalone,
      info.orientation,
      info.source,
      info.theme_color,
      info.background_color,
      info.is_icon_generated);
}

int ShortcutHelper::GetIdealHomescreenIconSizeInDp() {
  if (kIdealHomescreenIconSize == -1)
    GetHomescreenIconAndSplashImageSizes();
  return kIdealHomescreenIconSize;
}

int ShortcutHelper::GetMinimumHomescreenIconSizeInDp() {
  if (kMinimumHomescreenIconSize == -1)
    GetHomescreenIconAndSplashImageSizes();
  return kMinimumHomescreenIconSize;
}

int ShortcutHelper::GetIdealSplashImageSizeInDp() {
  if (kIdealSplashImageSize == -1)
    GetHomescreenIconAndSplashImageSizes();
  return kIdealSplashImageSize;
}

int ShortcutHelper::GetMinimumSplashImageSizeInDp() {
  if (kMinimumSplashImageSize == -1)
    GetHomescreenIconAndSplashImageSizes();
  return kMinimumSplashImageSize;
}

// static
void ShortcutHelper::FetchSplashScreenImage(
    content::WebContents* web_contents,
    const GURL& image_url,
    const int ideal_splash_image_size_in_dp,
    const int minimum_splash_image_size_in_dp,
    const std::string& webapp_id) {
  // This is a fire and forget task. It is not vital for the splash screen image
  // to be downloaded so if the downloader returns false there is no fallback.
  ManifestIconDownloader::Download(
      web_contents,
      image_url,
      ideal_splash_image_size_in_dp,
      minimum_splash_image_size_in_dp,
      base::Bind(&ShortcutHelper::StoreWebappData, webapp_id));
}

// static
void ShortcutHelper::StoreWebappData(
    const std::string& webapp_id,
    const SkBitmap& splash_image) {
  if (splash_image.drawsNothing())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_webapp_id =
      base::android::ConvertUTF8ToJavaString(env, webapp_id);
  ScopedJavaLocalRef<jobject> java_splash_image =
      gfx::ConvertToJavaBitmap(&splash_image);

  Java_ShortcutHelper_storeWebappData(
      env,
      base::android::GetApplicationContext(),
      java_webapp_id.obj(),
      java_splash_image.obj());
}

// static
SkBitmap ShortcutHelper::FinalizeLauncherIcon(const SkBitmap& bitmap,
                                              const GURL& url,
                                              bool* is_generated) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result;
  *is_generated = false;

  if (!bitmap.isNull()) {
    if (Java_ShortcutHelper_isIconLargeEnoughForLauncher(
            env, base::android::GetApplicationContext(), bitmap.width(),
            bitmap.height())) {
      ScopedJavaLocalRef<jobject> java_bitmap =
          gfx::ConvertToJavaBitmap(&bitmap);
      result = Java_ShortcutHelper_createHomeScreenIconFromWebIcon(
          env, base::android::GetApplicationContext(), java_bitmap.obj());
    }
  }

  if (result.is_null()) {
    ScopedJavaLocalRef<jstring> java_url =
        base::android::ConvertUTF8ToJavaString(env, url.spec());
    SkColor mean_color = SkColorSetRGB(
        kDefaultRGBIconValue, kDefaultRGBIconValue, kDefaultRGBIconValue);

    if (!bitmap.isNull())
      mean_color = color_utils::CalculateKMeanColorOfBitmap(bitmap);

    *is_generated = true;
    result = Java_ShortcutHelper_generateHomeScreenIcon(
        env, base::android::GetApplicationContext(), java_url.obj(),
        SkColorGetR(mean_color), SkColorGetG(mean_color),
        SkColorGetB(mean_color));
  }

  return gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(result.obj()));
}

bool ShortcutHelper::RegisterShortcutHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
