// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/mojo/media_route_controller.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "extensions/common/extension.h"
#if defined(OS_WIN)
#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"
#endif

namespace media_router {

MediaRouterDesktop::~MediaRouterDesktop() = default;

// static
void MediaRouterDesktop::BindToRequest(const extensions::Extension* extension,
                                       content::BrowserContext* context,
                                       mojom::MediaRouterRequest request,
                                       content::RenderFrameHost* source) {
  MediaRouterDesktop* impl = static_cast<MediaRouterDesktop*>(
      MediaRouterFactory::GetApiForBrowserContext(context));
  DCHECK(impl);

  impl->BindToMojoRequest(std::move(request), *extension);
}

void MediaRouterDesktop::OnUserGesture() {
  MediaRouterMojoImpl::OnUserGesture();
  // Allow MRPM to intelligently update sinks and observers by passing in a
  // media source.
  UpdateMediaSinks(MediaSourceForDesktop().id());

#if defined(OS_WIN)
  EnsureMdnsDiscoveryEnabled();
#endif
}

void MediaRouterDesktop::DoCreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const url::Origin& origin,
    int tab_id,
    std::vector<MediaRouteResponseCallback> callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoCreateRoute(source_id, sink_id, origin, tab_id,
                                       std::move(callbacks), timeout,
                                       incognito);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoCreateRoute,
                     weak_factory_.GetWeakPtr(), source_id, sink_id, origin,
                     tab_id, std::move(callbacks), timeout, incognito),
      MediaRouteProviderWakeReason::CREATE_ROUTE);
}

void MediaRouterDesktop::DoJoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int tab_id,
    std::vector<MediaRouteResponseCallback> callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoJoinRoute(source_id, presentation_id, origin, tab_id,
                                     std::move(callbacks), timeout, incognito);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoJoinRoute,
                     weak_factory_.GetWeakPtr(), source_id, presentation_id,
                     origin, tab_id, std::move(callbacks), timeout, incognito),
      MediaRouteProviderWakeReason::JOIN_ROUTE);
}

void MediaRouterDesktop::DoConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const url::Origin& origin,
    int tab_id,
    std::vector<MediaRouteResponseCallback> callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoConnectRouteByRouteId(source_id, route_id, origin,
                                                 tab_id, std::move(callbacks),
                                                 timeout, incognito);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoConnectRouteByRouteId,
                     weak_factory_.GetWeakPtr(), source_id, route_id, origin,
                     tab_id, std::move(callbacks), timeout, incognito),
      MediaRouteProviderWakeReason::CONNECT_ROUTE_BY_ROUTE_ID);
}

void MediaRouterDesktop::DoTerminateRoute(const MediaRoute::Id& route_id) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoTerminateRoute(route_id);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoTerminateRoute,
                     weak_factory_.GetWeakPtr(), route_id),
      MediaRouteProviderWakeReason::TERMINATE_ROUTE);
}

void MediaRouterDesktop::DoDetachRoute(const MediaRoute::Id& route_id) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoDetachRoute(route_id);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoDetachRoute,
                     weak_factory_.GetWeakPtr(), route_id),
      MediaRouteProviderWakeReason::DETACH_ROUTE);
}

void MediaRouterDesktop::DoSendRouteMessage(const MediaRoute::Id& route_id,
                                            const std::string& message,
                                            SendRouteMessageCallback callback) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoSendRouteMessage(route_id, message,
                                            std::move(callback));
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoSendRouteMessage,
                     weak_factory_.GetWeakPtr(), route_id, message,
                     std::move(callback)),
      MediaRouteProviderWakeReason::SEND_SESSION_MESSAGE);
}

void MediaRouterDesktop::DoSendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    std::unique_ptr<std::vector<uint8_t>> data,
    SendRouteMessageCallback callback) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoSendRouteBinaryMessage(route_id, std::move(data),
                                                  std::move(callback));
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoSendRouteBinaryMessage,
                     weak_factory_.GetWeakPtr(), route_id,
                     base::Passed(std::move(data)), std::move(callback)),
      MediaRouteProviderWakeReason::SEND_SESSION_BINARY_MESSAGE);
}

