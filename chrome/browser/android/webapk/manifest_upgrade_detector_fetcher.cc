// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/manifest_upgrade_detector_fetcher.h"

#include <jni.h>
#include <vector>

#include "base/android/jni_string.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/webapk_icon_hasher.h"
#include "chrome/browser/android/webapk/webapk_web_manifest_checker.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/manifest.h"
#include "jni/ManifestUpgradeDetectorFetcher_jni.h"
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
  GURL web_manifest_url(base::android::ConvertJavaStringToUTF8(
      env, java_web_manifest_url));
  ManifestUpgradeDetectorFetcher* fetcher =
      new ManifestUpgradeDetectorFetcher(env, obj, scope, web_manifest_url);
  return reinterpret_cast<intptr_t>(fetcher);
}

ManifestUpgradeDetectorFetcher::ManifestUpgradeDetectorFetcher(
    JNIEnv* env,
    jobject obj,
    const GURL& scope,
    const GURL& web_manifest_url)
    : content::WebContentsObserver(nullptr),
      scope_(scope),
      web_manifest_url_(web_manifest_url),
      info_(GURL()),
      weak_ptr_factory_(this) {
  java_ref_.Reset(env, obj);
}

ManifestUpgradeDetectorFetcher::~ManifestUpgradeDetectorFetcher() {
}

// static
bool ManifestUpgradeDetectorFetcher::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ManifestUpgradeDetectorFetcher::ReplaceWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  content::WebContentsObserver::Observe(web_contents);
}

void ManifestUpgradeDetectorFetcher::Destroy(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  delete this;
}

void ManifestUpgradeDetectorFetcher::Start(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  ReplaceWebContents(env, obj, java_web_contents);
  if (!web_contents()->IsLoading())
    FetchInstallableData();
}

void ManifestUpgradeDetectorFetcher::DidStopLoading() {
  FetchInstallableData();
}

void ManifestUpgradeDetectorFetcher::FetchInstallableData() {
  GURL url = web_contents()->GetLastCommittedURL();

  // DidStopLoading() can be called multiple times for a single URL. Only fetch
  // installable data the first time.
  if (url == last_fetched_url_)
    return;
  last_fetched_url_ = url;

  if (!IsInScope(url, scope_))
    return;

  InstallableParams params;
  params.ideal_icon_size_in_dp =
      ShortcutHelper::GetIdealHomescreenIconSizeInDp();
  params.minimum_icon_size_in_dp =
      ShortcutHelper::GetMinimumHomescreenIconSizeInDp();
  params.check_installable = true;
  params.fetch_valid_icon = true;
  InstallableManager::CreateForWebContents(web_contents());
  InstallableManager* installable_manager =
      InstallableManager::FromWebContents(web_contents());
  installable_manager->GetData(
      params,
      base::Bind(&ManifestUpgradeDetectorFetcher::OnDidGetInstallableData,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManifestUpgradeDetectorFetcher::OnDidGetInstallableData(
    const InstallableData& data) {
  // If the manifest is empty, it means the current WebContents doesn't
  // associate with a Web Manifest. In such case, we ignore the empty manifest
  // and continue observing the WebContents's loading until we find a page that
  // links to the Web Manifest that we are looking for.
  // If the manifest URL is different from the current one, we will continue
  // observing too. It is based on our assumption that it is invalid for
  // web developers to change the Web Manifest location. When it does
  // change, we will treat the new Web Manifest as the one of another WebAPK.
  if (data.manifest.IsEmpty() || web_manifest_url_ != data.manifest_url)
    return;

  // TODO(pkotwicz): Tell Java side that the Web Manifest was fetched but the
  // Web Manifest is not WebAPK-compatible. (http://crbug.com/639536)
  if (data.error_code != NO_ERROR_DETECTED ||
      !AreWebManifestUrlsWebApkCompatible(data.manifest)) {
    return;
  }

  info_.UpdateFromManifest(data.manifest);
  info_.manifest_url = data.manifest_url;
  info_.icon_url = data.icon_url;
  icon_ = *data.icon;

  icon_hasher_.reset(new WebApkIconHasher());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  icon_hasher_->DownloadAndComputeMurmur2Hash(
      profile->GetRequestContext(),
      data.icon_url,
      base::Bind(&ManifestUpgradeDetectorFetcher::OnGotIconMurmur2Hash,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManifestUpgradeDetectorFetcher::OnGotIconMurmur2Hash(
    const std::string& icon_murmur2_hash) {
  icon_hasher_.reset();

  if (icon_murmur2_hash.empty()) {
    // TODO(pkotwicz): Tell Java side that the Web Manifest was fetched but the
    // Web Manifest is not WebAPK-compatible. (http://crbug.com/639536)
    return;
  }

  OnDataAvailable(info_, icon_murmur2_hash, icon_);
}

void ManifestUpgradeDetectorFetcher::OnDataAvailable(
    const ShortcutInfo& info,
    const std::string& icon_murmur2_hash,
    const SkBitmap& icon_bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_scope =
      base::android::ConvertUTF8ToJavaString(env, info.scope.spec());
  ScopedJavaLocalRef<jstring> java_name =
      base::android::ConvertUTF16ToJavaString(env, info.name);
  ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, info.short_name);
  ScopedJavaLocalRef<jstring> java_icon_url =
      base::android::ConvertUTF8ToJavaString(env, info.icon_url.spec());
  ScopedJavaLocalRef<jstring> java_icon_murmur2_hash =
      base::android::ConvertUTF8ToJavaString(env, icon_murmur2_hash);
  ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(&icon_bitmap);

  Java_ManifestUpgradeDetectorFetcher_onDataAvailable(
      env, java_ref_, java_url, java_scope, java_name, java_short_name,
      java_icon_url, java_icon_murmur2_hash, java_bitmap, info.display,
      info.orientation, info.theme_color, info.background_color);
}
