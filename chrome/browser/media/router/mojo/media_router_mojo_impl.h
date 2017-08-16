// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/router/issue_manager.h"
#include "chrome/browser/media/router/media_router_base.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/common/media_router/route_request_result.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}

namespace media_router {

enum class MediaRouteProviderWakeReason;
class CastMediaSinkService;
class DialMediaSinkServiceProxy;

// MediaRouter implementation that delegates calls to a MediaRouteProvider.
class MediaRouterMojoImpl : public MediaRouterBase,
                            public mojom::MediaRouter {
 public:
  ~MediaRouterMojoImpl() override;

  // MediaRouter implementation.
  // Execution of calls to MediaRouteProvider is delegated to the Do* methods,
  // which can be overridden in subclasses for additional logic such as request
  // queueing. Methods with Do* methods are marked final.
  void CreateRoute(const MediaSource::Id& source_id,
                   const MediaSink::Id& sink_id,
                   const url::Origin& origin,
                   content::WebContents* web_contents,
                   std::vector<MediaRouteResponseCallback> callbacks,
                   base::TimeDelta timeout,
                   bool incognito) final;
  void JoinRoute(const MediaSource::Id& source_id,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 content::WebContents* web_contents,
                 std::vector<MediaRouteResponseCallback> callbacks,
                 base::TimeDelta timeout,
                 bool incognito) final;
  void ConnectRouteByRouteId(const MediaSource::Id& source,
                             const MediaRoute::Id& route_id,
                             const url::Origin& origin,
                             content::WebContents* web_contents,
                             std::vector<MediaRouteResponseCallback> callbacks,
                             base::TimeDelta timeout,
                             bool incognito) final;
  void TerminateRoute(const MediaRoute::Id& route_id) final;
  void DetachRoute(const MediaRoute::Id& route_id) final;
  void SendRouteMessage(const MediaRoute::Id& route_id,
                        const std::string& message,
                        SendRouteMessageCallback callback) final;
  void SendRouteBinaryMessage(const MediaRoute::Id& route_id,
                              std::unique_ptr<std::vector<uint8_t>> data,
                              SendRouteMessageCallback callback) final;
  void AddIssue(const IssueInfo& issue_info) override;
  void ClearIssue(const Issue::Id& issue_id) override;
  void OnUserGesture() override;
  void SearchSinks(const MediaSink::Id& sink_id,
                   const MediaSource::Id& source_id,
                   const std::string& search_input,
                   const std::string& domain,
                   MediaSinkSearchResponseCallback sink_callback) final;
  void ProvideSinks(const std::string& provider_name,
                    std::vector<MediaSinkInternal> sinks) final;
  scoped_refptr<MediaRouteController> GetRouteController(
      const MediaRoute::Id& route_id) final;
  void RegisterMediaRouteProvider(
      mojom::MediaRouteProviderPtr media_route_provider_ptr,
      mojom::MediaRouter::RegisterMediaRouteProviderCallback callback) override;

  void set_instance_id_for_test(const std::string& instance_id) {
    instance_id_ = instance_id;
  }

 protected:
  // Standard constructor, used by
  // MediaRouterMojoImplFactory::GetApiForBrowserContext.
  explicit MediaRouterMojoImpl(content::BrowserContext* context);

  // These calls invoke methods in the MediaRouteProvider via Mojo.
  virtual void DoCreateRoute(const MediaSource::Id& source_id,
                             const MediaSink::Id& sink_id,
                             const url::Origin& origin,
                             int tab_id,
                             std::vector<MediaRouteResponseCallback> callbacks,
                             base::TimeDelta timeout,
                             bool incognito);
  virtual void DoJoinRoute(const MediaSource::Id& source_id,
                           const std::string& presentation_id,
                           const url::Origin& origin,
                           int tab_id,
                           std::vector<MediaRouteResponseCallback> callbacks,
                           base::TimeDelta timeout,
                           bool incognito);
  virtual void DoConnectRouteByRouteId(
      const MediaSource::Id& source_id,
      const MediaRoute::Id& route_id,
      const url::Origin& origin,
      int tab_id,
      std::vector<MediaRouteResponseCallback> callbacks,
      base::TimeDelta timeout,
      bool incognito);
  virtual void DoTerminateRoute(const MediaRoute::Id& route_id);
  virtual void DoDetachRoute(const MediaRoute::Id& route_id);
  virtual void DoSendRouteMessage(const MediaRoute::Id& route_id,
                                  const std::string& message,
                                  SendRouteMessageCallback callback);
  virtual void DoSendRouteBinaryMessage(
      const MediaRoute::Id& route_id,
      std::unique_ptr<std::vector<uint8_t>> data,
      SendRouteMessageCallback callback);
  virtual void DoStartListeningForRouteMessages(const MediaRoute::Id& route_id);
  virtual void DoStopListeningForRouteMessages(const MediaRoute::Id& route_id);
  virtual void DoStartObservingMediaSinks(const MediaSource::Id& source_id);
  virtual void DoStopObservingMediaSinks(const MediaSource::Id& source_id);
  virtual void DoStartObservingMediaRoutes(const MediaSource::Id& source_id);
  virtual void DoStopObservingMediaRoutes(const MediaSource::Id& source_id);
  virtual void DoSearchSinks(const MediaSink::Id& sink_id,
                             const MediaSource::Id& source_id,
                             const std::string& search_input,
                             const std::string& domain,
                             MediaSinkSearchResponseCallback sink_callback);
  virtual void DoCreateMediaRouteController(
      const MediaRoute::Id& route_id,
      mojom::MediaControllerRequest mojo_media_controller_request,
      mojom::MediaStatusObserverPtr mojo_observer);
  virtual void DoProvideSinks(const std::string& provider_name,
                              std::vector<MediaSinkInternal> sinks);
  virtual void DoUpdateMediaSinks(const MediaSource::Id& source_id);

  // Error handler callback for |media_route_provider_|.
  virtual void OnConnectionError();

  // Issues 0+ calls to |media_route_provider_| to ensure its state is in sync
  // with MediaRouter on a best-effort basis.
  virtual void SyncStateToMediaRouteProvider();

  // Requests MRPM to update media sinks.  This allows MRPs that only do
  // discovery on sink queries an opportunity to update discovery results
  // even if the MRP SinkAvailability is marked UNAVAILABLE.
  void UpdateMediaSinks(const MediaSource::Id& source_id);

  // Mojo proxy object for the Media Route Provider Manager.
  // Set to null initially, and later set to the Provider Manager proxy object
  // passed in via |RegisterMediaRouteProvider()|.
  // This is set to null again when the component extension is suspended
  // if or a Mojo channel error occured.
  mojom::MediaRouteProviderPtr media_route_provider_;

  // Stores route controllers that can be used to send media commands.
  std::unordered_map<MediaRoute::Id, MediaRouteController*> route_controllers_;

 private:
  friend class MediaRouterFactory;
  friend class MediaRouterMojoImplTest;
  friend class MediaRouterMojoTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, JoinRoute);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, JoinRouteTimedOutFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           JoinRouteIncognitoMismatchFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           IncognitoRoutesTerminatedOnProfileShutdown);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterAndUnregisterMediaSinksObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterMediaSinksObserverWithAvailabilityChange);
  FRIEND_TEST_ALL_PREFIXES(
      MediaRouterMojoImplTest,
      RegisterAndUnregisterMediaSinksObserverWithAvailabilityChange);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterAndUnregisterMediaRoutesObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RouteMessagesSingleObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RouteMessagesMultipleObservers);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, HandleIssue);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, GetRouteController);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           GetRouteControllerMultipleTimes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           GetRouteControllerAfterInvalidation);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           GetRouteControllerAfterRouteInvalidation);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           FailToCreateRouteController);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterMediaRoutesObserver_DedupingWithCache);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallback);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallbackRemoved);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           SyncStateToMediaRouteProvider);

  // Represents a query to the MRPM for media sinks and holds observers for the
  // query.
  struct MediaSinksQuery {
   public:
    MediaSinksQuery();
    ~MediaSinksQuery();

    // True if the query has been sent to the MRPM.
    bool is_active = false;

    // Cached list of sinks and origins for the query.
    base::Optional<std::vector<MediaSink>> cached_sink_list;
    std::vector<url::Origin> origins;

    base::ObserverList<MediaSinksObserver> observers;

   private:
    DISALLOW_COPY_AND_ASSIGN(MediaSinksQuery);
  };

  struct MediaRoutesQuery {
   public:
    MediaRoutesQuery();
    ~MediaRoutesQuery();

    // Cached list of routes and joinable route IDs for the query.
    base::Optional<std::vector<MediaRoute>> cached_route_list;
    std::vector<std::string> joinable_route_ids;

    base::ObserverList<MediaRoutesObserver> observers;

   private:
    DISALLOW_COPY_AND_ASSIGN(MediaRoutesQuery);
  };

  // MediaRouter implementation.
  bool RegisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void UnregisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void RegisterIssuesObserver(IssuesObserver* observer) override;
  void UnregisterIssuesObserver(IssuesObserver* observer) override;
  void RegisterRouteMessageObserver(RouteMessageObserver* observer) override;
  void UnregisterRouteMessageObserver(RouteMessageObserver* observer) override;
  void DetachRouteController(const MediaRoute::Id& route_id,
                             MediaRouteController* controller) override;

  // Notifies |observer| of any existing cached routes, if it is still
  // registered.
  void NotifyOfExistingRoutesIfRegistered(const MediaSource::Id& source_id,
                                          MediaRoutesObserver* observer) const;

  // mojom::MediaRouter implementation.
  void OnIssue(const IssueInfo& issue) override;
  void OnSinksReceived(const std::string& media_source,
                       const std::vector<MediaSinkInternal>& internal_sinks,
                       const std::vector<url::Origin>& origins) override;
  void OnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::string& media_source,
      const std::vector<std::string>& joinable_route_ids) override;
  void OnSinkAvailabilityUpdated(
      mojom::MediaRouter::SinkAvailability availability) override;
  void OnPresentationConnectionStateChanged(
      const std::string& route_id,
      content::PresentationConnectionState state) override;
  void OnPresentationConnectionClosed(
      const std::string& route_id,
      content::PresentationConnectionCloseReason reason,
      const std::string& message) override;
  void OnRouteMessagesReceived(
      const std::string& route_id,
      const std::vector<content::PresentationConnectionMessage>& messages)
      override;
  void OnMediaRemoterCreated(
      int32_t tab_id,
      media::mojom::MirrorServiceRemoterPtr remoter,
      media::mojom::MirrorServiceRemotingSourceRequest source_request) override;

  // Result callback when Mojo terminateRoute is invoked.  |route_id| is bound
  // to the ID of the route that was terminated.
  void OnTerminateRouteResult(const MediaRoute::Id& route_id,
                              const base::Optional<std::string>& error_text,
                              RouteRequestResult::ResultCode result_code);

  // Converts the callback result of calling Mojo CreateRoute()/JoinRoute()
  // into a local callback.
  void RouteResponseReceived(const std::string& presentation_id,
                             bool is_incognito,
                             std::vector<MediaRouteResponseCallback> callbacks,
                             bool is_join,
                             const base::Optional<MediaRoute>& media_route,
                             const base::Optional<std::string>& error_text,
                             RouteRequestResult::ResultCode result_code);

  // Start browser side sink discovery.
  void StartDiscovery();

  // Invalidates and removes controllers from |route_controllers_| whose media
  // routes do not appear in |routes|.
  void RemoveInvalidRouteControllers(const std::vector<MediaRoute>& routes);

  // Callback called by MRP's CreateMediaRouteController().
  void OnMediaControllerCreated(const MediaRoute::Id& route_id, bool success);

  std::unordered_map<MediaSource::Id, std::unique_ptr<MediaSinksQuery>>
      sinks_queries_;

  std::unordered_map<MediaSource::Id, std::unique_ptr<MediaRoutesQuery>>
      routes_queries_;

  std::unordered_map<MediaRoute::Id,
                     std::unique_ptr<base::ObserverList<RouteMessageObserver>>>
      message_observers_;

  IssueManager issue_manager_;

  // GUID unique to each browser run. Component extension uses this to detect
  // when its persisted state was written by an older browser instance, and is
  // therefore stale.
  std::string instance_id_;

  // The last reported sink availability from the media route provider manager.
  mojom::MediaRouter::SinkAvailability availability_;

  // Media sink service for DIAL devices.
  scoped_refptr<DialMediaSinkServiceProxy> dial_media_sink_service_proxy_;

  // Media sink service for CAST devices.
  scoped_refptr<CastMediaSinkService> cast_media_sink_service_;

  content::BrowserContext* const context_;

  base::WeakPtrFactory<MediaRouterMojoImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterMojoImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_
