// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/selection_popup_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/public/browser/web_contents.h"
#include "jni/SelectionPopupController_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

void Init(JNIEnv* env,
          const JavaParamRef<jobject>& obj,
          const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  // Owns itself and gets destroyed when |WebContentsDestroyed| is called.
  (new SelectionPopupController(env, obj, web_contents))->Initialize();
}

bool RegisterSelectionPopupController(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

SelectionPopupController::SelectionPopupController(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    WebContents* web_contents)
    : RenderWidgetHostConnector(web_contents) {
  java_obj_ = JavaObjectWeakGlobalRef(env, obj);
}

void SelectionPopupController::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (old_rwhva)
    old_rwhva->set_selection_popup_controller(nullptr);
  if (new_rwhva)
    new_rwhva->set_selection_popup_controller(this);
}

void SelectionPopupController::OnSelectionEvent(
    ui::SelectionEventType event,
    const gfx::RectF& selection_rect) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return;

  Java_SelectionPopupController_onSelectionEvent(
      env, obj, event, selection_rect.x(), selection_rect.y(),
      selection_rect.right(), selection_rect.bottom());
}

void SelectionPopupController::OnSelectionChanged(const std::string& text) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtext = ConvertUTF8ToJavaString(env, text);
  Java_SelectionPopupController_onSelectionChanged(env, obj, jtext);
}

}  // namespace content
