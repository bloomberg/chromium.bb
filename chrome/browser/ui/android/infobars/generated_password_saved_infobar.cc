// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/generated_password_saved_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/web_contents.h"
#include "jni/GeneratedPasswordSavedInfoBarDelegate_jni.h"

// static
void GeneratedPasswordSavedInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents) {
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(make_scoped_ptr(new GeneratedPasswordSavedInfoBar(
          make_scoped_ptr(new GeneratedPasswordSavedInfoBarDelegateAndroid))));
}

GeneratedPasswordSavedInfoBar::GeneratedPasswordSavedInfoBar(
    scoped_ptr<GeneratedPasswordSavedInfoBarDelegateAndroid> delegate)
    : InfoBarAndroid(delegate.Pass()) {
}

GeneratedPasswordSavedInfoBar::~GeneratedPasswordSavedInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
GeneratedPasswordSavedInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  GeneratedPasswordSavedInfoBarDelegateAndroid* infobar_delegate =
      static_cast<GeneratedPasswordSavedInfoBarDelegateAndroid*>(delegate());

  return Java_GeneratedPasswordSavedInfoBarDelegate_show(
      env, reinterpret_cast<intptr_t>(this), GetEnumeratedIconId(),
      base::android::ConvertUTF16ToJavaString(
          env, infobar_delegate->message_text()).obj(),
      infobar_delegate->inline_link_range().start(),
      infobar_delegate->inline_link_range().end(),
      base::android::ConvertUTF16ToJavaString(
          env, infobar_delegate->button_label()).obj());
}

void GeneratedPasswordSavedInfoBar::OnLinkClicked(JNIEnv* env, jobject obj) {
  if (!owner())
    return;

  static_cast<GeneratedPasswordSavedInfoBarDelegateAndroid*>(delegate())
      ->OnInlineLinkClicked();
  RemoveSelf();
}

void GeneratedPasswordSavedInfoBar::ProcessButton(
    int action,
    const std::string& action_value) {
  if (!owner())
    return;

  RemoveSelf();
}

bool RegisterGeneratedPasswordSavedInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
