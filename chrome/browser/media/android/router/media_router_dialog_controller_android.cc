// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/media_router_dialog_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/media/router/media_source.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "jni/ChromeMediaRouterDialogController_jni.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    media_router::MediaRouterDialogControllerAndroid);

using content::WebContents;

namespace media_router {

// static
MediaRouterDialogControllerAndroid*
MediaRouterDialogControllerAndroid::GetOrCreateForWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);
  // This call does nothing if the controller already exists.
  MediaRouterDialogControllerAndroid::CreateForWebContents(web_contents);
  return MediaRouterDialogControllerAndroid::FromWebContents(web_contents);
}

MediaRouterDialogControllerAndroid::MediaRouterDialogControllerAndroid(
    WebContents* web_contents)
    : MediaRouterDialogController(web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_dialog_controller_.Reset(Java_ChromeMediaRouterDialogController_create(
      env,
      reinterpret_cast<jlong>(this),
      base::android::GetApplicationContext()));
}

// static
bool MediaRouterDialogControllerAndroid::Register(JNIEnv* env) {
  bool ret = RegisterNativesImpl(env);
  // No native calls to register yet.
  // DCHECK(g_ChromeMediaRouterDialogController_clazz);
  return ret;
}

MediaRouterDialogControllerAndroid::~MediaRouterDialogControllerAndroid() {
}

void MediaRouterDialogControllerAndroid::CreateMediaRouterDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();

  scoped_ptr<CreatePresentationSessionRequest> presentation_request(
      PassPresentationRequest());

  ScopedJavaLocalRef<jstring> jsource_urn =
      base::android::ConvertUTF8ToJavaString(
          env, presentation_request->GetMediaSource().id());

  Java_ChromeMediaRouterDialogController_createDialog(
      env, java_dialog_controller_.obj(), jsource_urn.obj());
}

void MediaRouterDialogControllerAndroid::CloseMediaRouterDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_ChromeMediaRouterDialogController_closeDialog(
      env, java_dialog_controller_.obj());
}

bool MediaRouterDialogControllerAndroid::IsShowingMediaRouterDialog() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ChromeMediaRouterDialogController_isShowingDialog(
      env, java_dialog_controller_.obj());
}

}  // namespace media_router

