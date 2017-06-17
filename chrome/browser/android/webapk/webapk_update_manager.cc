// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_update_manager.h"

#include <jni.h>
#include <memory>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "jni/WebApkUpdateManager_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

using base::android::JavaRef;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

namespace {

// Called with the serialized proto to send to the WebAPK server.
void OnBuiltProto(const JavaRef<jobject>& java_callback,
                  std::unique_ptr<std::vector<uint8_t>> proto) {
  base::android::RunCallbackAndroid(java_callback, *proto);
}

// Called after the update either succeeds or fails.
void OnUpdated(const JavaRef<jobject>& java_callback,
               WebApkInstallResult result,
               bool relax_updates,
               const std::string& webapk_package) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkUpdateCallback_onResultFromNative(
      env, java_callback, static_cast<int>(result), relax_updates);
}

}  // anonymous namespace

// static
bool WebApkUpdateManager::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static JNI method.
static void BuildUpdateWebApkProto(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
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
    jboolean java_is_manifest_stale,
    const JavaParamRef<jobject>& java_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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

  WebApkInstaller::BuildProto(
      info, primary_icon, badge_icon, webapk_package,
      std::to_string(java_webapk_version), icon_url_to_murmur2_hash,
      java_is_manifest_stale,
      base::Bind(&OnBuiltProto, ScopedJavaGlobalRef<jobject>(java_callback)));
}

// static JNI method.
static void UpdateWebApk(JNIEnv* env,
                         const JavaParamRef<jclass>& clazz,
                         const JavaParamRef<jstring>& java_webapk_package,
                         const JavaParamRef<jstring>& java_start_url,
                         const JavaParamRef<jstring>& java_short_name,
                         const JavaParamRef<jbyteArray>& java_serialized_proto,
                         const JavaParamRef<jobject>& java_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScopedJavaGlobalRef<jobject> callback_ref(java_callback);

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&OnUpdated, callback_ref, WebApkInstallResult::FAILURE,
                   false /* relax_updates */, "" /* webapk_package */));
    return;
  }

  std::string webapk_package =
      ConvertJavaStringToUTF8(env, java_webapk_package);
  GURL start_url = GURL(ConvertJavaStringToUTF8(env, java_start_url));
  base::string16 short_name = ConvertJavaStringToUTF16(env, java_short_name);
  std::unique_ptr<std::vector<uint8_t>> serialized_proto =
      base::MakeUnique<std::vector<uint8_t>>();
  JavaByteArrayToByteVector(env, java_serialized_proto, serialized_proto.get());

  WebApkInstallService::Get(profile)->UpdateAsync(
      webapk_package, start_url, short_name, std::move(serialized_proto),
      base::Bind(&OnUpdated, callback_ref));
}
