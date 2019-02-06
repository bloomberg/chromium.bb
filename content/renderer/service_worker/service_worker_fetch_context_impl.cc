// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_fetch_context_impl.h"

#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/content_constants_internal.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "ipc/ipc_message.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"

namespace content {

ServiceWorkerFetchContextImpl::ServiceWorkerFetchContextImpl(
    const blink::mojom::RendererPreferences& renderer_preferences,
    const GURL& worker_script_url,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        script_loader_factory_info,
    int service_worker_provider_id,
    std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
    std::unique_ptr<WebSocketHandshakeThrottleProvider>
        websocket_handshake_throttle_provider,
    blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request)
    : renderer_preferences_(renderer_preferences),
      worker_script_url_(worker_script_url),
      url_loader_factory_info_(std::move(url_loader_factory_info)),
      script_loader_factory_info_(std::move(script_loader_factory_info)),
      service_worker_provider_id_(service_worker_provider_id),
      throttle_provider_(std::move(throttle_provider)),
      websocket_handshake_throttle_provider_(
          std::move(websocket_handshake_throttle_provider)),
      preference_watcher_binding_(this),
      preference_watcher_request_(std::move(preference_watcher_request)) {}

ServiceWorkerFetchContextImpl::~ServiceWorkerFetchContextImpl() {}

void ServiceWorkerFetchContextImpl::SetTerminateSyncLoadEvent(
    base::WaitableEvent* terminate_sync_load_event) {
  DCHECK(!terminate_sync_load_event_);
  terminate_sync_load_event_ = terminate_sync_load_event;
}

void ServiceWorkerFetchContextImpl::InitializeOnWorkerThread(
    blink::AcceptLanguagesWatcher* watcher) {
  resource_dispatcher_ = std::make_unique<ResourceDispatcher>();
  resource_dispatcher_->set_terminate_sync_load_event(
      terminate_sync_load_event_);
  preference_watcher_binding_.Bind(std::move(preference_watcher_request_));

  web_url_loader_factory_ = std::make_unique<WebURLLoaderFactoryImpl>(
      resource_dispatcher_->GetWeakPtr(),
      network::SharedURLLoaderFactory::Create(
          std::move(url_loader_factory_info_)));

  if (script_loader_factory_info_) {
    web_script_loader_factory_ =
        std::make_unique<content::WebURLLoaderFactoryImpl>(
            resource_dispatcher_->GetWeakPtr(),
            network::SharedURLLoaderFactory::Create(
                std::move(script_loader_factory_info_)));
  }

  accept_languages_watcher_ = watcher;
}

blink::WebURLLoaderFactory*
ServiceWorkerFetchContextImpl::GetURLLoaderFactory() {
  return web_url_loader_factory_.get();
}

std::unique_ptr<blink::WebURLLoaderFactory>
ServiceWorkerFetchContextImpl::WrapURLLoaderFactory(
    mojo::ScopedMessagePipeHandle url_loader_factory_handle) {
  return std::make_unique<WebURLLoaderFactoryImpl>(
      resource_dispatcher_->GetWeakPtr(),
      base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
          network::mojom::URLLoaderFactoryPtrInfo(
              std::move(url_loader_factory_handle),
              network::mojom::URLLoaderFactory::Version_)));
}

blink::WebURLLoaderFactory*
ServiceWorkerFetchContextImpl::GetScriptLoaderFactory() {
  return web_script_loader_factory_.get();
}

void ServiceWorkerFetchContextImpl::WillSendRequest(
    blink::WebURLRequest& request) {
  if (renderer_preferences_.enable_do_not_track) {
    request.SetHTTPHeaderField(blink::WebString::FromUTF8(kDoNotTrackHeader),
                               "1");
  }
  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_service_worker_provider_id(service_worker_provider_id_);
  extra_data->set_originated_from_service_worker(true);
  extra_data->set_initiated_in_secure_context(true);
  if (throttle_provider_) {
    extra_data->set_url_loader_throttles(throttle_provider_->CreateThrottles(
        MSG_ROUTING_NONE, request, WebURLRequestToResourceType(request)));
  }
  request.SetExtraData(std::move(extra_data));

  if (!renderer_preferences_.enable_referrers) {
    request.SetHTTPReferrer(blink::WebString(),
                            network::mojom::ReferrerPolicy::kDefault);
  }
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerFetchContextImpl::IsControlledByServiceWorker() const {
  return blink::mojom::ControllerServiceWorkerMode::kNoController;
}

blink::WebURL ServiceWorkerFetchContextImpl::SiteForCookies() const {
  // According to the spec, we can use the |worker_script_url_| for
  // SiteForCookies, because "site for cookies" for the service worker is
  // the service worker's origin's host's registrable domain.
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-07#section-2.1.2
  return worker_script_url_;
}

base::Optional<blink::WebSecurityOrigin>
ServiceWorkerFetchContextImpl::TopFrameOrigin() const {
  // TODO(jkarlin): Determine what the top-frame-origin of a service worker is.
  // See https://crbug.com/918868.
  return base::nullopt;
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
ServiceWorkerFetchContextImpl::CreateWebSocketHandshakeThrottle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!websocket_handshake_throttle_provider_)
    return nullptr;
  return websocket_handshake_throttle_provider_->CreateThrottle(
      MSG_ROUTING_NONE, std::move(task_runner));
}

void ServiceWorkerFetchContextImpl::NotifyUpdate(
    blink::mojom::RendererPreferencesPtr new_prefs) {
  DCHECK(accept_languages_watcher_);
  if (renderer_preferences_.accept_languages != new_prefs->accept_languages)
    accept_languages_watcher_->NotifyUpdate();
  renderer_preferences_ = *new_prefs;
}

blink::WebString ServiceWorkerFetchContextImpl::GetAcceptLanguages() const {
  return blink::WebString::FromUTF8(renderer_preferences_.accept_languages);
}

}  // namespace content
