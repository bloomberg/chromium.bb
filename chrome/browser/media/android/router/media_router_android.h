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
  void ClearIssue(const Issue::Id& issue_id) override;

  // JNI functions.
  void OnSinksReceived(
      JNIEnv* env, jobject obj, jstring source_urn, jint count);
  void OnRouteCreated(
      JNIEnv* env,
      jobject obj,
      jstring media_route_id,
      jint jcreate_route_request_id,
      jboolean jis_local);
  void OnRouteCreationError(
      JNIEnv* env,
      jobject obj,
      jstring error_text,
      jint jcreate_route_request_id);

 private:
  friend class MediaRouterFactory;

  explicit MediaRouterAndroid(content::BrowserContext*);

  // MediaRouter implementation.
  void RegisterMediaSinksObserver(MediaSinksObserver* observer) override;
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

  DISALLOW_COPY_AND_ASSIGN(MediaRouterAndroid);
};

} // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_ANDROID_H_
