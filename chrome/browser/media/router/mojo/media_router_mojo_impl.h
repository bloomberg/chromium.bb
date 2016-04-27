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
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/issue_manager.h"
#include "chrome/browser/media/router/media_router_base.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/mojo/media_router.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class EventPageTracker;
class Extension;
}

namespace media_router {

enum class MediaRouteProviderWakeReason;

// MediaRouter implementation that delegates calls to the component extension.
// Also handles the suspension and wakeup of the component extension.
class MediaRouterMojoImpl : public MediaRouterBase,
                            public interfaces::MediaRouter {
 public:
  ~MediaRouterMojoImpl() override;

  // Sets up the MediaRouterMojoImpl instance owned by |context| to handle
  // MediaRouterObserver requests from the component extension given by
  // |extension|. Creates the MediaRouterMojoImpl instance if it does not
  // exist.
  // Called by the Mojo module registry.
  // |extension|: The component extension, used for querying
  //     suspension state.
  // |context|: The BrowserContext which owns the extension process.
  // |request|: The Mojo connection request used for binding.
  static void BindToRequest(
      const extensions::Extension* extension,
      content::BrowserContext* context,
      mojo::InterfaceRequest<interfaces::MediaRouter> request);

  // MediaRouter implementation.
  // Execution of the requests is delegated to the Do* methods, which can be
  // enqueued for later use if the extension is temporarily suspended.
  void CreateRoute(const MediaSource::Id& source_id,
                   const MediaSink::Id& sink_id,
                   const GURL& origin,
                   content::WebContents* web_contents,
                   const std::vector<MediaRouteResponseCallback>& callbacks,
                   base::TimeDelta timeout,
                   bool off_the_record) override;
  void JoinRoute(const MediaSource::Id& source_id,
                 const std::string& presentation_id,
                 const GURL& origin,
                 content::WebContents* web_contents,
                 const std::vector<MediaRouteResponseCallback>& callbacks,
                 base::TimeDelta timeout,
                 bool off_the_record) override;
  void ConnectRouteByRouteId(
      const MediaSource::Id& source,
      const MediaRoute::Id& route_id,
      const GURL& origin,
      content::WebContents* web_contents,
      const std::vector<MediaRouteResponseCallback>& callbacks,
      base::TimeDelta timeout,
      bool off_the_record) override;
  void TerminateRoute(const MediaRoute::Id& route_id) override;
  void DetachRoute(const MediaRoute::Id& route_id) override;
  void SendRouteMessage(const MediaRoute::Id& route_id,
                        const std::string& message,
                        const SendRouteMessageCallback& callback) override;
  void SendRouteBinaryMessage(
      const MediaRoute::Id& route_id,
      std::unique_ptr<std::vector<uint8_t>> data,
      const SendRouteMessageCallback& callback) override;
  void AddIssue(const Issue& issue) override;
  void ClearIssue(const Issue::Id& issue_id) override;
  void OnUserGesture() override;
  void SearchSinksAndCreateRoute(
      const MediaSink::Id& sink_id,
      const MediaSource::Id& source_id,
      const std::string& search_input,
      const std::string& domain,
      const GURL& origin,
      content::WebContents* web_contents,
      const std::vector<MediaRouteResponseCallback>& route_callbacks,
      const MediaSinkSearchResponseCallback& sink_callback,
      base::TimeDelta timeout,
      bool off_the_record) override;

  const std::string& media_route_provider_extension_id() const {
    return media_route_provider_extension_id_;
  }

  void set_instance_id_for_test(const std::string& instance_id) {
    instance_id_ = instance_id;
  }

 private:
  friend class MediaRouterFactory;
  friend class MediaRouterMojoExtensionTest;
  friend class MediaRouterMojoTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, JoinRoute);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, JoinRouteTimedOutFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           JoinRouteOffTheRecordMismatchFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           OffTheRecordRoutesTerminatedOnProfileShutdown);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterAndUnregisterMediaSinksObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterMediaSinksObserverWithAvailabilityChange);
  FRIEND_TEST_ALL_PREFIXES(
      MediaRouterMojoImplTest,
      RegisterAndUnregisterMediaSinksObserverWithAvailabilityChange);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterAndUnregisterMediaRoutesObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, HandleIssue);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoExtensionTest,
                           DeferredBindingAndSuspension);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoExtensionTest,
                           DrainPendingRequestQueue);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoExtensionTest,
                           DropOldestPendingRequest);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoExtensionTest,
                           AttemptedWakeupTooManyTimes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoExtensionTest,
                           WakeupFailedDrainsQueue);

  // The max number of pending requests allowed. When number of pending requests
  // exceeds this number, the oldest request will be dropped.
  static const int kMaxPendingRequests = 30;

  // Max consecutive attempts to wake up the component extension before
  // giving up and draining the pending request queue.
  static const int kMaxWakeupAttemptCount = 3;

  // Represents a query to the MRPM for media sinks and holds observers for the
  // query.
  struct MediaSinksQuery {
   public:
    MediaSinksQuery();
    ~MediaSinksQuery();

    // True if the query has been sent to the MRPM.
    bool is_active = false;

    // True if cached result is available.
    bool has_cached_result = false;

    // Cached list of sinks for the query, if |has_cached_result| is true.
    // Empty otherwise.
    std::vector<MediaSink> cached_sink_list;
    std::vector<GURL> origins;
    base::ObserverList<MediaSinksObserver> observers;

   private:
    DISALLOW_COPY_AND_ASSIGN(MediaSinksQuery);
  };

  struct MediaRoutesQuery {
   public:
    MediaRoutesQuery();
    ~MediaRoutesQuery();

    // True if the query has been sent to the MRPM.  False otherwise.
    bool is_active = false;
    base::ObserverList<MediaRoutesObserver> observers;

   private:
    DISALLOW_COPY_AND_ASSIGN(MediaRoutesQuery);
  };

  // Standard constructor, used by
  // MediaRouterMojoImplFactory::GetApiForBrowserContext.
  explicit MediaRouterMojoImpl(
      extensions::EventPageTracker* event_page_tracker);

  // Binds |this| to a Mojo interface request, so that clients can acquire a
  // handle to a MediaRouterMojoImpl instance via the Mojo service connector.
  // Stores the ID of |extension| in |media_route_provider_extension_id_|.
  void BindToMojoRequest(
      mojo::InterfaceRequest<interfaces::MediaRouter> request,
      const extensions::Extension& extension);

  // Enqueues a closure for later execution by ExecutePendingRequests().
  void EnqueueTask(const base::Closure& closure);

  // Runs a closure if the extension monitored by |extension_monitor_| is
  // active, or defers it for later execution if the extension is suspended.
  void RunOrDefer(const base::Closure& request);

  // Dispatches the Mojo requests queued in |pending_requests_|.
  void ExecutePendingRequests();

  // Drops all pending requests. Called when we have a connection error to
  // component extension and further reattempts are unlikely to help.
  void DrainPendingRequests();

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

  // These calls invoke methods in the component extension via Mojo.
  void DoCreateRoute(const MediaSource::Id& source_id,
                     const MediaSink::Id& sink_id,
                     const std::string& origin,
                     int tab_id,
                     const std::vector<MediaRouteResponseCallback>& callbacks,
                     base::TimeDelta timeout,
                     bool off_the_record);
  void DoJoinRoute(const MediaSource::Id& source_id,
                   const std::string& presentation_id,
                   const std::string& origin,
                   int tab_id,
                   const std::vector<MediaRouteResponseCallback>& callbacks,
                   base::TimeDelta timeout,
                   bool off_the_record);
  void DoConnectRouteByRouteId(
      const MediaSource::Id& source_id,
      const MediaRoute::Id& route_id,
      const std::string& origin,
      int tab_id,
      const std::vector<MediaRouteResponseCallback>& callbacks,
      base::TimeDelta timeout,
      bool off_the_record);
  void DoTerminateRoute(const MediaRoute::Id& route_id);
  void DoDetachRoute(const MediaRoute::Id& route_id);
  void DoSendSessionMessage(const MediaRoute::Id& route_id,
                            const std::string& message,
                            const SendRouteMessageCallback& callback);
  void DoSendSessionBinaryMessage(const MediaRoute::Id& route_id,
                                  std::unique_ptr<std::vector<uint8_t>> data,
                                  const SendRouteMessageCallback& callback);
  void DoListenForRouteMessages(const MediaRoute::Id& route_id);
  void DoStopListeningForRouteMessages(const MediaRoute::Id& route_id);
  void DoStartObservingMediaSinks(const MediaSource::Id& source_id);
  void DoStopObservingMediaSinks(const MediaSource::Id& source_id);
  void DoStartObservingMediaRoutes(const MediaSource::Id& source_id);
  void DoStopObservingMediaRoutes(const MediaSource::Id& source_id);
  void DoSearchSinksAndCreateRoute(
      const MediaSink::Id& sink_id,
      const MediaSource::Id& source_id,
      const std::string& search_input,
      const std::string& domain,
      const std::string& origin,
      int tab_id,
      const std::vector<MediaRouteResponseCallback>& route_callbacks,
      const MediaSinkSearchResponseCallback& sink_callback,
      base::TimeDelta timeout,
      bool off_the_record);

  // Invoked when the next batch of messages arrives.
  // |route_id|: ID of route of the messages.
  // |messages|: A list of messages received.
  // |error|: true if an error occurred.
  void OnRouteMessagesReceived(
      const MediaRoute::Id& route_id,
      mojo::Array<interfaces::RouteMessagePtr> messages,
      bool error);

  // Error handler callback for |binding_| and |media_route_provider_|.
  void OnConnectionError();

  // interfaces::MediaRouter implementation.
  void RegisterMediaRouteProvider(
      interfaces::MediaRouteProviderPtr media_route_provider_ptr,
      const interfaces::MediaRouter::RegisterMediaRouteProviderCallback&
          callback) override;
  void OnIssue(interfaces::IssuePtr issue) override;
  void OnSinksReceived(const mojo::String& media_source,
                       mojo::Array<interfaces::MediaSinkPtr> sinks,
                       mojo::Array<mojo::String> origins) override;
  void OnSearchSinkIdReceived(const mojo::String& pseudo_sink_id,
                              const mojo::String& sink_id) override;
  void OnRoutesUpdated(mojo::Array<interfaces::MediaRoutePtr> routes,
                       const mojo::String& media_source,
                       mojo::Array<mojo::String> joinable_route_ids) override;
  void OnSinkAvailabilityUpdated(
      interfaces::MediaRouter::SinkAvailability availability) override;
  void OnPresentationConnectionStateChanged(
      const mojo::String& route_id,
      interfaces::MediaRouter::PresentationConnectionState state) override;
  void OnPresentationConnectionClosed(
      const mojo::String& route_id,
      interfaces::MediaRouter::PresentationConnectionCloseReason reason,
      const mojo::String& message) override;

  // Converts the callback result of calling Mojo CreateRoute()/JoinRoute()
  // into a local callback.
  void RouteResponseReceived(
      const std::string& presentation_id,
      bool off_the_record,
      const std::vector<MediaRouteResponseCallback>& callbacks,
      interfaces::MediaRoutePtr media_route,
      const mojo::String& error_text,
      interfaces::RouteRequestResultCode result_code);

  // Callback invoked by |event_page_tracker_| after an attempt to wake the
  // component extension. If |success| is false, the pending request queue is
  // drained.
  void EventPageWakeComplete(bool success);

  // Removes all requests from the pending requests queue. Called when there is
  // a permanent error connecting to component extension.
  void DrainRequestQueue();

  // Calls to |event_page_tracker_| to wake the component extension.
  // |media_route_provider_extension_id_| must not be empty and the extension
  // should be currently suspended.
  // If there have already been too many wakeup attempts, give up and drain
  // the pending request queue.
  void AttemptWakeEventPage();

  // Sets the reason why we are attempting to wake the extension.  Since
  // multiple tasks may be enqueued for execution each time the extension runs,
  // we record the first such reason.
  void SetWakeReason(MediaRouteProviderWakeReason reason);

  // Clears the wake reason after the extension has been awoken.
  void ClearWakeReason();

