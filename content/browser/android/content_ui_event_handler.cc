// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_ui_event_handler.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "jni/ContentUiEventHandler_jni.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/android/motion_event_android.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

ContentUiEventHandler::ContentUiEventHandler(JNIEnv* env,
                                             const JavaRef<jobject>& obj)
    : java_ref_(env, obj) {}

bool ContentUiEventHandler::OnKeyUp(const ui::KeyEventAndroid& event) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    return Java_ContentUiEventHandler_onKeyUp(env, j_obj, event.key_code(),
                                              event.GetJavaObject());
  }
  return false;
}

bool ContentUiEventHandler::DispatchKeyEvent(const ui::KeyEventAndroid& event) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    return Java_ContentUiEventHandler_dispatchKeyEvent(env, j_obj,
                                                       event.GetJavaObject());
  }
  return false;
}

void JNI_ContentUiEventHandler_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  CHECK(web_contents)
      << "A ContentUiEventHandler should be created with a valid WebContents.";
  static_cast<WebContentsViewAndroid*>(web_contents->GetView())
      ->SetContentUiEventHandler(
          std::make_unique<ContentUiEventHandler>(env, obj));
}

}  // namespace content
