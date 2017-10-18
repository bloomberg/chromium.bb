// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_client.h"
#include "content/renderer/service_worker/embedded_worker_devtools_agent.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "content/renderer/service_worker/web_service_worker_installed_scripts_manager_impl.h"
#include "third_party/WebKit/public/platform/WebContentSettingsClient.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerInstalledScriptsManager.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorker.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorkerStartData.h"

namespace content {

EmbeddedWorkerInstanceClientImpl::WorkerWrapper::WorkerWrapper(
    std::unique_ptr<blink::WebEmbeddedWorker> worker,
    int devtools_agent_route_id)
    : worker_(std::move(worker)),
      devtools_agent_(base::MakeUnique<EmbeddedWorkerDevToolsAgent>(
          worker_.get(),
          devtools_agent_route_id)) {}

EmbeddedWorkerInstanceClientImpl::WorkerWrapper::~WorkerWrapper() = default;

// static
void EmbeddedWorkerInstanceClientImpl::Create(
    base::TimeTicks blink_initialized_time,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner,
    mojom::EmbeddedWorkerInstanceClientAssociatedRequest request) {
  // This won't be leaked because the lifetime will be managed internally.
  // See the class documentation for detail.
  EmbeddedWorkerInstanceClientImpl* client =
      new EmbeddedWorkerInstanceClientImpl(std::move(io_thread_runner),
                                           std::move(request));
  client->blink_initialized_time_ = blink_initialized_time;
}

void EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed() {
  DCHECK(wrapper_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed");
  // Destroys |this|.
  wrapper_.reset();
}

void EmbeddedWorkerInstanceClientImpl::StartWorker(
    const EmbeddedWorkerStartParams& params,
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
    mojom::ControllerServiceWorkerRequest controller_request,
    mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info,
    mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
    blink::mojom::WorkerContentSettingsProxyPtr content_settings_proxy) {
  DCHECK(ChildThreadImpl::current());
  DCHECK(!wrapper_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::StartWorker");
  auto interface_provider = std::move(provider_info->interface_provider);
  auto client = base::MakeUnique<ServiceWorkerContextClient>(
      params.embedded_worker_id, params.service_worker_version_id, params.scope,
      params.script_url,
      ServiceWorkerUtils::IsScriptStreamingEnabled() && installed_scripts_info,
      std::move(dispatcher_request), std::move(controller_request),
      std::move(instance_host), std::move(provider_info),
      std::move(temporal_self_));
  client->set_blink_initialized_time(blink_initialized_time_);
  client->set_start_worker_received_time(base::TimeTicks::Now());
  wrapper_ = StartWorkerContext(
      params, std::move(installed_scripts_info), std::move(client),
      std::move(content_settings_proxy), std::move(interface_provider));
}

void EmbeddedWorkerInstanceClientImpl::StopWorker() {
  // StopWorker must be called after StartWorker is called.
  DCHECK(ChildThreadImpl::current());
  DCHECK(wrapper_);

  TRACE_EVENT0("ServiceWorker", "EmbeddedWorkerInstanceClientImpl::StopWorker");
  wrapper_->worker()->TerminateWorkerContext();
}

void EmbeddedWorkerInstanceClientImpl::ResumeAfterDownload() {
  DCHECK(wrapper_);
  DCHECK(wrapper_->worker());
  wrapper_->worker()->ResumeAfterDownload();
}

void EmbeddedWorkerInstanceClientImpl::AddMessageToConsole(
    blink::WebConsoleMessage::Level level,
    const std::string& message) {
  DCHECK(wrapper_);
  DCHECK(wrapper_->worker());
  wrapper_->worker()->AddMessageToConsole(
      blink::WebConsoleMessage(level, blink::WebString::FromUTF8(message)));
}

EmbeddedWorkerInstanceClientImpl::EmbeddedWorkerInstanceClientImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner,
    mojom::EmbeddedWorkerInstanceClientAssociatedRequest request)
    : binding_(this, std::move(request)),
      temporal_self_(this),
      io_thread_runner_(std::move(io_thread_runner)) {
  binding_.set_connection_error_handler(base::BindOnce(
      &EmbeddedWorkerInstanceClientImpl::OnError, base::Unretained(this)));
}

EmbeddedWorkerInstanceClientImpl::~EmbeddedWorkerInstanceClientImpl() {}

void EmbeddedWorkerInstanceClientImpl::OnError() {
  // Destroys |this| if |temporal_self_| still owns this (i.e., StartWorker()
  // was not yet called).
  temporal_self_.reset();
}

std::unique_ptr<EmbeddedWorkerInstanceClientImpl::WorkerWrapper>
EmbeddedWorkerInstanceClientImpl::StartWorkerContext(
    const EmbeddedWorkerStartParams& params,
    mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info,
    std::unique_ptr<ServiceWorkerContextClient> context_client,
    blink::mojom::WorkerContentSettingsProxyPtr content_settings_proxy,
    service_manager::mojom::InterfaceProviderPtr interface_provider) {
  std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManager> manager;
  // |installed_scripts_info| is null if scripts should be served by net layer,
  // when the worker is not installed, or the worker is launched for checking
  // the update.
  if (ServiceWorkerUtils::IsScriptStreamingEnabled() &&
      installed_scripts_info) {
    manager = WebServiceWorkerInstalledScriptsManagerImpl::Create(
        std::move(installed_scripts_info), io_thread_runner_);
  }

  auto wrapper = base::MakeUnique<WorkerWrapper>(
      blink::WebEmbeddedWorker::Create(
          std::move(context_client), std::move(manager),
          content_settings_proxy.PassInterface().PassHandle(),
          interface_provider.PassInterface().PassHandle()),
      params.worker_devtools_agent_route_id);

  blink::WebEmbeddedWorkerStartData start_data;
  start_data.script_url = params.script_url;
  start_data.user_agent =
      blink::WebString::FromUTF8(GetContentClient()->GetUserAgent());
  start_data.wait_for_debugger_mode =
      params.wait_for_debugger
          ? blink::WebEmbeddedWorkerStartData::kWaitForDebugger
          : blink::WebEmbeddedWorkerStartData::kDontWaitForDebugger;
  start_data.v8_cache_options = static_cast<blink::WebSettings::V8CacheOptions>(
      params.settings.v8_cache_options);
  start_data.data_saver_enabled = params.settings.data_saver_enabled;
  start_data.pause_after_download_mode =
      params.pause_after_download
          ? blink::WebEmbeddedWorkerStartData::kPauseAfterDownload
          : blink::WebEmbeddedWorkerStartData::kDontPauseAfterDownload;

  wrapper->worker()->StartWorkerContext(start_data);
  return wrapper;
}

}  // namespace content
