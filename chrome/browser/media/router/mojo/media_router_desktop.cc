// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

#include "base/strings/string_util.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/mojo/media_route_controller.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/mojo/wired_display_media_route_provider.h"
#include "chrome/common/chrome_features.h"
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MediaRouterMojoImpl::OnUserGesture();
  // Allow MRPM to intelligently update sinks and observers by passing in a
  // media source.
  UpdateMediaSinks(MediaSourceForDesktop().id());

  if (dial_media_sink_service_)
    dial_media_sink_service_->OnUserGesture();

  if (cast_media_sink_service_)
    cast_media_sink_service_->OnUserGesture();

#if defined(OS_WIN)
  EnsureMdnsDiscoveryEnabled();
#endif
}

base::Optional<MediaRouteProviderId>
MediaRouterDesktop::GetProviderIdForPresentation(
    const std::string& presentation_id) {
  // TODO(takumif): Once the Android Media Router also uses MediaRouterMojoImpl,
  // we must support these presentation IDs in Android as well.
  if (presentation_id == kAutoJoinPresentationId ||
      base::StartsWith(presentation_id, kCastPresentationIdPrefix,
                       base::CompareCase::SENSITIVE)) {
    return MediaRouteProviderId::EXTENSION;
  }
  return MediaRouterMojoImpl::GetProviderIdForPresentation(presentation_id);
}

MediaRouterDesktop::MediaRouterDesktop(content::BrowserContext* context)
    : MediaRouterMojoImpl(context), weak_factory_(this) {
  InitializeMediaRouteProviders();
#if defined(OS_WIN)
  CanFirewallUseLocalPorts(
      base::BindOnce(&MediaRouterDesktop::OnFirewallCheckComplete,
                     weak_factory_.GetWeakPtr()));
#endif
}

MediaRouterDesktop::MediaRouterDesktop(
    content::BrowserContext* context,
    std::unique_ptr<DialMediaSinkService> dial_media_sink_service,
    std::unique_ptr<CastMediaSinkService> cast_media_sink_service)
    : MediaRouterMojoImpl(context),
      dial_media_sink_service_(std::move(dial_media_sink_service)),
      cast_media_sink_service_(std::move(cast_media_sink_service)),
      weak_factory_(this) {
  InitializeMediaRouteProviders();
}

void MediaRouterDesktop::RegisterMediaRouteProvider(
    MediaRouteProviderId provider_id,
    mojom::MediaRouteProviderPtr media_route_provider_ptr,
    mojom::MediaRouter::RegisterMediaRouteProviderCallback callback) {
  auto config = mojom::MediaRouteProviderConfig::New();
  // Enabling browser side discovery means disabling extension side discovery.
  // We are migrating discovery from the external Media Route Provider to the
  // Media Router (crbug.com/687383), so we need to disable it in the provider.
  config->enable_cast_discovery = !media_router::CastDiscoveryEnabled();
  std::move(callback).Run(instance_id(), std::move(config));

  SyncStateToMediaRouteProvider(provider_id);

  if (provider_id == MediaRouteProviderId::EXTENSION) {
    RegisterExtensionMediaRouteProvider(std::move(media_route_provider_ptr));
  } else {
    media_route_provider_ptr.set_connection_error_handler(
        base::BindOnce(&MediaRouterDesktop::OnProviderConnectionError,
                       weak_factory_.GetWeakPtr(), provider_id));
    media_route_providers_[provider_id] = std::move(media_route_provider_ptr);
  }
}

void MediaRouterDesktop::RegisterExtensionMediaRouteProvider(
    mojom::MediaRouteProviderPtr extension_provider_ptr) {
  StartDiscovery();
#if defined(OS_WIN)
  // The extension MRP already turns on mDNS discovery for platforms other than
  // Windows. It only relies on this signalling from MR on Windows to avoid
  // triggering a firewall prompt out of the context of MR from the user's
  // perspective. This particular call reminds the extension to enable mDNS
  // discovery when it wakes up, has been upgraded, etc.
  if (should_enable_mdns_discovery_)
    EnsureMdnsDiscoveryEnabled();
#endif
  // Now that we have a Mojo pointer to the extension MRP, we reset the Mojo
  // pointers to extension-side route controllers and request them to be bound
  // to new implementations. This must happen before EventPageRequestManager
  // executes commands to the MRP and its route controllers. Commands to the
  // route controllers, once executed, will be queued in Mojo pipes until the
  // Mojo requests are bound to implementations.
  // TODO(takumif): Once we have route controllers for MRPs other than the
  // extension MRP, we'll need to group them by MRP so that below is performed
  // only for extension route controllers.
  for (const auto& pair : route_controllers_)
    InitMediaRouteController(pair.second);
  extension_provider_proxy_->RegisterMediaRouteProvider(
      std::move(extension_provider_ptr));
}

