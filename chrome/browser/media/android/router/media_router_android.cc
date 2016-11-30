// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/media_router_android.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/route_message.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "content/public/browser/browser_context.h"
#include "jni/ChromeMediaRouter_jni.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

namespace media_router {

MediaRouterAndroid::MediaRouteRequest::MediaRouteRequest(
    const MediaSource& source,
    const std::string& presentation_id,
    const std::vector<MediaRouteResponseCallback>& callbacks)
    : media_source(source),
      presentation_id(presentation_id),
      callbacks(callbacks) {}

MediaRouterAndroid::MediaRouteRequest::~MediaRouteRequest() {}


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

const MediaRoute* MediaRouterAndroid::FindRouteBySource(
    const MediaSource::Id& source_id) const {
  for (const auto& route : active_routes_) {
    if (route.media_source().id() == source_id)
      return &route;
  }
  return nullptr;
}

void MediaRouterAndroid::CreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  // TODO(avayvod): Implement timeouts (crbug.com/583036).
  if (!origin.is_valid()) {
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Invalid origin", RouteRequestResult::INVALID_ORIGIN);
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(*result);
    return;
  }

  std::string presentation_id = MediaRouterBase::CreatePresentationId();

  int tab_id = -1;
  TabAndroid* tab = web_contents
      ? TabAndroid::FromWebContents(web_contents) : nullptr;
  if (tab)
    tab_id = tab->GetAndroidId();

  bool is_incognito = web_contents
      && web_contents->GetBrowserContext()->IsOffTheRecord();

  int route_request_id =
      route_requests_.Add(base::MakeUnique<MediaRouteRequest>(
          MediaSource(source_id), presentation_id, callbacks));

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource_id =
          base::android::ConvertUTF8ToJavaString(env, source_id);
  ScopedJavaLocalRef<jstring> jsink_id =
          base::android::ConvertUTF8ToJavaString(env, sink_id);
  ScopedJavaLocalRef<jstring> jpresentation_id =
          base::android::ConvertUTF8ToJavaString(env, presentation_id);
  ScopedJavaLocalRef<jstring> jorigin =
          base::android::ConvertUTF8ToJavaString(env, origin.spec());

  Java_ChromeMediaRouter_createRoute(env, java_media_router_, jsource_id,
                                     jsink_id, jpresentation_id, jorigin,
                                     tab_id, is_incognito, route_request_id);
}

void MediaRouterAndroid::ConnectRouteByRouteId(
    const MediaSource::Id& source,
    const MediaRoute::Id& route_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  NOTIMPLEMENTED();
}

void MediaRouterAndroid::JoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  // TODO(avayvod): Implement timeouts (crbug.com/583036).
  if (!origin.is_valid()) {
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Invalid origin", RouteRequestResult::INVALID_ORIGIN);
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(*result);
    return;
  }

  int tab_id = -1;
  TabAndroid* tab = web_contents
      ? TabAndroid::FromWebContents(web_contents) : nullptr;
  if (tab)
    tab_id = tab->GetAndroidId();

  DVLOG(2) << "JoinRoute: " << source_id << ", " << presentation_id << ", "
           << origin.spec() << ", " << tab_id;

  int request_id = route_requests_.Add(base::MakeUnique<MediaRouteRequest>(
      MediaSource(source_id), presentation_id, callbacks));

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource_id =
          base::android::ConvertUTF8ToJavaString(env, source_id);
  ScopedJavaLocalRef<jstring> jpresentation_id =
          base::android::ConvertUTF8ToJavaString(env, presentation_id);
  ScopedJavaLocalRef<jstring> jorigin =
          base::android::ConvertUTF8ToJavaString(env, origin.spec());

  Java_ChromeMediaRouter_joinRoute(env, java_media_router_, jsource_id,
                                   jpresentation_id, jorigin, tab_id,
                                   request_id);
}

void MediaRouterAndroid::TerminateRoute(const MediaRoute::Id& route_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jroute_id =
          base::android::ConvertUTF8ToJavaString(env, route_id);
  Java_ChromeMediaRouter_closeRoute(env, java_media_router_, jroute_id);
}

