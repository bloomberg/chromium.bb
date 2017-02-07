// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_update_data_fetcher.h"

#include <jni.h>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/webapk_icon_hasher.h"
#include "chrome/browser/android/webapk/webapk_web_manifest_checker.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/manifest.h"
#include "jni/WebApkUpdateDataFetcher_jni.h"
#include "third_party/smhasher/src/MurmurHash2.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Returns whether the given |url| is within the scope of the |scope| url.
bool IsInScope(const GURL& url, const GURL& scope) {
  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
}

}  // anonymous namespace

jlong Initialize(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 const JavaParamRef<jstring>& java_scope_url,
                 const JavaParamRef<jstring>& java_web_manifest_url) {
  GURL scope(base::android::ConvertJavaStringToUTF8(env, java_scope_url));
  GURL web_manifest_url(
      base::android::ConvertJavaStringToUTF8(env, java_web_manifest_url));
  WebApkUpdateDataFetcher* fetcher =
      new WebApkUpdateDataFetcher(env, obj, scope, web_manifest_url);
  return reinterpret_cast<intptr_t>(fetcher);
}

WebApkUpdateDataFetcher::WebApkUpdateDataFetcher(JNIEnv* env,
                                                 jobject obj,
                                                 const GURL& scope,
                                                 const GURL& web_manifest_url)
    : content::WebContentsObserver(nullptr),
      scope_(scope),
      web_manifest_url_(web_manifest_url),
      is_initial_fetch_(false),
      info_(GURL()),
      weak_ptr_factory_(this) {
  java_ref_.Reset(env, obj);
}

WebApkUpdateDataFetcher::~WebApkUpdateDataFetcher() {}

// static
bool WebApkUpdateDataFetcher::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void WebApkUpdateDataFetcher::ReplaceWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  content::WebContentsObserver::Observe(web_contents);
}

void WebApkUpdateDataFetcher::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  delete this;
}

void WebApkUpdateDataFetcher::Start(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  ReplaceWebContents(env, obj, java_web_contents);
  if (!web_contents()->IsLoading())
    FetchInstallableData();
}

void WebApkUpdateDataFetcher::DidStopLoading() {
  FetchInstallableData();
}

void WebApkUpdateDataFetcher::FetchInstallableData() {
  GURL url = web_contents()->GetLastCommittedURL();

  // DidStopLoading() can be called multiple times for a single URL. Only fetch
  // installable data the first time.
  if (url == last_fetched_url_)
    return;
  is_initial_fetch_ = last_fetched_url_.is_empty();
  last_fetched_url_ = url;

  if (!IsInScope(url, scope_)) {
    OnWebManifestNotWebApkCompatible();
    return;
  }

  InstallableParams params;
  params.ideal_primary_icon_size_in_px =
      ShortcutHelper::GetIdealHomescreenIconSizeInPx();
  params.minimum_primary_icon_size_in_px =
      ShortcutHelper::GetMinimumHomescreenIconSizeInPx();
  params.check_installable = true;
  params.fetch_valid_primary_icon = true;
  InstallableManager::CreateForWebContents(web_contents());
  InstallableManager* installable_manager =
      InstallableManager::FromWebContents(web_contents());
  installable_manager->GetData(
      params, base::Bind(&WebApkUpdateDataFetcher::OnDidGetInstallableData,
                         weak_ptr_factory_.GetWeakPtr()));
}

void WebApkUpdateDataFetcher::OnDidGetInstallableData(
    const InstallableData& data) {
  // Determine whether or not the manifest is WebAPK-compatible. There are 3
  // cases:
  // 1. the site isn't installable.
  // 2. the URLs in the manifest expose passwords.
  // 3. there is no manifest or the manifest is different to the one we're
  // expecting.
  // For case 3, if the manifest is empty, it means the current WebContents
  // doesn't associate with a Web Manifest. In such case, we ignore the empty
  // manifest and continue observing the WebContents's loading until we find a
  // page that links to the Web Manifest that we are looking for.
  // If the manifest URL is different from the current one, we will continue
  // observing too. It is based on our assumption that it is invalid for
  // web developers to change the Web Manifest location. When it does
  // change, we will treat the new Web Manifest as the one of another WebAPK.
  if (data.error_code != NO_ERROR_DETECTED || data.manifest.IsEmpty() ||
      web_manifest_url_ != data.manifest_url ||
      !AreWebManifestUrlsWebApkCompatible(data.manifest)) {
    OnWebManifestNotWebApkCompatible();
    return;
  }

  info_.UpdateFromManifest(data.manifest);
  info_.manifest_url = data.manifest_url;
  info_.best_primary_icon_url = data.primary_icon_url;
  best_primary_icon_ = *data.primary_icon;

  icon_hasher_.reset(new WebApkIconHasher());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  icon_hasher_->DownloadAndComputeMurmur2Hash(
      profile->GetRequestContext(), data.primary_icon_url,
      base::Bind(&WebApkUpdateDataFetcher::OnGotIconMurmur2Hash,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebApkUpdateDataFetcher::OnGotIconMurmur2Hash(
    const std::string& best_primary_icon_murmur2_hash) {
  icon_hasher_.reset();

  if (best_primary_icon_murmur2_hash.empty()) {
    OnWebManifestNotWebApkCompatible();
    return;
  }

  OnDataAvailable(info_, best_primary_icon_murmur2_hash, best_primary_icon_);
}

void WebApkUpdateDataFetcher::OnDataAvailable(
    const ShortcutInfo& info,
    const std::string& best_primary_icon_murmur2_hash,
    const SkBitmap& best_primary_icon) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_scope =
      base::android::ConvertUTF8ToJavaString(env, info.scope.spec());
  ScopedJavaLocalRef<jstring> java_name =
      base::android::ConvertUTF16ToJavaString(env, info.name);
  ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, info.short_name);
  ScopedJavaLocalRef<jstring> java_best_primary_icon_url =
      base::android::ConvertUTF8ToJavaString(env,
                                             info.best_primary_icon_url.spec());
  ScopedJavaLocalRef<jstring> java_best_primary_icon_murmur2_hash =
      base::android::ConvertUTF8ToJavaString(env,
                                             best_primary_icon_murmur2_hash);
  ScopedJavaLocalRef<jobject> java_best_primary_icon =
      gfx::ConvertToJavaBitmap(&best_primary_icon);

  ScopedJavaLocalRef<jobjectArray> java_icon_urls =
      base::android::ToJavaArrayOfStrings(env, info.icon_urls);

  Java_WebApkUpdateDataFetcher_onDataAvailable(
      env, java_ref_, java_url, java_scope, java_name, java_short_name,
      java_best_primary_icon_url, java_best_primary_icon_murmur2_hash,
      java_best_primary_icon, java_icon_urls, info.display, info.orientation,
      info.theme_color, info.background_color);
}

void WebApkUpdateDataFetcher::OnWebManifestNotWebApkCompatible() {
  if (!is_initial_fetch_)
    return;

  Java_WebApkUpdateDataFetcher_onWebManifestForInitialUrlNotWebApkCompatible(
      base::android::AttachCurrentThread(), java_ref_);
}
