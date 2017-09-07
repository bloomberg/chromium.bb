// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DESKTOP_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DESKTOP_H_

#include "build/build_config.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {
class Extension;
}

namespace media_router {
class EventPageRequestManager;

// MediaRouter implementation that uses the MediaRouteProvider implemented in
// the component extension.
class MediaRouterDesktop : public MediaRouterMojoImpl {
 public:
  ~MediaRouterDesktop() override;

  // Sets up the MediaRouter instance owned by |context| to handle
  // MediaRouterObserver requests from the component extension given by
  // |extension|. Creates the MediaRouterMojoImpl instance if it does not
  // exist.
  // Called by the Mojo module registry.
  // |extension|: The component extension, used for querying
  //     suspension state.
  // |context|: The BrowserContext which owns the extension process.
  // |request|: The Mojo connection request used for binding.
  static void BindToRequest(const extensions::Extension* extension,
                            content::BrowserContext* context,
                            mojom::MediaRouterRequest request,
                            content::RenderFrameHost* source);

  // MediaRouter implementation.
  void OnUserGesture() override;

 protected:
  // These methods override MediaRouterMojoImpl methods. They call the base
  // methods if the connection to the MediaRouteProvider is valid. Otherwise,
  // they queue themselves to be called again later by |request_manager_| when
  // the connection is valid.
  void DoCreateRoute(const MediaSource::Id& source_id,
                     const MediaSink::Id& sink_id,
                     const url::Origin& origin,
                     int tab_id,
                     std::vector<MediaRouteResponseCallback> callbacks,
                     base::TimeDelta timeout,
                     bool incognito) override;
  void DoJoinRoute(const MediaSource::Id& source_id,
                   const std::string& presentation_id,
                   const url::Origin& origin,
                   int tab_id,
                   std::vector<MediaRouteResponseCallback> callbacks,
                   base::TimeDelta timeout,
                   bool incognito) override;
  void DoConnectRouteByRouteId(
      const MediaSource::Id& source_id,
      const MediaRoute::Id& route_id,
      const url::Origin& origin,
      int tab_id,
      std::vector<MediaRouteResponseCallback> callbacks,
      base::TimeDelta timeout,
      bool incognito) override;
  void DoTerminateRoute(const MediaRoute::Id& route_id) override;
  void DoDetachRoute(const MediaRoute::Id& route_id) override;
  void DoSendRouteMessage(const MediaRoute::Id& route_id,
                          const std::string& message,
                          SendRouteMessageCallback callback) override;
  void DoSendRouteBinaryMessage(const MediaRoute::Id& route_id,
                                std::unique_ptr<std::vector<uint8_t>> data,
                                SendRouteMessageCallback callback) override;
  void DoStartListeningForRouteMessages(
      const MediaRoute::Id& route_id) override;
  void DoStopListeningForRouteMessages(const MediaRoute::Id& route_id) override;
  void DoStartObservingMediaSinks(const MediaSource::Id& source_id) override;
  void DoStopObservingMediaSinks(const MediaSource::Id& source_id) override;
  void DoStartObservingMediaRoutes(const MediaSource::Id& source_id) override;
  void DoStopObservingMediaRoutes(const MediaSource::Id& source_id) override;
  void DoSearchSinks(const MediaSink::Id& sink_id,
                     const MediaSource::Id& source_id,
                     const std::string& search_input,
                     const std::string& domain,
                     MediaSinkSearchResponseCallback sink_callback) override;
  void DoCreateMediaRouteController(MediaRouteController* controller) override;
  void DoProvideSinks(const std::string& provider_name,
                      std::vector<MediaSinkInternal> sinks) override;
  void DoUpdateMediaSinks(const MediaSource::Id& source_id) override;
#if defined(OS_WIN)
  void DoEnsureMdnsDiscoveryEnabled();
#endif

  // This is the version that gets queued (instead of
  // DoCreateMediaRouteController) if CreateMediaRouteController is deferred.
  // This is because the MediaRouteController may be deleted by the time the
  // callback is executed, so we need to perform a check that the controller
  // exists. Note that it is possible that the original controller has been
  // deleted and replaced with another instance. The base class
  // DoCreateMediaRouteController should be handling this case correctly
  // already.
  void CreateMediaRouteControllerDeferred(const MediaRoute::Id& route_id);

  // Error handler callback for |binding_| and |media_route_provider_|.
  void OnConnectionError() override;

  // Issues 0+ calls to |media_route_provider_| to ensure its state is in sync
  // with MediaRouter on a best-effort basis.
  // The extension might have become out of sync with MediaRouter due to one
  // of few reasons:
  // (1) The extension crashed and lost unpersisted changes.
  // (2) The extension was updated; temporary data is cleared.
  // (3) The extension has an unforseen bug which causes temporary data to be
  //     persisted incorrectly on suspension.
  void SyncStateToMediaRouteProvider() override;

 private:
  friend class MediaRouterDesktopTest;
  friend class MediaRouterDesktopTestTest;
  friend class MediaRouterFactory;

  enum class FirewallCheck {
    // Skips the firewall check for the benefit of unit tests so they do not
    // have to depend on the system's firewall configuration.
    SKIP_FOR_TESTING,
    // Perform the firewall check (default).
    RUN,
  };

  MediaRouterDesktop(content::BrowserContext* context,
                     FirewallCheck check_firewall = FirewallCheck::RUN);

  // mojom::MediaRouter implementation.
  // Notifies |request_manager_| that the Mojo connection to MediaRouteProvider
  // is valid.
  void RegisterMediaRouteProvider(
      mojom::MediaRouteProviderPtr media_route_provider_ptr,
      mojom::MediaRouter::RegisterMediaRouteProviderCallback callback) override;

  // Binds |this| to a Mojo interface request, so that clients can acquire a
  // handle to a MediaRouter instance via the Mojo service connector.
  // Passes the extension's ID to the event page request manager.
  void BindToMojoRequest(mojo::InterfaceRequest<mojom::MediaRouter> request,
                         const extensions::Extension& extension);

#if defined(OS_WIN)
  // Ensures that mDNS discovery is enabled in the MRPM extension. This can be
  // called many times but the MRPM will only be called once per registration
  // period.
  void EnsureMdnsDiscoveryEnabled();

  // Callback used to enable mDNS in the MRPM if a firewall prompt will not be
  // triggered. If a firewall prompt would be triggered, enabling mDNS won't
  // happen until the user is clearly interacting with MR.
  void OnFirewallCheckComplete(bool firewall_can_use_local_ports);
#endif

  // Request manager responsible for waking the component extension and calling
  // the requests to it.
  EventPageRequestManager* const request_manager_;

  // Binds |this| to a Mojo connection stub for mojom::MediaRouter.
  mojo::Binding<mojom::MediaRouter> binding_;

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

  base::WeakPtrFactory<MediaRouterDesktop> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDesktop);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DESKTOP_H_