void MediaRouterAndroid::SendRouteMessage(
    const MediaRoute::Id& route_id,
    const std::string& message,
    const SendRouteMessageCallback& callback) {
  int callback_id = message_callbacks_.Add(
      base::MakeUnique<SendRouteMessageCallback>(callback));
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jroute_id =
          base::android::ConvertUTF8ToJavaString(env, route_id);
  ScopedJavaLocalRef<jstring> jmessage =
          base::android::ConvertUTF8ToJavaString(env, message);
  Java_ChromeMediaRouter_sendStringMessage(env, java_media_router_, jroute_id,
                                           jmessage, callback_id);
}

void MediaRouterAndroid::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    std::unique_ptr<std::vector<uint8_t>> data,
    const SendRouteMessageCallback& callback) {
  int callback_id = message_callbacks_.Add(
      base::MakeUnique<SendRouteMessageCallback>(callback));
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jroute_id =
      base::android::ConvertUTF8ToJavaString(env, route_id);
  ScopedJavaLocalRef<jbyteArray> jbyte_array =
      base::android::ToJavaByteArray(env, &((*data)[0]), data->size());
  Java_ChromeMediaRouter_sendBinaryMessage(env, java_media_router_, jroute_id,
                                           jbyte_array, callback_id);
}

void MediaRouterAndroid::AddIssue(const Issue& issue) {
  NOTIMPLEMENTED();
}

void MediaRouterAndroid::ClearIssue(const Issue::Id& issue_id) {
  NOTIMPLEMENTED();
}

void MediaRouterAndroid::OnUserGesture() {
}

void MediaRouterAndroid::SearchSinks(
    const MediaSink::Id& sink_id,
    const MediaSource::Id& source_id,
    const std::string& search_input,
    const std::string& domain,
    const MediaSinkSearchResponseCallback& sink_callback) {
  NOTIMPLEMENTED();
}

void MediaRouterAndroid::DetachRoute(const MediaRoute::Id& route_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jroute_id =
          base::android::ConvertUTF8ToJavaString(env, route_id);
  Java_ChromeMediaRouter_detachRoute(env, java_media_router_, jroute_id);
}

bool MediaRouterAndroid::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  const std::string& source_id = observer->source().id();
  base::ObserverList<MediaSinksObserver>* observer_list =
      sinks_observers_.get(source_id);
  if (!observer_list) {
    observer_list = new base::ObserverList<MediaSinksObserver>;
    sinks_observers_.add(source_id, base::WrapUnique(observer_list));
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }

  observer_list->AddObserver(observer);
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource_id =
      base::android::ConvertUTF8ToJavaString(env, source_id);
  return Java_ChromeMediaRouter_startObservingMediaSinks(
      env, java_media_router_, jsource_id);
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
    Java_ChromeMediaRouter_stopObservingMediaSinks(env, java_media_router_,
                                                   jsource_id);
  }
}

void MediaRouterAndroid::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  DVLOG(2) << "Added MediaRoutesObserver: " << observer;
  if (!observer->source_id().empty())
    NOTIMPLEMENTED() << "Joinable routes query not implemented.";

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

void MediaRouterAndroid::RegisterRouteMessageObserver(
    RouteMessageObserver* observer) {
  const MediaRoute::Id& route_id = observer->route_id();
  auto* observer_list = message_observers_.get(route_id);
  if (!observer_list) {
    observer_list = new base::ObserverList<RouteMessageObserver>;
    message_observers_.add(route_id, base::WrapUnique(observer_list));
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }

  observer_list->AddObserver(observer);
}

void MediaRouterAndroid::UnregisterRouteMessageObserver(
    RouteMessageObserver* observer) {
  const MediaRoute::Id& route_id = observer->route_id();
  auto* observer_list = message_observers_.get(route_id);
  DCHECK(observer_list->HasObserver(observer));

  observer_list->RemoveObserver(observer);
  if (!observer_list->might_have_observers())
    message_observers_.erase(route_id);
}

void MediaRouterAndroid::OnSinksReceived(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jsource_urn,
    jint jcount) {
  std::vector<MediaSink> sinks_converted;
  sinks_converted.reserve(jcount);
  for (int i = 0; i < jcount; ++i) {
    ScopedJavaLocalRef<jstring> jsink_urn = Java_ChromeMediaRouter_getSinkUrn(
        env, java_media_router_, jsource_urn, i);
    ScopedJavaLocalRef<jstring> jsink_name = Java_ChromeMediaRouter_getSinkName(
        env, java_media_router_, jsource_urn, i);
    sinks_converted.push_back(
        MediaSink(ConvertJavaStringToUTF8(env, jsink_urn.obj()),
        ConvertJavaStringToUTF8(env, jsink_name.obj()),
        MediaSink::GENERIC));
  }

  std::string source_urn = ConvertJavaStringToUTF8(env, jsource_urn);
  auto it = sinks_observers_.find(source_urn);
  if (it != sinks_observers_.end()) {
    // TODO(imcheng): Pass origins to OnSinksUpdated (crbug.com/594858).
    for (auto& observer : *it->second)
      observer.OnSinksUpdated(sinks_converted, std::vector<GURL>());
  }
}

