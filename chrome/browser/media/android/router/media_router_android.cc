// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/media_router_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "jni/ChromeMediaRouter_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

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
  DCHECK(g_ChromeMediaRouter_clazz);
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
  const std::string& source_id = observer->source().id();
  base::ObserverList<MediaSinksObserver>* observer_list =
      sinks_observers_.get(source_id);
  if (!observer_list) {
    observer_list = new base::ObserverList<MediaSinksObserver>;
    sinks_observers_.add(source_id, make_scoped_ptr(observer_list));
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }

  observer_list->AddObserver(observer);
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource_id =
      base::android::ConvertUTF8ToJavaString(env, source_id);
  Java_ChromeMediaRouter_startObservingMediaSinks(
      env, java_media_router_.obj(), jsource_id.obj());
}

void MediaRouterAndroid::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  const std::string& source_id = observer->source().id();
  auto* observer_list = sinks_observers_.get(source_id);
  if (!observer_list || !observer_list->HasObserver(observer))
    return;

  // If we are removing the final observer for the source, then stop
  // observing sinks for it.
  // might_have_observers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  observer_list->RemoveObserver(observer);
  if (!observer_list->might_have_observers()) {
    sinks_observers_.erase(source_id);
    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaLocalRef<jstring> jsource_id =
        base::android::ConvertUTF8ToJavaString(env, source_id);
    Java_ChromeMediaRouter_stopObservingMediaSinks(
        env, java_media_router_.obj(), jsource_id.obj());
  }
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

void MediaRouterAndroid::OnSinksReceived(
    JNIEnv* env,
    jobject obj,
    jstring jsource_urn,
    jint count) {
  std::vector<MediaSink> sinks_converted;
  sinks_converted.reserve(count);
  for (int i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jstring> jsink_urn =
        Java_ChromeMediaRouter_getSinkUrn(
            env, java_media_router_.obj(), jsource_urn, i);
    ScopedJavaLocalRef<jstring> jsink_name =
        Java_ChromeMediaRouter_getSinkName(
            env, java_media_router_.obj(), jsource_urn, i);
    sinks_converted.push_back(
        MediaSink(ConvertJavaStringToUTF8(env, jsink_urn.obj()),
        ConvertJavaStringToUTF8(env, jsink_name.obj())));
  }

  std::string source_urn = ConvertJavaStringToUTF8(env, jsource_urn);
  auto it = sinks_observers_.find(source_urn);
  if (it != sinks_observers_.end()) {
    FOR_EACH_OBSERVER(MediaSinksObserver, *it->second,
                      OnSinksReceived(sinks_converted));
  }
}


}  // namespace media_router
