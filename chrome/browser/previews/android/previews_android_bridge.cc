// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/android/previews_android_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "jni/PreviewsAndroidBridge_jni.h"

static jlong JNI_PreviewsAndroidBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new PreviewsAndroidBridge(env, obj));
}

PreviewsAndroidBridge::PreviewsAndroidBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj)
    : weak_factory_(this) {}

PreviewsAndroidBridge::~PreviewsAndroidBridge() {}

jboolean PreviewsAndroidBridge::ShouldShowPreviewUI(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return false;

  PreviewsUITabHelper* tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return false;

  return tab_helper->should_display_android_omnibox_badge();
}