void MediaRouterAndroid::OnRouteCreated(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jmedia_route_id,
    const JavaParamRef<jstring>& jsink_id,
    jint jroute_request_id,
    jboolean jis_local) {
  MediaRouteRequest* request = route_requests_.Lookup(jroute_request_id);
  if (!request)
    return;

  MediaRoute route(ConvertJavaStringToUTF8(env, jmedia_route_id),
                   request->media_source,
                   ConvertJavaStringToUTF8(env, jsink_id), std::string(),
                   jis_local, std::string(),
                   true);  // TODO(avayvod): Populate for_display.

  std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromSuccess(
      base::MakeUnique<MediaRoute>(route), request->presentation_id);
  for (const MediaRouteResponseCallback& callback : request->callbacks)
    callback.Run(*result);

  route_requests_.Remove(jroute_request_id);

  active_routes_.push_back(route);
  for (auto& observer : routes_observers_)
    observer.OnRoutesUpdated(active_routes_, std::vector<MediaRoute::Id>());
}

void MediaRouterAndroid::OnRouteRequestError(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jerror_text,
    jint jroute_request_id) {
  MediaRouteRequest* request = route_requests_.Lookup(jroute_request_id);
  if (!request)
    return;

  // TODO(imcheng): Provide a more specific result code.
  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromError(ConvertJavaStringToUTF8(env, jerror_text),
                                    RouteRequestResult::UNKNOWN_ERROR);
  for (const MediaRouteResponseCallback& callback : request->callbacks)
    callback.Run(*result);

  route_requests_.Remove(jroute_request_id);
}

void MediaRouterAndroid::OnRouteClosed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jmedia_route_id) {
  MediaRoute::Id route_id = ConvertJavaStringToUTF8(env, jmedia_route_id);
  for (auto it = active_routes_.begin(); it != active_routes_.end(); ++it)
    if (it->media_route_id() == route_id) {
      active_routes_.erase(it);
      break;
    }

  for (auto& observer : routes_observers_)
    observer.OnRoutesUpdated(active_routes_, std::vector<MediaRoute::Id>());
  NotifyPresentationConnectionStateChange(
      route_id, content::PRESENTATION_CONNECTION_STATE_TERMINATED);
}

void MediaRouterAndroid::OnRouteClosedWithError(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jmedia_route_id,
    const JavaParamRef<jstring>& jmessage) {
  MediaRoute::Id route_id = ConvertJavaStringToUTF8(env, jmedia_route_id);
  std::string message = ConvertJavaStringToUTF8(env, jmessage);
  NotifyPresentationConnectionClose(
      route_id,
      content::PRESENTATION_CONNECTION_CLOSE_REASON_CONNECTION_ERROR,
      message);

  OnRouteClosed(env, obj, jmedia_route_id);
}

void MediaRouterAndroid::OnMessageSentResult(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jboolean jsuccess,
                                             jint jcallback_id) {
  SendRouteMessageCallback* callback = message_callbacks_.Lookup(jcallback_id);
  callback->Run(jsuccess);
  message_callbacks_.Remove(jcallback_id);
}

// Notifies the media router about a message received from the media route.
void MediaRouterAndroid::OnMessage(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   const JavaParamRef<jstring>& jmedia_route_id,
                                   const JavaParamRef<jstring>& jmessage) {
  MediaRoute::Id route_id = ConvertJavaStringToUTF8(env, jmedia_route_id);
  auto* observer_list = message_observers_.get(route_id);
  if (!observer_list)
    return;

  std::vector<RouteMessage> messages(1);
  messages.front().type = RouteMessage::TEXT;
  messages.front().text = ConvertJavaStringToUTF8(env, jmessage);
  for (auto& observer : *observer_list)
    observer.OnMessagesReceived(messages);
}

}  // namespace media_router
