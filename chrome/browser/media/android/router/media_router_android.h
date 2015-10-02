// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_ANDROID_H_
#define CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/media/router/media_router.h"

namespace content {
class BrowserContext;
}

namespace media_router {

// A stub implementation of MediaRouter interface on Android.
class MediaRouterAndroid : public MediaRouter {
 public:
  ~MediaRouterAndroid() override;

  static bool Register(JNIEnv* env);

  // MediaRouter implementation.
  void CreateRoute(
      const MediaSource::Id& source_id,
      const MediaSink::Id& sink_id,
      const GURL& origin,
      int tab_id,
      const std::vector<MediaRouteResponseCallback>& callbacks) override;
  void JoinRoute(
      const MediaSource::Id& source,
      const std::string& presentation_id,
      const GURL& origin,
      int tab_id,
      const std::vector<MediaRouteResponseCallback>& callbacks) override;
  void CloseRoute(const MediaRoute::Id& route_id) override;
  void SendRouteMessage(const MediaRoute::Id& route_id,
                        const std::string& message,
                        const SendRouteMessageCallback& callback) override;
  void SendRouteBinaryMessage(
      const MediaRoute::Id& route_id,
      scoped_ptr<std::vector<uint8>> data,
      const SendRouteMessageCallback& callback) override;
  void AddIssue(const Issue& issue) override;
  void ClearIssue(const Issue::Id& issue_id) override;
  void OnPresentationSessionDetached(const MediaRoute::Id& route_id) override;

  // The methods called by the Java counterpart.

  // Notifies the media router that information about sinks is received for
  // a specific source URN.
  void OnSinksReceived(
      JNIEnv* env, jobject obj, jstring jsource_urn, jint jcount);

  // Notifies the media router about a successful route creation.
  void OnRouteCreated(
      JNIEnv* env,
      jobject obj,
      jstring jmedia_route_id,
      jint jcreate_route_request_id,
      jboolean jis_local);

  // Notifies the media router that route creation failed.
  void OnRouteCreationError(
      JNIEnv* env,
      jobject obj,
      jstring jerror_text,
      jint jcreate_route_request_id);

  // Notifies the media router when the route was closed.
  void OnRouteClosed(JNIEnv* env, jobject obj, jstring jmedia_route_id);

  // Notifies the media router about the result of sending a message.
  void OnMessageSentResult(
      JNIEnv* env, jobject obj, jboolean jsuccess, jint jcallback_id);

  // Notifies the media router about a message received from the media route.
  void OnMessage(
      JNIEnv* env, jobject obj, jstring jmedia_route_id, jstring jmessage);

 private:
  friend class MediaRouterFactory;

  explicit MediaRouterAndroid(content::BrowserContext*);

  // MediaRouter implementation.
  bool RegisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void UnregisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void RegisterIssuesObserver(IssuesObserver* observer) override;
  void UnregisterIssuesObserver(IssuesObserver* observer) override;
  void RegisterPresentationSessionMessagesObserver(
      PresentationSessionMessagesObserver* observer) override;
  void UnregisterPresentationSessionMessagesObserver(
      PresentationSessionMessagesObserver* observer) override;

  base::android::ScopedJavaGlobalRef<jobject> java_media_router_;

  using MediaSinkObservers = base::ScopedPtrHashMap<
      MediaSource::Id,
      scoped_ptr<base::ObserverList<MediaSinksObserver>>>;
  MediaSinkObservers sinks_observers_;

  base::ObserverList<MediaRoutesObserver> routes_observers_;

  struct CreateMediaRouteRequest {
    CreateMediaRouteRequest(
        const MediaSource& source,
        const MediaSink& sink,
        const std::string& presentation_id,
        const std::vector<MediaRouteResponseCallback>& callbacks);
    ~CreateMediaRouteRequest();

    MediaSource media_source;
    MediaSink media_sink;
    std::string presentation_id;
    std::vector<MediaRouteResponseCallback> callbacks;
  };

  using CreateMediaRouteRequests =
      IDMap<CreateMediaRouteRequest, IDMapOwnPointer>;
  CreateMediaRouteRequests create_route_requests_;

  using MediaRoutes = std::vector<MediaRoute>;
  MediaRoutes active_routes_;

  using SendMessageCallbacks = IDMap<SendRouteMessageCallback, IDMapOwnPointer>;
  SendMessageCallbacks message_callbacks_;

  using MessagesObservers = base::ScopedPtrHashMap<
      MediaRoute::Id,
      scoped_ptr<base::ObserverList<PresentationSessionMessagesObserver>>>;
  MessagesObservers messages_observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterAndroid);
};

} // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_ANDROID_H_
