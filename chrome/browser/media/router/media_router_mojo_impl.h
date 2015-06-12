// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_MOJO_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_MOJO_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class EventPageTracker;
}

namespace media_router {

// MediaRouter implementation that delegates calls to the component extension.
// Also handles the suspension and wakeup of the component extension.
class MediaRouterMojoImpl : public MediaRouter,
                            public interfaces::MediaRouterObserver,
                            public mojo::ErrorHandler,
                            public KeyedService {
 public:
  ~MediaRouterMojoImpl() override;

  // Sets up the MediaRouterMojoImpl instance owned by |context| to handle
  // MediaRouterObserver requests from the component extension given by
  // |extension_id|. Creates the MediaRouterMojoImpl instance if it does not
  // exist.
  // Called by the Mojo module registry.
  // |extension_id|: The ID of the component extension, used for querying
  //     suspension state.
  // |context|: The BrowserContext which owns the extension process.
  // |request|: The Mojo connection request used for binding.
  static void BindToRequest(
      const std::string& extension_id,
      content::BrowserContext* context,
      mojo::InterfaceRequest<interfaces::MediaRouterObserver> request);

  // MediaRouter implementation.
  // Execution of the requests is delegated to the Do* methods, which can be
  // enqueued for later use if the extension is temporarily suspended.
  void CreateRoute(const MediaSource::Id& source_id,
                   const MediaSink::Id& sink_id,
                   const MediaRouteResponseCallback& callback) override;
  void CloseRoute(const MediaRoute::Id& route_id) override;
  void SendRouteMessage(const MediaRoute::Id& route_id,
                        const std::string& message,
                        const SendRouteMessageCallback& callback) override;
  void ClearIssue(const Issue::Id& issue_id) override;
  void RegisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void UnregisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void AddIssuesObserver(IssuesObserver* observer) override;
  void RemoveIssuesObserver(IssuesObserver* observer) override;

  void set_instance_id_for_test(const std::string& instance_id) {
    instance_id_ = instance_id;
  }

 private:
  friend class MediaRouterMojoImplFactory;
  friend class MediaRouterMojoTest;

  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoExtensionTest,
                           DeferredBindingAndSuspension);

  // Standard constructor, used by
  // MediaRouterMojoImplFactory::GetApiForBrowserContext.
  explicit MediaRouterMojoImpl(
      extensions::EventPageTracker* event_page_tracker);

  // Binds |this| to a Mojo interface request, so that clients can acquire a
  // handle to a MediaRouterMojoImpl instance via the Mojo service connector.
  // Stores the |extension_id| of the component extension.
  void BindToMojoRequest(
      mojo::InterfaceRequest<interfaces::MediaRouterObserver> request,
      const std::string& extension_id);

  // Enqueues a closure for later execution by ExecutePendingRequests().
  void EnqueueTask(const base::Closure& closure);

  // Runs a closure if the extension monitored by |extension_monitor_| is
  // active, or defers it for later execution if the extension is suspended.
  void RunOrDefer(const base::Closure& request);

  // Dispatches the Mojo requests queued in |pending_requests_|.
  void ExecutePendingRequests();

  // These calls invoke methods in the component extension via Mojo.
  void DoCreateRoute(const MediaSource::Id& source_id,
                     const MediaSink::Id& sink_id,
                     const MediaRouteResponseCallback& callback);
  void DoCloseRoute(const MediaRoute::Id& route_id);
  void DoSendSessionMessage(const MediaRoute::Id& route_id,
                            const std::string& message,
                            const SendRouteMessageCallback& callback);
  void DoClearIssue(const Issue::Id& issue_id);
  void DoStartObservingMediaSinks(const std::string& source_id);
  void DoStopObservingMediaSinks(const std::string& source_id);
  void DoStartObservingMediaRoutes();
  void DoStopObservingMediaRoutes();
  void DoStartObservingIssues();
  void DoStopObservingIssues();

  // mojo::ErrorHandler implementation.
  void OnConnectionError() override;

  // interfaces::MediaRouterObserver implementation.
  void ProvideMediaRouter(
      interfaces::MediaRouterPtr media_router_ptr,
      const interfaces::MediaRouterObserver::ProvideMediaRouterCallback&
          callback) override;
  void OnMessage(const mojo::String& route_id,
                 const mojo::String& message) override;
  void OnIssue(interfaces::IssuePtr issue) override;
  void OnSinksReceived(const mojo::String& media_source,
                       mojo::Array<interfaces::MediaSinkPtr> sinks) override;
  void OnRoutesUpdated(mojo::Array<interfaces::MediaRoutePtr> routes) override;

  // Pending requests queued to be executed once component extension
  // becomes ready.
  std::vector<base::Closure> pending_requests_;

  base::ScopedPtrHashMap<MediaSource::Id,
                         scoped_ptr<base::ObserverList<MediaSinksObserver>>>
      sinks_observers_;

  base::ObserverList<MediaRoutesObserver> routes_observers_;

  // Binds |this| to listen for updates from the component extension Media
  // Router.
  scoped_ptr<mojo::Binding<interfaces::MediaRouterObserver>> binding_;

  // Mojo proxy object for the component extension Media Router.
  // Set to null initially, and set to the proxy to the component extension
  // on |ProvideMediaRouter()|. This is set to null again when the component
  // extension becomes suspended or if there is a connection error.
  interfaces::MediaRouterPtr mojo_media_router_;

  // Id of the component extension. Used for managing its suspend/wake state
  // via event_page_tracker_.
  std::string mojo_media_router_extension_id_;

  // Allows the extension to be monitored for suspend, and woken.
  // This is a reference to a BrowserContext keyed service that outlives this
  // instance.
  extensions::EventPageTracker* event_page_tracker_;

  // GUID unique to each browser run. Component extension uses this to detect
  // when its persisted state was written by an older browser instance, and is
  // therefore stale.
  std::string instance_id_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterMojoImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_MOJO_IMPL_H_
