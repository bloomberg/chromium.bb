// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/transition_page_helper.h"

#include "cc/layers/layer.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/TransitionPageHelper_jni.h"

// TODO(simonhatch): Why does 1.f opacity seem to kill per-pixel alpha?
const float TRANSITION_MAX_OPACITY = 0.99999f;

TransitionPageHelper* TransitionPageHelper::FromJavaObject(JNIEnv* env,
                                                           jobject jobj) {
  return reinterpret_cast<TransitionPageHelper*>(
      Java_TransitionPageHelper_getNativePtr(env, jobj));
}

TransitionPageHelper::TransitionPageHelper() {
}

TransitionPageHelper::~TransitionPageHelper() {
}

void TransitionPageHelper::Destroy(JNIEnv* env, jobject jobj) {
  delete this;
}

void TransitionPageHelper::SetWebContents(JNIEnv* env,
                                          jobject jobj,
                                          jobject jcontent_view_core) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  DCHECK(content_view_core);
  DCHECK(content_view_core->GetWebContents());

  web_contents_.reset(content_view_core->GetWebContents());
}

void TransitionPageHelper::ReleaseWebContents(JNIEnv* env, jobject jobj) {
  DCHECK(web_contents_.get());
  web_contents_.reset();
}

void TransitionPageHelper::SetOpacity(JNIEnv* env,
                                      jobject jobj,
                                      jobject jcontent_view_core,
                                      jfloat opacity) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  if (!content_view_core)
    return;

  content_view_core->GetLayer()->SetOpacity(
      std::min(opacity, TRANSITION_MAX_OPACITY));
}

static jlong Init(JNIEnv* env, jobject obj) {
  return reinterpret_cast<long>(new TransitionPageHelper());
}

bool TransitionPageHelper::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
