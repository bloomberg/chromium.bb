// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/infobar_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "jni/InfoBar_jni.h"


// InfoBarAndroid -------------------------------------------------------------

InfoBarAndroid::InfoBarAndroid(scoped_ptr<infobars::InfoBarDelegate> delegate)
    : infobars::InfoBar(delegate.Pass()) {
}

InfoBarAndroid::~InfoBarAndroid() {
}

void InfoBarAndroid::ReassignJavaInfoBar(InfoBarAndroid* replacement) {
  DCHECK(replacement);
  if (!java_info_bar_.is_null()) {
    replacement->set_java_infobar(java_info_bar_);
    java_info_bar_.Reset();
  }
}

void InfoBarAndroid::set_java_infobar(
    const base::android::JavaRef<jobject>& java_info_bar) {
  DCHECK(java_info_bar_.is_null());
  java_info_bar_.Reset(java_info_bar);
}

bool InfoBarAndroid::HasSetJavaInfoBar() const {
  return !java_info_bar_.is_null();
}

void InfoBarAndroid::OnButtonClicked(JNIEnv* env,
                                     jobject obj,
                                     jint action,
                                     jstring action_value) {
  std::string value = base::android::ConvertJavaStringToUTF8(env, action_value);
  ProcessButton(action, value);
}

void InfoBarAndroid::OnCloseButtonClicked(JNIEnv* env, jobject obj) {
  if (!owner())
    return; // We're closing; don't call anything, it might access the owner.
  delegate()->InfoBarDismissed();
  RemoveSelf();
}

void InfoBarAndroid::CloseJavaInfoBar() {
  if (!java_info_bar_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InfoBar_closeInfoBar(env, java_info_bar_.obj());
  }
}

int InfoBarAndroid::GetEnumeratedIconId() {
  return ResourceMapper::MapFromChromiumId(delegate()->GetIconID());
}


// Native JNI methods ---------------------------------------------------------

bool RegisterNativeInfoBar(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
