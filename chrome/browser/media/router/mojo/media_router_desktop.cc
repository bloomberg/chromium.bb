// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
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
  if (should_enable_mdns_discovery_)
    media_route_provider_->EnableMdnsDiscovery();
#endif
  MediaRouterMojoImpl::SyncStateToMediaRouteProvider();
}

MediaRouterDesktop::MediaRouterDesktop(content::BrowserContext* context,
                                       FirewallCheck check_firewall)
    : MediaRouterMojoImpl(context),
      request_manager_(
          EventPageRequestManagerFactory::GetApiForBrowserContext(context)),
      extension_provider_(context, mojo::MakeRequest(&media_route_provider_)),
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
  extension_provider_.RegisterMediaRouteProvider(
      std::move(media_route_provider_ptr));

  auto config = mojom::MediaRouteProviderConfig::New();
  // Enabling browser side discovery means disabling extension side discovery.
  // We are migrating discovery from the external Media Route Provider to the
  // Media Router (crbug.com/687383), so we need to disable it in the provider.
  config->enable_dial_discovery = !media_router::DialLocalDiscoveryEnabled();
  config->enable_cast_discovery = !media_router::CastDiscoveryEnabled();
  std::move(callback).Run(instance_id(), std::move(config));

  SyncStateToMediaRouteProvider();

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
    auto callback =
        base::BindOnce(&MediaRouterDesktop::OnMediaControllerCreated,
                       weak_factory_.GetWeakPtr(), route_id);
    media_route_provider_->CreateMediaRouteController(
        route_id, route_controller->CreateControllerRequest(),
        route_controller->BindObserverPtr(), std::move(callback));
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

  media_route_provider_->EnableMdnsDiscovery();
  should_enable_mdns_discovery_ = true;
}

void MediaRouterDesktop::OnFirewallCheckComplete(
    bool firewall_can_use_local_ports) {
  if (firewall_can_use_local_ports)
    EnsureMdnsDiscoveryEnabled();
}
#endif

}  // namespace media_router
