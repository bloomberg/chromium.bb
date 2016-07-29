// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/manifest_upgrade_detector.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "chrome/browser/android/shortcut_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/manifest.h"
#include "jni/ManifestUpgradeDetector_jni.h"
#include "url/gurl.h"

namespace {

// Returns whether the given |url| is within the scope of the |scope| url.
bool IsInScope(const GURL& url, const GURL& scope) {
  return base::StartsWith(url.spec(), scope.spec(),
                         base::CompareCase::SENSITIVE);
}

}  // anonymous namespace

jlong Initialize(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 const JavaParamRef<jobject>& java_web_contents,
                 const JavaParamRef<jstring>& java_scope_url,
                 const JavaParamRef<jstring>& java_web_manifest_url) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  GURL scope(base::android::ConvertJavaStringToUTF8(env, java_scope_url));
  GURL web_manifest_url(base::android::ConvertJavaStringToUTF8(
      env, java_web_manifest_url));
  ManifestUpgradeDetector* manifest_upgrade_detector =
      new ManifestUpgradeDetector(env, obj, web_contents, scope,
                                  web_manifest_url);
  return reinterpret_cast<intptr_t>(manifest_upgrade_detector);
}

ManifestUpgradeDetector::ManifestUpgradeDetector(
    JNIEnv* env,
    jobject obj,
    content::WebContents* web_contents,
    const GURL& scope,
    const GURL& web_manifest_url)
    : content::WebContentsObserver(web_contents),
      started_(false),
      scope_(scope),
      web_manifest_url_(web_manifest_url),
      weak_ptr_factory_(this) {
  java_ref_.Reset(env, obj);
}

ManifestUpgradeDetector::~ManifestUpgradeDetector() {
}

// static
bool ManifestUpgradeDetector::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ManifestUpgradeDetector::ReplaceWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  content::WebContentsObserver::Observe(web_contents);
}

void ManifestUpgradeDetector::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  delete this;
}

void ManifestUpgradeDetector::Start(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  started_ = true;
}

void ManifestUpgradeDetector::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->GetParent())
    return;
  if (!started_ || !IsInScope(validated_url, scope_))
    return;

  web_contents()->GetManifest(
      base::Bind(&ManifestUpgradeDetector::OnDidGetManifest,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManifestUpgradeDetector::OnDidGetManifest(
    const GURL& manifest_url,
    const content::Manifest& manifest) {
  // If the manifest is empty, it means the current WebContents doesn't
  // associate with a Web Manifest. In such case, we ignore the empty manifest
  // and continue observing the WebContents's loading until we find a page that
  // links to the Web Manifest that we are looking for.
  // If the manifest URL is different from the current one, we will continue
  // observing too. It is based on our assumption that it is invalid for
  // web developers to change the Web Manifest location. When it does
  // change, we will treat the new Web Manifest as the one of another WebAPK.
  if (manifest.IsEmpty() || web_manifest_url_ != manifest_url)
    return;

  started_ = false;

  ShortcutInfo info(GURL::EmptyGURL());
  info.UpdateFromManifest(manifest);
  info.manifest_url = manifest_url;

  OnDataAvailable(info);
}

void ManifestUpgradeDetector::OnDataAvailable(const ShortcutInfo& info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_scope =
      base::android::ConvertUTF8ToJavaString(env, info.scope.spec());
  ScopedJavaLocalRef<jstring> java_name =
      base::android::ConvertUTF16ToJavaString(env, info.name);
  ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, info.short_name);

  Java_ManifestUpgradeDetector_onDataAvailable(
      env, java_ref_.obj(),
      java_url.obj(),
      java_scope.obj(),
      java_name.obj(),
      java_short_name.obj(),
      info.display,
      info.orientation,
      info.theme_color,
      info.background_color);
}