void MediaRouterDesktop::DoStartListeningForRouteMessages(
    const MediaRoute::Id& route_id) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoStartListeningForRouteMessages(route_id);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoStartListeningForRouteMessages,
                     weak_factory_.GetWeakPtr(), route_id),
      MediaRouteProviderWakeReason::START_LISTENING_FOR_ROUTE_MESSAGES);
}

void MediaRouterDesktop::DoStopListeningForRouteMessages(
    const MediaRoute::Id& route_id) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoStopListeningForRouteMessages(route_id);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoStopListeningForRouteMessages,
                     weak_factory_.GetWeakPtr(), route_id),
      MediaRouteProviderWakeReason::STOP_LISTENING_FOR_ROUTE_MESSAGES);
}

void MediaRouterDesktop::DoStartObservingMediaSinks(
    const MediaSource::Id& source_id) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoStartObservingMediaSinks(source_id);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoStartObservingMediaSinks,
                     weak_factory_.GetWeakPtr(), source_id),
      MediaRouteProviderWakeReason::START_OBSERVING_MEDIA_SINKS);
}

void MediaRouterDesktop::DoStopObservingMediaSinks(
    const MediaSource::Id& source_id) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoStopObservingMediaSinks(source_id);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoStopObservingMediaSinks,
                     weak_factory_.GetWeakPtr(), source_id),
      MediaRouteProviderWakeReason::STOP_OBSERVING_MEDIA_SINKS);
}

void MediaRouterDesktop::DoStartObservingMediaRoutes(
    const MediaSource::Id& source_id) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoStartObservingMediaRoutes(source_id);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoStartObservingMediaRoutes,
                     weak_factory_.GetWeakPtr(), source_id),
      MediaRouteProviderWakeReason::START_OBSERVING_MEDIA_ROUTES);
}

void MediaRouterDesktop::DoStopObservingMediaRoutes(
    const MediaSource::Id& source_id) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoStopObservingMediaRoutes(source_id);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoStopObservingMediaRoutes,
                     weak_factory_.GetWeakPtr(), source_id),
      MediaRouteProviderWakeReason::STOP_OBSERVING_MEDIA_ROUTES);
}

void MediaRouterDesktop::DoSearchSinks(
    const MediaSink::Id& sink_id,
    const MediaSource::Id& source_id,
    const std::string& search_input,
    const std::string& domain,
    MediaSinkSearchResponseCallback sink_callback) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoSearchSinks(sink_id, source_id, search_input, domain,
                                       std::move(sink_callback));
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoSearchSinks,
                     weak_factory_.GetWeakPtr(), sink_id, source_id,
                     search_input, domain, std::move(sink_callback)),
      MediaRouteProviderWakeReason::SEARCH_SINKS);
}

void MediaRouterDesktop::DoCreateMediaRouteController(
    const MediaRoute::Id& route_id,
    mojom::MediaControllerRequest mojo_media_controller_request,
    mojom::MediaStatusObserverPtr mojo_observer) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoCreateMediaRouteController(
        route_id, std::move(mojo_media_controller_request),
        std::move(mojo_observer));
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoCreateMediaRouteController,
                     weak_factory_.GetWeakPtr(), route_id,
                     std::move(mojo_media_controller_request),
                     std::move(mojo_observer)),
      MediaRouteProviderWakeReason::CREATE_MEDIA_ROUTE_CONTROLLER);
}

void MediaRouterDesktop::DoProvideSinks(const std::string& provider_name,
                                        std::vector<MediaSinkInternal> sinks) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoProvideSinks(provider_name, std::move(sinks));
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoProvideSinks,
                     weak_factory_.GetWeakPtr(), provider_name,
                     std::move(sinks)),
      MediaRouteProviderWakeReason::PROVIDE_SINKS);
}

void MediaRouterDesktop::DoUpdateMediaSinks(const MediaSource::Id& source_id) {
  if (request_manager_->mojo_connections_ready()) {
    MediaRouterMojoImpl::DoUpdateMediaSinks(source_id);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoUpdateMediaSinks,
                     weak_factory_.GetWeakPtr(), source_id),
      MediaRouteProviderWakeReason::UPDATE_MEDIA_SINKS);
}

