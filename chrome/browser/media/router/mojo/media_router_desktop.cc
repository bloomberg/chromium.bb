// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_proxy.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/mojo/media_route_controller.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "extensions/common/extension.h"
#if defined(OS_WIN)
#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"
#endif

namespace media_router {

MediaRouterDesktop::~MediaRouterDesktop() {
  if (dial_media_sink_service_proxy_) {
    dial_media_sink_service_proxy_->Stop();
    dial_media_sink_service_proxy_->ClearObserver(
        cast_media_sink_service_.get());
  }
  if (cast_media_sink_service_)
    cast_media_sink_service_->Stop();
}

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MediaRouterMojoImpl::OnUserGesture();
  // Allow MRPM to intelligently update sinks and observers by passing in a
  // media source.
  UpdateMediaSinks(MediaSourceForDesktop().id());
  if (cast_media_sink_service_)
    cast_media_sink_service_->ForceDiscovery();

#if defined(OS_WIN)
  EnsureMdnsDiscoveryEnabled();
#endif
}

void MediaRouterDesktop::OnConnectionError() {
  extension_provider_.OnMojoConnectionError();
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
  StartDiscovery();
}

MediaRouterDesktop::MediaRouterDesktop(content::BrowserContext* context,
                                       FirewallCheck check_firewall)
    : MediaRouterMojoImpl(context),
      extension_provider_(context, mojo::MakeRequest(&media_route_provider_)),
      weak_factory_(this) {
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
  // Now that we have a Mojo pointer to the extension MRP, we reset the Mojo
  // pointers to extension-side route controllers and request them to be bound
  // to new implementations. This must happen before EventPageRequestManager
  // executes commands to the MRP and its route controllers. Commands to the
  // route controllers, once executed, will be queued in Mojo pipes until the
  // Mojo requests are bound to implementations.
  for (const auto& pair : route_controllers_)
    InitMediaRouteController(pair.second);

  extension_provider_.RegisterMediaRouteProvider(
      std::move(media_route_provider_ptr));
}

void MediaRouterDesktop::BindToMojoRequest(
    mojo::InterfaceRequest<mojom::MediaRouter> request,
    const extensions::Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MediaRouterMojoImpl::BindToMojoRequest(std::move(request));
  extension_provider_.SetExtensionId(extension.id());
  if (!provider_version_was_recorded_) {
    MediaRouterMojoMetrics::RecordMediaRouteProviderVersion(extension);
    provider_version_was_recorded_ = true;
  }
}

void MediaRouterDesktop::StartDiscovery() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "StartDiscovery";

  if (media_router::CastDiscoveryEnabled()) {
    if (!cast_media_sink_service_) {
      cast_media_sink_service_ = base::MakeRefCounted<CastMediaSinkService>(
          base::BindRepeating(&MediaRouterMojoImpl::ProvideSinks,
                              weak_factory_.GetWeakPtr(), "cast"),
          context(),
          content::BrowserThread::GetTaskRunnerForThread(
              content::BrowserThread::IO));
    }
    cast_media_sink_service_->Start();
  }

  if (media_router::DialLocalDiscoveryEnabled()) {
    if (!dial_media_sink_service_proxy_) {
      dial_media_sink_service_proxy_ =
          base::MakeRefCounted<DialMediaSinkServiceProxy>(
              base::BindRepeating(&MediaRouterMojoImpl::ProvideSinks,
                                  weak_factory_.GetWeakPtr(), "dial"),
              context());
      dial_media_sink_service_proxy_->SetObserver(
          cast_media_sink_service_.get());
    }
    dial_media_sink_service_proxy_->Start();
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
