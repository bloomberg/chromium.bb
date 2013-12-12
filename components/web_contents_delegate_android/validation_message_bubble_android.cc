// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_contents_delegate_android/validation_message_bubble_android.h"

#include "base/android/jni_string.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/ValidationMessageBubble_jni.h"
#include "ui/gfx/rect.h"

using base::android::ConvertUTF16ToJavaString;
using content::ContentViewCore;
using content::RenderWidgetHost;

namespace {

inline ContentViewCore* GetContentViewCoreFrom(RenderWidgetHost* widget_host) {
  return ContentViewCore::FromWebContents(
      content::WebContents::FromRenderViewHost(
          content::RenderViewHost::From(widget_host)));
}

}

namespace web_contents_delegate_android {

ValidationMessageBubbleAndroid::ValidationMessageBubbleAndroid(
    RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_root_view,
    const base::string16& main_text,
    const base::string16& sub_text) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_validation_message_bubble_.Reset(
      Java_ValidationMessageBubble_createAndShow(
          env,
          GetContentViewCoreFrom(widget_host)->GetJavaObject().obj(),
          anchor_in_root_view.x(),
          anchor_in_root_view.y(),
          anchor_in_root_view.width(),
          anchor_in_root_view.height(),
          ConvertUTF16ToJavaString(env, main_text).obj(),
          ConvertUTF16ToJavaString(env, sub_text).obj()));
}

ValidationMessageBubbleAndroid::~ValidationMessageBubbleAndroid() {
  Java_ValidationMessageBubble_close(base::android::AttachCurrentThread(),
                                     java_validation_message_bubble_.obj());
}

void ValidationMessageBubbleAndroid::SetPositionRelativeToAnchor(
    RenderWidgetHost* widget_host, const gfx::Rect& anchor_in_root_view) {
  Java_ValidationMessageBubble_setPositionRelativeToAnchor(
      base::android::AttachCurrentThread(),
      java_validation_message_bubble_.obj(),
      GetContentViewCoreFrom(widget_host)->GetJavaObject().obj(),
      anchor_in_root_view.x(),
      anchor_in_root_view.y(),
      anchor_in_root_view.width(),
      anchor_in_root_view.height());
}

// static
bool ValidationMessageBubbleAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace web_contents_delegate_android
