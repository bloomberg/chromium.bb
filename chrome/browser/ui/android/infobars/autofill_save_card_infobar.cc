// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/autofill_save_card_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_mobile.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "jni/AutofillSaveCardInfoBar_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using base::android::ScopedJavaLocalRef;

namespace autofill {

std::unique_ptr<infobars::InfoBar> CreateSaveCardInfoBarMobile(
    std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate) {
  return base::MakeUnique<AutofillSaveCardInfoBar>(std::move(delegate));
}

}  // namespace autofill

AutofillSaveCardInfoBar::AutofillSaveCardInfoBar(
    std::unique_ptr<autofill::AutofillSaveCardInfoBarDelegateMobile> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

AutofillSaveCardInfoBar::~AutofillSaveCardInfoBar() {}

void AutofillSaveCardInfoBar::OnLegalMessageLinkClicked(JNIEnv* env,
                                                        jobject obj,
                                                        jstring url) {
  GetSaveCardDelegate()->OnLegalMessageLinkClicked(
      GURL(base::android::ConvertJavaStringToUTF16(env, url)));
}

base::android::ScopedJavaLocalRef<jobject>
AutofillSaveCardInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  autofill::AutofillSaveCardInfoBarDelegateMobile* delegate =
      GetSaveCardDelegate();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (delegate->GetIconId() == infobars::InfoBarDelegate::kNoIconID &&
      !delegate->GetIcon().IsEmpty()) {
    java_bitmap = gfx::ConvertToJavaBitmap(delegate->GetIcon().ToSkBitmap());
  }

  base::android::ScopedJavaLocalRef<jobject> java_delegate =
      Java_AutofillSaveCardInfoBar_create(
          env, reinterpret_cast<intptr_t>(this), GetEnumeratedIconId(),
          java_bitmap, base::android::ConvertUTF16ToJavaString(
                           env, delegate->GetMessageText()),
          base::android::ConvertUTF16ToJavaString(env, delegate->GetLinkText()),
          base::android::ConvertUTF16ToJavaString(
              env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK)),
          base::android::ConvertUTF16ToJavaString(
              env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL)));

  Java_AutofillSaveCardInfoBar_addDetail(
      env, java_delegate,
      ResourceMapper::MapFromChromiumId(delegate->issuer_icon_id()),
      base::android::ConvertUTF16ToJavaString(env, delegate->card_label()),
      base::android::ConvertUTF16ToJavaString(env, delegate->card_sub_label()));

  for (const auto& line : delegate->legal_messages()) {
    Java_AutofillSaveCardInfoBar_addLegalMessageLine(
        env, java_delegate,
        base::android::ConvertUTF16ToJavaString(env, line.text()));
    for (const auto& link : line.links()) {
      Java_AutofillSaveCardInfoBar_addLinkToLastLegalMessageLine(
          env, java_delegate, link.range.start(), link.range.end(),
          base::android::ConvertUTF8ToJavaString(env, link.url.spec()));
    }
  }

  return java_delegate;
}

autofill::AutofillSaveCardInfoBarDelegateMobile*
AutofillSaveCardInfoBar::GetSaveCardDelegate() {
  return static_cast<autofill::AutofillSaveCardInfoBarDelegateMobile*>(
      GetDelegate());
}
