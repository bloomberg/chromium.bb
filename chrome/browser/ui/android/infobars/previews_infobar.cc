// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/previews_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "jni/PreviewsInfoBar_jni.h"

PreviewsInfoBar::PreviewsInfoBar(
    std::unique_ptr<PreviewsInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

PreviewsInfoBar::~PreviewsInfoBar() {}

base::android::ScopedJavaLocalRef<jobject> PreviewsInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
  PreviewsInfoBarDelegate* delegate =
      static_cast<PreviewsInfoBarDelegate*>(GetDelegate());
  base::android::ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetMessageText());
  base::android::ScopedJavaLocalRef<jstring> link_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetLinkText());
  base::android::ScopedJavaLocalRef<jstring> timestamp_text =
      base::android::ConvertUTF16ToJavaString(env,
                                              delegate->GetTimestampText());
  return Java_PreviewsInfoBar_show(env, GetEnumeratedIconId(), message_text,
                                   link_text, timestamp_text);
}

// static
std::unique_ptr<infobars::InfoBar> PreviewsInfoBar::CreateInfoBar(
    infobars::InfoBarManager* infobar_manager,
    std::unique_ptr<PreviewsInfoBarDelegate> delegate) {
  return base::MakeUnique<PreviewsInfoBar>(std::move(delegate));
}
