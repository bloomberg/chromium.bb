// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/media_router_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "jni/ChromeMediaRouter_jni.h"

namespace media_router {

MediaRouterAndroid::MediaRouterAndroid(content::BrowserContext*) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_media_router_.Reset(Java_ChromeMediaRouter_create(
      env,
      reinterpret_cast<jlong>(this),
      base::android::GetApplicationContext()));
}

MediaRouterAndroid::~MediaRouterAndroid() {
}

// static
bool MediaRouterAndroid::Register(JNIEnv* env) {
  bool ret = RegisterNativesImpl(env);
  // No native calls to register just yet.
  // DCHECK(g_ChromeMediaRouter_clazz);
  return ret;
}

void MediaRouterAndroid::CreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const GURL& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::JoinRoute(
    const MediaSource::Id& source,
    const std::string& presentation_id,
    const GURL& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::CloseRoute(const MediaRoute::Id& route_id) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::SendRouteMessage(
    const MediaRoute::Id& route_id,
    const std::string& message,
    const SendRouteMessageCallback& callback) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    scoped_ptr<std::vector<uint8>> data,
    const SendRouteMessageCallback& callback) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::ClearIssue(const Issue::Id& issue_id) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::RegisterIssuesObserver(IssuesObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::UnregisterIssuesObserver(IssuesObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::RegisterPresentationSessionMessagesObserver(
    PresentationSessionMessagesObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::UnregisterPresentationSessionMessagesObserver(
    PresentationSessionMessagesObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace media_router
