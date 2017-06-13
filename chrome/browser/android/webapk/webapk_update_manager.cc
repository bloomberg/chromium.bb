// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_update_manager.h"

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "jni/WebApkUpdateManager_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

// static
bool WebApkUpdateManager::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void WebApkUpdateManager::OnBuiltWebApk(const std::string& id,
                                        WebApkInstallResult result,
                                        bool relax_updates,
                                        const std::string& webapk_package) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jstring> java_id =
      base::android::ConvertUTF8ToJavaString(env, id);
  Java_WebApkUpdateManager_onBuiltWebApk(
      env, java_id.obj(), static_cast<int>(result), relax_updates);
}

// static JNI method.
static void UpdateAsync(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& java_id,
    const JavaParamRef<jstring>& java_start_url,
    const JavaParamRef<jstring>& java_scope,
    const JavaParamRef<jstring>& java_name,
    const JavaParamRef<jstring>& java_short_name,
    const JavaParamRef<jstring>& java_primary_icon_url,
    const JavaParamRef<jobject>& java_primary_icon_bitmap,
    const JavaParamRef<jstring>& java_badge_icon_url,
    const JavaParamRef<jobject>& java_badge_icon_bitmap,
    const JavaParamRef<jobjectArray>& java_icon_urls,
    const JavaParamRef<jobjectArray>& java_icon_hashes,
    jint java_display_mode,
    jint java_orientation,
    jlong java_theme_color,
    jlong java_background_color,
    const JavaParamRef<jstring>& java_web_manifest_url,
    const JavaParamRef<jstring>& java_webapk_package,
    jint java_webapk_version,
    jboolean java_is_manifest_stale) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    NOTREACHED() << "Profile not found.";
    return;
  }

  std::string id(ConvertJavaStringToUTF8(env, java_id));
  ShortcutInfo info(GURL(ConvertJavaStringToUTF8(env, java_start_url)));
  info.scope = GURL(ConvertJavaStringToUTF8(env, java_scope));
  info.name = ConvertJavaStringToUTF16(env, java_name);
  info.short_name = ConvertJavaStringToUTF16(env, java_short_name);
  info.user_title = info.short_name;
  info.display = static_cast<blink::WebDisplayMode>(java_display_mode);
  info.orientation =
      static_cast<blink::WebScreenOrientationLockType>(java_orientation);
  info.theme_color = (int64_t)java_theme_color;
  info.background_color = (int64_t)java_background_color;
  info.best_primary_icon_url =
      GURL(ConvertJavaStringToUTF8(env, java_primary_icon_url));
  info.best_badge_icon_url =
      GURL(ConvertJavaStringToUTF8(env, java_badge_icon_url));
  info.manifest_url = GURL(ConvertJavaStringToUTF8(env, java_web_manifest_url));

  base::android::AppendJavaStringArrayToStringVector(env, java_icon_urls.obj(),
                                                     &info.icon_urls);

  std::vector<std::string> icon_hashes;
  base::android::AppendJavaStringArrayToStringVector(
      env, java_icon_hashes.obj(), &icon_hashes);

  std::map<std::string, std::string> icon_url_to_murmur2_hash;
  for (size_t i = 0; i < info.icon_urls.size(); ++i)
    icon_url_to_murmur2_hash[info.icon_urls[i]] = icon_hashes[i];

  gfx::JavaBitmap java_primary_icon_bitmap_lock(java_primary_icon_bitmap);
  SkBitmap primary_icon =
      gfx::CreateSkBitmapFromJavaBitmap(java_primary_icon_bitmap_lock);
  primary_icon.setImmutable();

  gfx::JavaBitmap java_badge_icon_bitmap_lock(java_badge_icon_bitmap);
  SkBitmap badge_icon =
      gfx::CreateSkBitmapFromJavaBitmap(java_badge_icon_bitmap_lock);
  badge_icon.setImmutable();

  std::string webapk_package;
  ConvertJavaStringToUTF8(env, java_webapk_package, &webapk_package);

  WebApkInstallService* install_service = WebApkInstallService::Get(profile);
  if (install_service->IsInstallInProgress(info.manifest_url)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&WebApkUpdateManager::OnBuiltWebApk, id,
                   WebApkInstallResult::FAILURE, false /* relax_updates */,
                   "" /* webapk_package */));
    return;
  }
  install_service->UpdateAsync(
      info, primary_icon, badge_icon, webapk_package, java_webapk_version,
      icon_url_to_murmur2_hash, java_is_manifest_stale,
      base::Bind(&WebApkUpdateManager::OnBuiltWebApk, id));
}