void MediaRouterDesktop::BindToMojoRequest(
    mojo::InterfaceRequest<mojom::MediaRouter> request,
    const extensions::Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MediaRouterMojoImpl::BindToMojoRequest(std::move(request));
  extension_provider_proxy_->SetExtensionId(extension.id());
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
      cast_media_sink_service_ =
          std::make_unique<CastMediaSinkService>(context());
      cast_media_sink_service_->Start(
          base::BindRepeating(&MediaRouterDesktop::ProvideSinks,
                              weak_factory_.GetWeakPtr(), "cast"));
    } else {
      cast_media_sink_service_->ForceSinkDiscoveryCallback();
    }
  }

  if (!dial_media_sink_service_) {
    dial_media_sink_service_ =
        std::make_unique<DialMediaSinkService>(context());

    OnDialSinkAddedCallback dial_sink_added_cb;
    scoped_refptr<base::SequencedTaskRunner> dial_sink_added_cb_sequence;
    if (cast_media_sink_service_) {
      dial_sink_added_cb = cast_media_sink_service_->GetDialSinkAddedCallback();
      dial_sink_added_cb_sequence =
          cast_media_sink_service_->GetImplTaskRunner();
    }
    dial_media_sink_service_->Start(
        base::BindRepeating(&MediaRouterDesktop::ProvideSinks,
                            weak_factory_.GetWeakPtr(), "dial"),
        dial_sink_added_cb, dial_sink_added_cb_sequence);
  } else {
    dial_media_sink_service_->ForceSinkDiscoveryCallback();
  }
}

void MediaRouterDesktop::ProvideSinks(const std::string& provider_name,
                                      std::vector<MediaSinkInternal> sinks) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "Provider [" << provider_name << "] found " << sinks.size()
           << " devices...";
  extension_provider_proxy_->ProvideSinks(provider_name, std::move(sinks));
}

void MediaRouterDesktop::InitializeMediaRouteProviders() {
  InitializeExtensionMediaRouteProviderProxy();
  if (base::FeatureList::IsEnabled(features::kLocalScreenCasting))
    InitializeWiredDisplayMediaRouteProvider();
}

void MediaRouterDesktop::InitializeExtensionMediaRouteProviderProxy() {
  mojom::MediaRouteProviderPtr extension_provider_proxy_ptr;
  extension_provider_proxy_ =
      std::make_unique<ExtensionMediaRouteProviderProxy>(
          context(), mojo::MakeRequest(&extension_provider_proxy_ptr));
  media_route_providers_[MediaRouteProviderId::EXTENSION] =
      std::move(extension_provider_proxy_ptr);
}

void MediaRouterDesktop::InitializeWiredDisplayMediaRouteProvider() {
  mojom::MediaRouterPtr media_router_ptr;
  MediaRouterMojoImpl::BindToMojoRequest(mojo::MakeRequest(&media_router_ptr));
  mojom::MediaRouteProviderPtr wired_display_provider_ptr;
  wired_display_provider_ = std::make_unique<WiredDisplayMediaRouteProvider>(
      mojo::MakeRequest(&wired_display_provider_ptr),
      std::move(media_router_ptr), Profile::FromBrowserContext(context()));
  RegisterMediaRouteProvider(
      MediaRouteProviderId::WIRED_DISPLAY,
      std::move(wired_display_provider_ptr),
      base::BindOnce([](const std::string& instance_id,
                        mojom::MediaRouteProviderConfigPtr config) {}));
}

#if defined(OS_WIN)
void MediaRouterDesktop::EnsureMdnsDiscoveryEnabled() {
  if (cast_media_sink_service_) {
    cast_media_sink_service_->StartMdnsDiscovery();
  } else {
    media_route_providers_[MediaRouteProviderId::EXTENSION]
        ->EnableMdnsDiscovery();
  }

  // Record that we enabled mDNS discovery, so that we will know to enable again
  // when we reconnect to the component extension.
  should_enable_mdns_discovery_ = true;
}

void MediaRouterDesktop::OnFirewallCheckComplete(
    bool firewall_can_use_local_ports) {
  if (firewall_can_use_local_ports)
    EnsureMdnsDiscoveryEnabled();
}
#endif

}  // namespace media_router
