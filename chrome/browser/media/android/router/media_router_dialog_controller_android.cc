// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/media_router_dialog_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/media/android/router/media_router_android.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "jni/ChromeMediaRouterDialogController_jni.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    media_router::MediaRouterDialogControllerAndroid);

using base::android::ConvertJavaStringToUTF8;
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

void MediaRouterDialogControllerAndroid::OnSinkSelected(
    JNIEnv* env, jobject obj, jstring jsink_id) {
  scoped_ptr<CreatePresentationSessionRequest>
      request(TakePresentationRequest());

  const std::string& source_id = request->media_source().id();
  const GURL& origin = request->frame_url().GetOrigin();

  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(
      base::Bind(&CreatePresentationSessionRequest::HandleRouteResponse,
                 base::Passed(&request)));

  MediaRouter* router = MediaRouterFactory::GetApiForBrowserContext(
      initiator()->GetBrowserContext());
  router->CreateRoute(source_id, ConvertJavaStringToUTF8(env, jsink_id), origin,
                      SessionTabHelper::IdForTab(initiator()),
                      route_response_callbacks);
}

void MediaRouterDialogControllerAndroid::OnDialogDismissed(
    JNIEnv* env, jobject obj) {
  scoped_ptr<CreatePresentationSessionRequest> request(
      TakePresentationRequest());

  // If OnDialogDismissed is called after OnSinkSelected, do nothing.
  if (!request)
    return;

  request->MaybeInvokeErrorCallback(content::PresentationError(
      content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
      "The device picker dialog has been dismissed"));
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
  return RegisterNativesImpl(env);
}

MediaRouterDialogControllerAndroid::~MediaRouterDialogControllerAndroid() {
}

void MediaRouterDialogControllerAndroid::CreateMediaRouterDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> jsource_urn =
      base::android::ConvertUTF8ToJavaString(
          env, presentation_request()->media_source().id());

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

