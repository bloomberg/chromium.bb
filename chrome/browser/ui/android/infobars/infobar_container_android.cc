// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/infobar_container_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/auto_login_infobar_delegate_android.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "content/public/browser/web_contents.h"
#include "jni/InfoBarContainer_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

// InfoBarContainerAndroid
InfoBarContainerAndroid::InfoBarContainerAndroid(JNIEnv* env,
                                                 jobject obj,
                                                 jobject auto_login_delegate)
    : InfoBarContainer(NULL),
      weak_java_infobar_container_(env, obj),
      weak_java_auto_login_delegate_(env, auto_login_delegate) {}

InfoBarContainerAndroid::~InfoBarContainerAndroid() {
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

// TODO(miguelg) Move this out of infobar container.
void InfoBarContainerAndroid::OnWebContentsReplaced(
    WebContents* old_web_contents,
    WebContents* new_web_contents) {

  InfoBarService* new_infobar_service = NULL;
  if (new_web_contents)
    new_infobar_service = InfoBarService::FromWebContents(new_web_contents);
  if (!new_infobar_service)
    return;

  ChangeInfoBarService(new_infobar_service);
}

// InfobarContainer
void InfoBarContainerAndroid::PlatformSpecificAddInfoBar(InfoBar* infobar,
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
    if (!auto_login_delegate->AttachAccount(weak_java_auto_login_delegate_)) {
      return;
    }
  }

  AttachJavaInfoBar(android_bar);
}

void InfoBarContainerAndroid::AttachJavaInfoBar(InfoBarAndroid* android_bar) {
  // Java infobar already set on the new bar, nothing to do.
  if (android_bar->HasSetJavaInfoBar())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_infobar =
      android_bar->CreateRenderInfoBar(env);
  Java_InfoBarContainer_addInfoBar(
      env, weak_java_infobar_container_.get(env).obj(), java_infobar.obj());
  android_bar->set_java_infobar(java_infobar);
}

void InfoBarContainerAndroid::PlatformSpecificReplaceInfoBar(
    InfoBar* old_infobar, InfoBar* new_infobar) {
  InfoBarAndroid* new_android_bar = static_cast<InfoBarAndroid*>(new_infobar);
  InfoBarAndroid* old_android_bar =
      old_infobar != NULL ? static_cast<InfoBarAndroid*>(old_infobar) : NULL;
  new_android_bar->PassJavaInfoBar(old_android_bar);
}

void InfoBarContainerAndroid::PlatformSpecificRemoveInfoBar(InfoBar* infobar) {
  InfoBarAndroid* android_infobar = static_cast<InfoBarAndroid*>(infobar);
  android_infobar->CloseJavaInfoBar();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, infobar);
}

// -----------------------------------------------------------------------------
// Native JNI methods
// -----------------------------------------------------------------------------
static int Init(JNIEnv* env,
                jobject obj,
                jint native_web_contents,
                jobject auto_login_delegate) {
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      reinterpret_cast<content::WebContents*>(native_web_contents));

  InfoBarContainerAndroid* infobar_container =
      new InfoBarContainerAndroid(env, obj, auto_login_delegate);
  infobar_container->ChangeInfoBarService(infobar_service);
  return reinterpret_cast<int>(infobar_container);
}

// Register native methods
bool RegisterInfoBarContainer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
