// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/find_in_page/find_in_page_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "jni/FindInPageBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;

FindInPageBridge::FindInPageBridge(JNIEnv* env,
                                   jobject obj,
                                   jobject j_web_contents)
    : weak_java_ref_(env, obj) {
  web_contents_ = content::WebContents::FromJavaWebContents(j_web_contents);
}

void FindInPageBridge::Destroy(JNIEnv*, jobject) {
  delete this;
}

void FindInPageBridge::StartFinding(JNIEnv* env,
                                    jobject obj,
                                    jstring search_string,
                                    jboolean forward_direction,
                                    jboolean case_sensitive) {
  FindTabHelper::FromWebContents(web_contents_)->
      StartFinding(
          base::android::ConvertJavaStringToUTF16(env, search_string),
          forward_direction,
          case_sensitive);
}

void FindInPageBridge::StopFinding(JNIEnv* env, jobject obj) {
  FindTabHelper::FromWebContents(web_contents_)->
      StopFinding(FindBarController::kClearSelectionOnPage);
}

ScopedJavaLocalRef<jstring> FindInPageBridge::GetPreviousFindText(JNIEnv* env,
                                                                  jobject obj) {
  return ConvertUTF16ToJavaString(
      env, FindTabHelper::FromWebContents(web_contents_)->previous_find_text());
}

void FindInPageBridge::RequestFindMatchRects(JNIEnv* env,
                                             jobject obj,
                                             jint current_version) {
  FindTabHelper::FromWebContents(web_contents_)->
      RequestFindMatchRects(current_version);
}

void FindInPageBridge::ActivateNearestFindResult(JNIEnv* env,
                                                 jobject obj,
                                                 jfloat x,
                                                 jfloat y) {
  FindTabHelper::FromWebContents(web_contents_)->
      ActivateNearestFindResult(x, y);
}

void FindInPageBridge::ActivateFindInPageResultForAccessibility(
    JNIEnv* env, jobject obj) {
  FindTabHelper::FromWebContents(web_contents_)->
      ActivateFindInPageResultForAccessibility();
}

// static
static jlong Init(JNIEnv* env, jobject obj, jobject j_web_contents) {
  FindInPageBridge* bridge = new FindInPageBridge(env, obj, j_web_contents);
  return reinterpret_cast<intptr_t>(bridge);
}

bool FindInPageBridge::RegisterFindInPageBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
