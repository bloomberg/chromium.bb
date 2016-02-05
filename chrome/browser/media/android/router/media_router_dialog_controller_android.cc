// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/media_router_dialog_controller_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/media/android/router/media_router_android.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/presentation_request.h"
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
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jsink_id) {
  scoped_ptr<CreatePresentationConnectionRequest> create_connection_request =
      TakeCreateConnectionRequest();
  const PresentationRequest& presentation_request =
      create_connection_request->presentation_request();
  const MediaSource::Id source_id = presentation_request.GetMediaSource().id();
  const GURL origin = presentation_request.frame_url().GetOrigin();

  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(
      base::Bind(&CreatePresentationConnectionRequest::HandleRouteResponse,
                 base::Passed(&create_connection_request)));

  MediaRouter* router = MediaRouterFactory::GetApiForBrowserContext(
      initiator()->GetBrowserContext());
  router->CreateRoute(source_id, ConvertJavaStringToUTF8(env, jsink_id), origin,
                      initiator(), route_response_callbacks, base::TimeDelta());
}

void MediaRouterDialogControllerAndroid::OnRouteClosed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jmedia_route_id) {
  std::string media_route_id = ConvertJavaStringToUTF8(env, jmedia_route_id);

  MediaRouter* router = MediaRouterFactory::GetApiForBrowserContext(
      initiator()->GetBrowserContext());

  router->TerminateRoute(media_route_id);

  CancelPresentationRequest();
}

void MediaRouterDialogControllerAndroid::OnDialogCancelled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CancelPresentationRequest();
}

void MediaRouterDialogControllerAndroid::CancelPresentationRequest() {
  scoped_ptr<CreatePresentationConnectionRequest> request =
      TakeCreateConnectionRequest();
  DCHECK(request);

  request->InvokeErrorCallback(content::PresentationError(
      content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
      "Dialog closed."));
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

  const MediaSource::Id source_id =
      create_connection_request()->presentation_request().GetMediaSource().id();
  ScopedJavaLocalRef<jstring> jsource_urn =
      base::android::ConvertUTF8ToJavaString(env, source_id);

  // If it's a single route with the same source, show the controller dialog
  // instead of the device picker.
  // TODO(avayvod): maybe this logic should be in
  // PresentationServiceDelegateImpl: if the route exists for the same frame
  // and tab, show the route controller dialog, if not, show the device picker.
  MediaRouterAndroid* router = static_cast<MediaRouterAndroid*>(
      MediaRouterFactory::GetApiForBrowserContext(
          initiator()->GetBrowserContext()));
  const MediaRoute* matching_route = router->FindRouteBySource(source_id);
  if (matching_route) {
    ScopedJavaLocalRef<jstring> jmedia_route_id =
        base::android::ConvertUTF8ToJavaString(
            env, matching_route->media_route_id());

    Java_ChromeMediaRouterDialogController_openRouteControllerDialog(
        env, java_dialog_controller_.obj(), jsource_urn.obj(),
        jmedia_route_id.obj());
    return;
  }

  Java_ChromeMediaRouterDialogController_openRouteChooserDialog(
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

