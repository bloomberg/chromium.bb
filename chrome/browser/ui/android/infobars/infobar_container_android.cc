// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/infobar_container_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/auto_login_infobar_delegate_android.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "jni/InfoBarContainer_jni.h"


// InfoBarContainerAndroid ----------------------------------------------------

InfoBarContainerAndroid::InfoBarContainerAndroid(JNIEnv* env,
                                                 jobject obj,
                                                 jobject auto_login_delegate)
    : infobars::InfoBarContainer(NULL),
      weak_java_infobar_container_(env, obj),
      weak_java_auto_login_delegate_(env, auto_login_delegate) {}

InfoBarContainerAndroid::~InfoBarContainerAndroid() {
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void InfoBarContainerAndroid::PlatformSpecificAddInfoBar(
    infobars::InfoBar* infobar,
    size_t position) {
  DCHECK(infobar);
  InfoBarAndroid* android_bar = static_cast<InfoBarAndroid*>(infobar);
  if (!android_bar) {
    // TODO(bulach): CLANK: implement other types of InfoBars.
    // TODO(jrg): this will always print out WARNING_TYPE as an int.
    // Try and be more helpful.
    NOTIMPLEMENTED() << "CLANK: infobar type "
                     << infobar->delegate()->GetInfoBarType();
    return;
  }

  if (infobar->delegate()->AsAutoLoginInfoBarDelegate()) {
    AutoLoginInfoBarDelegateAndroid* auto_login_delegate =
        static_cast<AutoLoginInfoBarDelegateAndroid*>(
            infobar->delegate()->AsAutoLoginInfoBarDelegate());
    if (!auto_login_delegate->AttachAccount(weak_java_auto_login_delegate_))
      return;
  }

  AttachJavaInfoBar(android_bar);
}

void InfoBarContainerAndroid::AttachJavaInfoBar(InfoBarAndroid* android_bar) {
  if (android_bar->HasSetJavaInfoBar())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_infobar =
      android_bar->CreateRenderInfoBar(env);
  Java_InfoBarContainer_addInfoBar(
      env, weak_java_infobar_container_.get(env).obj(), java_infobar.obj());
  android_bar->set_java_infobar(java_infobar);
}

void InfoBarContainerAndroid::PlatformSpecificReplaceInfoBar(
    infobars::InfoBar* old_infobar,
    infobars::InfoBar* new_infobar) {
  static_cast<InfoBarAndroid*>(new_infobar)->PassJavaInfoBar(
      static_cast<InfoBarAndroid*>(old_infobar));
}

void InfoBarContainerAndroid::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
  InfoBarAndroid* android_infobar = static_cast<InfoBarAndroid*>(infobar);
  android_infobar->CloseJavaInfoBar();
}


// Native JNI methods ---------------------------------------------------------

static jlong Init(JNIEnv* env,
                  jobject obj,
                  jobject web_contents,
                  jobject auto_login_delegate) {
  InfoBarContainerAndroid* infobar_container =
      new InfoBarContainerAndroid(env, obj, auto_login_delegate);
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      content::WebContents::FromJavaWebContents(web_contents));
  infobar_container->ChangeInfoBarManager(infobar_service);
  return reinterpret_cast<intptr_t>(infobar_container);
}

bool RegisterInfoBarContainer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
