// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/subresource_filter_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "jni/SubresourceFilterInfoBar_jni.h"

using base::android::JavaParamRef;

SubresourceFilterInfoBar::SubresourceFilterInfoBar(
    std::unique_ptr<SubresourceFilterInfobarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

SubresourceFilterInfoBar::~SubresourceFilterInfoBar() {}

base::android::ScopedJavaLocalRef<jobject>
SubresourceFilterInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ScopedJavaLocalRef;
  SubresourceFilterInfobarDelegate* subresource_filter_delegate =
      static_cast<SubresourceFilterInfobarDelegate*>(delegate());
  ScopedJavaLocalRef<jstring> reload_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ScopedJavaLocalRef<jstring> ok_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> message_text = ConvertUTF16ToJavaString(
      env, subresource_filter_delegate->GetMessageText());
  ScopedJavaLocalRef<jstring> explanation_message = ConvertUTF16ToJavaString(
      env, subresource_filter_delegate->GetExplanationText());

  return Java_SubresourceFilterInfoBar_show(
      env, GetEnumeratedIconId(), message_text, ok_button_text,
      reload_button_text, explanation_message);
}

std::unique_ptr<infobars::InfoBar> CreateSubresourceFilterInfoBar(
    std::unique_ptr<SubresourceFilterInfobarDelegate> delegate) {
  return base::MakeUnique<SubresourceFilterInfoBar>(std::move(delegate));
}