#if defined(OS_WIN)
  // Ensures that mDNS discovery is enabled in the MRPM extension. This can be
  // called many times but the MRPM will only be called once per registration
  // period.
  void EnsureMdnsDiscoveryEnabled();
  void DoEnsureMdnsDiscoveryEnabled();

  // Callback used to enabled mDNS in the MRPM if a firewall prompt will not be
  // triggered. If a firewall prompt would be triggered, enabling mDNS won't
  // happen until the user is clearly interacting with MR.
  void OnFirewallCheckComplete(bool firewall_can_use_local_ports);
#endif

  // Requests MRPM to update media sinks.  This allows MRPs that only do
  // discovery on sink queries an opportunity to update discovery results
  // even if the MRP SinkAvailability is marked UNAVAILABLE.
  void UpdateMediaSinks(const MediaSource::Id& source_id);
  void DoUpdateMediaSinks(const MediaSource::Id& source_id);

  // Pending requests queued to be executed once component extension
  // becomes ready.
  std::deque<base::Closure> pending_requests_;

  base::ScopedPtrHashMap<MediaSource::Id, std::unique_ptr<MediaSinksQuery>>
      sinks_queries_;

  base::ScopedPtrHashMap<MediaSource::Id, std::unique_ptr<MediaRoutesQuery>>
      routes_queries_;

  std::unordered_map<std::string, std::vector<MediaSinkSearchResponseCallback>>
      search_callbacks_;

  using PresentationSessionMessagesObserverList =
      base::ObserverList<PresentationSessionMessagesObserver>;
  base::ScopedPtrHashMap<
      MediaRoute::Id,
      std::unique_ptr<PresentationSessionMessagesObserverList>>
      messages_observers_;

  // IDs of MediaRoutes being listened for messages. Note that this is
  // different from |message_observers_| because we might be waiting for
  // |OnRouteMessagesReceived()| to be invoked after all observers for that
  // route have been removed.
  std::set<MediaRoute::Id> route_ids_listening_for_messages_;

  IssueManager issue_manager_;

  // Binds |this| to a Mojo connection stub for interfaces::MediaRouter.
  std::unique_ptr<mojo::Binding<interfaces::MediaRouter>> binding_;

  // Mojo proxy object for the Media Route Provider Manager.
  // Set to null initially, and later set to the Provider Manager proxy object
  // passed in via |RegisterMediaRouteProvider()|.
  // This is set to null again when the component extension is suspended
  // if or a Mojo channel error occured.
  interfaces::MediaRouteProviderPtr media_route_provider_;

  // Id of the component extension. Used for managing its suspend/wake state
  // via event_page_tracker_.
  std::string media_route_provider_extension_id_;

  // Allows the extension to be monitored for suspend, and woken.
  // This is a reference to a BrowserContext keyed service that outlives this
  // instance.
  extensions::EventPageTracker* event_page_tracker_;

  // GUID unique to each browser run. Component extension uses this to detect
  // when its persisted state was written by an older browser instance, and is
  // therefore stale.
  std::string instance_id_;

  // The last reported sink availability from the media route provider manager.
  interfaces::MediaRouter::SinkAvailability availability_;

  int wakeup_attempt_count_ = 0;

  // Records the current reason the extension is being woken up.  Is set to
  // MediaRouteProviderWakeReason::TOTAL_COUNT if there is no pending reason.
  MediaRouteProviderWakeReason current_wake_reason_;

  // A flag to ensure that we record the provider version once, during the
  // initial event page wakeup attempt.
  bool provider_version_was_recorded_ = false;

#if defined(OS_WIN)
  // A pair of flags to ensure that mDNS discovery is only enabled on Windows
  // when there will be appropriate context for the user to associate a firewall
  // prompt with Media Router. |should_enable_mdns_discovery_| can only go from
  // |false| to |true|. On Windows, |is_mdns_enabled_| is set to |false| in
  // RegisterMediaRouteProvider and only set to |true| when we successfully call
  // the extension to enable mDNS.
  bool is_mdns_enabled_ = false;
  bool should_enable_mdns_discovery_ = false;
#endif

  base::WeakPtrFactory<MediaRouterMojoImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterMojoImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_
