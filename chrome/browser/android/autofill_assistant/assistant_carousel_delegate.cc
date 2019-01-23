// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/assistant_carousel_delegate.h"

#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"
#include "jni/AssistantCarouselDelegate_jni.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

AssistantCarouselDelegate::AssistantCarouselDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_carousel_delegate_ = Java_AssistantCarouselDelegate_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantCarouselDelegate::~AssistantCarouselDelegate() {
  Java_AssistantCarouselDelegate_clearNativePtr(
      AttachCurrentThread(), java_assistant_carousel_delegate_);
}

void AssistantCarouselDelegate::OnChipSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint index) {
  ui_controller_->OnChipSelected(index);
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantCarouselDelegate::GetJavaObject() {
  return java_assistant_carousel_delegate_;
}

}  // namespace autofill_assistant
