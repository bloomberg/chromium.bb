// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/manifest/manifest_icon_downloader.h"
#include "chrome/common/chrome_switches.h"
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
      Java_ShortcutHelper_getHomeScreenIconAndSplashImageSizes(env);
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
void ShortcutHelper::AddToLauncherInBackgroundWithSkBitmap(
    const ShortcutInfo& info,
    const std::string& webapp_id,
    const SkBitmap& icon_bitmap,
    const base::Closure& splash_image_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (info.display == blink::WebDisplayModeStandalone ||
      info.display == blink::WebDisplayModeFullscreen) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableWebApk)) {
      InstallWebApkInBackgroundWithSkBitmap(info, webapp_id, icon_bitmap);
      return;
    }
    AddWebappInBackgroundWithSkBitmap(info, webapp_id, icon_bitmap,
                                      splash_image_callback);
    return;
  }
  AddShortcutInBackgroundWithSkBitmap(info, icon_bitmap);
}

// static
void ShortcutHelper::InstallWebApkInBackgroundWithSkBitmap(
    const ShortcutInfo& info,
    const std::string& webapp_id,
    const SkBitmap& icon_bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(pkotwicz): Send request to WebAPK server to generate WebAPK.

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_scope_url =
      base::android::ConvertUTF8ToJavaString(env, info.scope.spec());
  ScopedJavaLocalRef<jstring> java_name =
      base::android::ConvertUTF16ToJavaString(env, info.name);
  ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, info.short_name);
  ScopedJavaLocalRef<jstring> java_icon_url =
      base::android::ConvertUTF8ToJavaString(env, info.icon_url.spec());
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (icon_bitmap.getSize())
    java_bitmap = gfx::ConvertToJavaBitmap(&icon_bitmap);
  ScopedJavaLocalRef<jstring> java_manifest_url =
      base::android::ConvertUTF8ToJavaString(env, info.manifest_url.spec());

  Java_ShortcutHelper_installWebApk(
      env,
      java_url.obj(),
      java_scope_url.obj(),
      java_name.obj(),
      java_short_name.obj(),
      java_icon_url.obj(),
      java_bitmap.obj(),
      info.display,
      info.orientation,
      info.theme_color,
      info.background_color,
      java_manifest_url.obj());
}

// static
void ShortcutHelper::AddWebappInBackgroundWithSkBitmap(
    const ShortcutInfo& info,
    const std::string& webapp_id,
    const SkBitmap& icon_bitmap,
    const base::Closure& splash_image_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Send the data to the Java side to create the shortcut.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_webapp_id =
      base::android::ConvertUTF8ToJavaString(env, webapp_id);
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_scope_url =
      base::android::ConvertUTF8ToJavaString(env, info.scope.spec());
  ScopedJavaLocalRef<jstring> java_user_title =
      base::android::ConvertUTF16ToJavaString(env, info.user_title);
  ScopedJavaLocalRef<jstring> java_name =
      base::android::ConvertUTF16ToJavaString(env, info.name);
  ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, info.short_name);
  ScopedJavaLocalRef<jstring> java_icon_url =
      base::android::ConvertUTF8ToJavaString(env, info.icon_url.spec());
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (icon_bitmap.getSize())
    java_bitmap = gfx::ConvertToJavaBitmap(&icon_bitmap);

  // The callback will need to be run after shortcut creation completes in order
  // to download the splash image and save it to the WebappDataStorage. Create a
  // copy of the callback here and send the pointer to Java, which will send it
  // back once the asynchronous shortcut creation process finishes.
  uintptr_t callback_pointer =
      reinterpret_cast<uintptr_t>(new base::Closure(splash_image_callback));

  Java_ShortcutHelper_addWebapp(
      env,
      java_webapp_id.obj(),
      java_url.obj(),
      java_scope_url.obj(),
      java_user_title.obj(),
      java_name.obj(),
      java_short_name.obj(),
      java_icon_url.obj(),
      java_bitmap.obj(),
      info.display,
      info.orientation,
      info.source,
      info.theme_color,
      info.background_color,
      callback_pointer);
}

void ShortcutHelper::AddShortcutInBackgroundWithSkBitmap(
    const ShortcutInfo& info,
    const SkBitmap& icon_bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_user_title =
      base::android::ConvertUTF16ToJavaString(env, info.user_title);
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (icon_bitmap.getSize())
    java_bitmap = gfx::ConvertToJavaBitmap(&icon_bitmap);

  Java_ShortcutHelper_addShortcut(env, java_url.obj(), java_user_title.obj(),
                                  java_bitmap.obj(), info.source);
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
      web_contents, image_url, ideal_splash_image_size_in_dp,
      minimum_splash_image_size_in_dp,
      base::Bind(&ShortcutHelper::StoreWebappSplashImage, webapp_id));
}

// static
void ShortcutHelper::StoreWebappSplashImage(
    const std::string& webapp_id,
    const SkBitmap& splash_image) {
  if (splash_image.drawsNothing())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_webapp_id =
      base::android::ConvertUTF8ToJavaString(env, webapp_id);
  ScopedJavaLocalRef<jobject> java_splash_image =
      gfx::ConvertToJavaBitmap(&splash_image);

  Java_ShortcutHelper_storeWebappSplashImage(
      env,
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
    if (Java_ShortcutHelper_isIconLargeEnoughForLauncher(env, bitmap.width(),
                                                         bitmap.height())) {
      ScopedJavaLocalRef<jobject> java_bitmap =
          gfx::ConvertToJavaBitmap(&bitmap);
      result = Java_ShortcutHelper_createHomeScreenIconFromWebIcon(
          env, java_bitmap.obj());
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
        env, java_url.obj(), SkColorGetR(mean_color), SkColorGetG(mean_color),
        SkColorGetB(mean_color));
  }

  return result.obj()
             ? gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(result.obj()))
             : SkBitmap();
}

// static
bool ShortcutHelper::IsWebApkInstalled(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  return Java_ShortcutHelper_isWebApkInstalled(env, java_url.obj());
}

// Callback used by Java when the shortcut has been created.
// |splash_image_callback| is a pointer to a base::Closure allocated in
// AddShortcutInBackgroundWithSkBitmap, so reinterpret_cast it back and run it.
//
// This callback should only ever be called when the shortcut was for a
// webapp-capable site; otherwise, |splash_image_callback| will have never been
// allocated and doesn't need to be run or deleted.
void OnWebappDataStored(JNIEnv* env,
                        const JavaParamRef<jclass>& clazz,
                        jlong jsplash_image_callback) {
  DCHECK(jsplash_image_callback);
  base::Closure* splash_image_callback =
      reinterpret_cast<base::Closure*>(jsplash_image_callback);
  splash_image_callback->Run();
  delete splash_image_callback;
}

bool ShortcutHelper::RegisterShortcutHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
