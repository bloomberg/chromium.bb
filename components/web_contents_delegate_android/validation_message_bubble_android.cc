// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_contents_delegate_android/validation_message_bubble_android.h"

#include "base/android/jni_string.h"
#include "jni/ValidationMessageBubble_jni.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_conversions.h"

using base::android::ConvertUTF16ToJavaString;

namespace {

gfx::Rect ScaleToRoundedRect(const gfx::Rect& rect, float scale) {
  gfx::RectF scaledRect(rect);
  scaledRect.Scale(scale);
  return ToNearestRect(scaledRect);
}

gfx::Size ScaleToRoundedSize(const gfx::SizeF& size, float scale) {
  return gfx::ToRoundedSize(gfx::ScaleSize(size, scale));
}
}  // namespace

namespace web_contents_delegate_android {

ValidationMessageBubbleAndroid::ValidationMessageBubbleAndroid(
    gfx::NativeView view,
    const base::string16& main_text,
    const base::string16& sub_text) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_validation_message_bubble_.Reset(
      Java_ValidationMessageBubble_createIfApplicable(
          env, view->GetContainerView(),
          ConvertUTF16ToJavaString(env, main_text),
          ConvertUTF16ToJavaString(env, sub_text)));
}

ValidationMessageBubbleAndroid::~ValidationMessageBubbleAndroid() {
  if (!java_validation_message_bubble_.is_null()) {
    Java_ValidationMessageBubble_close(base::android::AttachCurrentThread(),
                                       java_validation_message_bubble_);
  }
}

void ValidationMessageBubbleAndroid::ShowAtPositionRelativeToAnchor(
    gfx::NativeView view,
    const gfx::Rect& anchor_in_screen) {
  if (java_validation_message_bubble_.is_null())
    return;

  // Convert to physical unit before passing to Java.
  float scale = view->GetDipScale() * view->page_scale();
  gfx::Rect anchor = ScaleToRoundedRect(anchor_in_screen, scale);
  gfx::Size viewport = ScaleToRoundedSize(view->viewport_size(), scale);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ValidationMessageBubble_showAtPositionRelativeToAnchor(
      env, java_validation_message_bubble_, viewport.width(), viewport.height(),
      view->content_offset() * scale, anchor.x(), anchor.y(), anchor.width(),
      anchor.height());
}

}  // namespace web_contents_delegate_android
