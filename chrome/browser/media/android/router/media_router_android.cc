// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/media_router_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/guid.h"
#include "base/logging.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "jni/ChromeMediaRouter_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

namespace media_router {

MediaRouterAndroid::CreateMediaRouteRequest::CreateMediaRouteRequest(
    const MediaSource& source,
    const MediaSink& sink,
    const std::string& presentation_id,
    const std::vector<MediaRouteResponseCallback>& callbacks)
    : media_source(source),
      media_sink(sink),
      presentation_id(presentation_id),
      callbacks(callbacks) {}
MediaRouterAndroid::CreateMediaRouteRequest::~CreateMediaRouteRequest() {}

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
  if (!origin.is_valid()) {
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(nullptr, "", "Invalid origin");
    return;
  }

  // TODO(avayvod): unify presentation id generation code between platforms.
  // https://crbug.com/522239
  std::string presentation_id("mr_");
  presentation_id += base::GenerateGUID();

  CreateMediaRouteRequest* request = new CreateMediaRouteRequest(
      MediaSource(source_id),
      MediaSink(sink_id, std::string()),
      presentation_id,
      callbacks);
  int create_route_request_id = create_route_requests_.Add(request);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource_id =
          base::android::ConvertUTF8ToJavaString(env, source_id);
  ScopedJavaLocalRef<jstring> jsink_id =
          base::android::ConvertUTF8ToJavaString(env, sink_id);
  ScopedJavaLocalRef<jstring> jpresentation_id =
          base::android::ConvertUTF8ToJavaString(env, presentation_id);

  Java_ChromeMediaRouter_createRoute(
      env,
      java_media_router_.obj(),
      jsource_id.obj(),
      jsink_id.obj(),
      jpresentation_id.obj(),
      create_route_request_id);
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
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jroute_id =
          base::android::ConvertUTF8ToJavaString(env, route_id);
  Java_ChromeMediaRouter_closeRoute(
      env, java_media_router_.obj(), jroute_id.obj());
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
  DVLOG(2) << "Added MediaRoutesObserver: " << observer;
  routes_observers_.AddObserver(observer);
}

void MediaRouterAndroid::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  if (!routes_observers_.HasObserver(observer))
    return;
  routes_observers_.RemoveObserver(observer);
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
    jint jcount) {
  std::vector<MediaSink> sinks_converted;
  sinks_converted.reserve(jcount);
  for (int i = 0; i < jcount; ++i) {
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

void MediaRouterAndroid::OnRouteCreated(
    JNIEnv* env,
    jobject obj,
    jstring jmedia_route_id,
    jint jcreate_route_request_id,
    jboolean jis_local) {
  CreateMediaRouteRequest* request =
      create_route_requests_.Lookup(jcreate_route_request_id);
  if (!request)
    return;

  MediaRoute route(
      ConvertJavaStringToUTF8(env, jmedia_route_id),
      request->media_source,
      request->media_sink,
      std::string(),
      jis_local,
      std::string());

  for (const MediaRouteResponseCallback& callback : request->callbacks)
    callback.Run(&route, request->presentation_id, std::string());

  create_route_requests_.Remove(jcreate_route_request_id);

  active_routes_.push_back(route);
  FOR_EACH_OBSERVER(MediaRoutesObserver, routes_observers_,
                    OnRoutesUpdated(active_routes_));
}

void MediaRouterAndroid::OnRouteCreationError(
    JNIEnv* env,
    jobject obj,
    jstring jerror_text,
    jint jcreate_route_request_id) {
  CreateMediaRouteRequest* request =
      create_route_requests_.Lookup(jcreate_route_request_id);
  if (!request)
    return;

  std::string error_text = ConvertJavaStringToUTF8(env, jerror_text);

  for (const MediaRouteResponseCallback& callback : request->callbacks)
    callback.Run(nullptr, std::string(), error_text);

  create_route_requests_.Remove(jcreate_route_request_id);
}

void MediaRouterAndroid::OnRouteClosed(JNIEnv* env,
                                       jobject obj,
                                       jstring jmedia_route_id) {
  MediaRoute::Id route_id = ConvertJavaStringToUTF8(env, jmedia_route_id);
  for (auto it = active_routes_.begin(); it != active_routes_.end(); ++it)
    if (it->media_route_id() == route_id) {
      active_routes_.erase(it);
      break;
    }

  FOR_EACH_OBSERVER(MediaRoutesObserver, routes_observers_,
                    OnRoutesUpdated(active_routes_));
}

}  // namespace media_router