#if defined(OS_WIN)
void MediaRouterDesktop::DoEnsureMdnsDiscoveryEnabled() {
  if (request_manager_->mojo_connections_ready()) {
    DVLOG(1) << "DoEnsureMdnsDiscoveryEnabled";
    if (!is_mdns_enabled_) {
      media_route_provider_->EnableMdnsDiscovery();
      is_mdns_enabled_ = true;
    }
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouterDesktop::DoEnsureMdnsDiscoveryEnabled,
                     weak_factory_.GetWeakPtr()),
      MediaRouteProviderWakeReason::ENABLE_MDNS_DISCOVERY);
}
#endif

void MediaRouterDesktop::OnConnectionError() {
  request_manager_->OnMojoConnectionError();
  binding_.Close();
  MediaRouterMojoImpl::OnConnectionError();
}

void MediaRouterDesktop::SyncStateToMediaRouteProvider() {
#if defined(OS_WIN)
  // The MRPM extension already turns on mDNS discovery for platforms other than
  // Windows. It only relies on this signalling from MR on Windows to avoid
  // triggering a firewall prompt out of the context of MR from the user's
  // perspective. This particular call reminds the extension to enable mDNS
  // discovery when it wakes up, has been upgraded, etc.
  if (should_enable_mdns_discovery_) {
    DoEnsureMdnsDiscoveryEnabled();
  }
#endif
  MediaRouterMojoImpl::SyncStateToMediaRouteProvider();
}

MediaRouterDesktop::MediaRouterDesktop(content::BrowserContext* context,
                                       FirewallCheck check_firewall)
    : MediaRouterMojoImpl(context),
      request_manager_(
          EventPageRequestManagerFactory::GetApiForBrowserContext(context)),
      binding_(this),
      weak_factory_(this) {
  DCHECK(request_manager_);
#if defined(OS_WIN)
  if (check_firewall == FirewallCheck::RUN) {
    CanFirewallUseLocalPorts(
        base::BindOnce(&MediaRouterDesktop::OnFirewallCheckComplete,
                       weak_factory_.GetWeakPtr()));
  }
#endif
}

void MediaRouterDesktop::RegisterMediaRouteProvider(
    mojom::MediaRouteProviderPtr media_route_provider_ptr,
    mojom::MediaRouter::RegisterMediaRouteProviderCallback callback) {
  MediaRouterMojoImpl::RegisterMediaRouteProvider(
      std::move(media_route_provider_ptr), std::move(callback));
#if defined(OS_WIN)
  // The MRPM may have been upgraded or otherwise reload such that we could be
  // seeing an MRPM that doesn't know mDNS is enabled, even if we've told a
  // previously registered MRPM it should be enabled. Furthermore, there may be
  // a pending request to enable mDNS, so don't clear this flag after
  // ExecutePendingRequests().
  is_mdns_enabled_ = false;
#endif
  // Now that we have a Mojo pointer to the MRP, we request MRP-side route
  // controllers to be created again. This must happen before |request_manager_|
  // executes requests to the MRP and its route controllers.
  for (const auto& pair : route_controllers_) {
    const MediaRoute::Id& route_id = pair.first;
    MediaRouteController* route_controller = pair.second;
    MediaRouterMojoImpl::DoCreateMediaRouteController(
        route_id, route_controller->CreateControllerRequest(),
        route_controller->BindObserverPtr());
  }
  request_manager_->OnMojoConnectionsReady();
}

void MediaRouterDesktop::BindToMojoRequest(
    mojo::InterfaceRequest<mojom::MediaRouter> request,
    const extensions::Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(base::BindOnce(
      &MediaRouterDesktop::OnConnectionError, base::Unretained(this)));

  request_manager_->SetExtensionId(extension.id());
  if (!provider_version_was_recorded_) {
    MediaRouterMojoMetrics::RecordMediaRouteProviderVersion(extension);
    provider_version_was_recorded_ = true;
  }
}

#if defined(OS_WIN)
void MediaRouterDesktop::EnsureMdnsDiscoveryEnabled() {
  if (is_mdns_enabled_)
    return;

  DoEnsureMdnsDiscoveryEnabled();
  should_enable_mdns_discovery_ = true;
}

void MediaRouterDesktop::OnFirewallCheckComplete(
    bool firewall_can_use_local_ports) {
  if (firewall_can_use_local_ports)
    EnsureMdnsDiscoveryEnabled();
}
#endif

}  // namespace media_router
