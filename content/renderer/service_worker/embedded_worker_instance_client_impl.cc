// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_client.h"
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
    std::unique_ptr<blink::WebEmbeddedWorker> worker)
    : worker_(std::move(worker)) {}

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
    mojom::EmbeddedWorkerStartParamsPtr params) {
  DCHECK(ChildThreadImpl::current());
  DCHECK(!wrapper_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::StartWorker");
  service_manager::mojom::InterfaceProviderPtr interface_provider(
      std::move(params->provider_info->interface_provider));
  const bool use_script_streaming =
      ServiceWorkerUtils::IsScriptStreamingEnabled() &&
      params->installed_scripts_info;
  auto client = std::make_unique<ServiceWorkerContextClient>(
      params->embedded_worker_id, params->service_worker_version_id,
      params->scope, params->script_url, use_script_streaming,
      std::move(params->dispatcher_request),
      std::move(params->controller_request),
      std::move(params->service_worker_host), std::move(params->instance_host),
      std::move(params->provider_info), std::move(temporal_self_),
      ChildThreadImpl::current()->thread_safe_sender(), io_thread_runner_);
  client->set_blink_initialized_time(blink_initialized_time_);
  client->set_start_worker_received_time(base::TimeTicks::Now());
  wrapper_ = StartWorkerContext(std::move(params), std::move(client),
                                std::move(interface_provider));
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

void EmbeddedWorkerInstanceClientImpl::BindDevToolsAgent(
    blink::mojom::DevToolsAgentAssociatedRequest request) {
  DCHECK(wrapper_);
  DCHECK(wrapper_->worker());
  wrapper_->worker()->BindDevToolsAgent(request.PassHandle());
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
    mojom::EmbeddedWorkerStartParamsPtr params,
    std::unique_ptr<ServiceWorkerContextClient> context_client,
    service_manager::mojom::InterfaceProviderPtr interface_provider) {
  std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManager> manager;
  // |installed_scripts_info| is null if scripts should be served by net layer,
  // when the worker is not installed, or the worker is launched for checking
  // the update.
  if (ServiceWorkerUtils::IsScriptStreamingEnabled() &&
      params->installed_scripts_info) {
    manager = WebServiceWorkerInstalledScriptsManagerImpl::Create(
        std::move(params->installed_scripts_info), io_thread_runner_);
  }

  auto wrapper =
      std::make_unique<WorkerWrapper>(blink::WebEmbeddedWorker::Create(
          std::move(context_client), std::move(manager),
          params->content_settings_proxy.PassHandle(),
          interface_provider.PassInterface().PassHandle()));

  blink::WebEmbeddedWorkerStartData start_data;
  start_data.script_url = params->script_url;
  start_data.user_agent =
      blink::WebString::FromUTF8(GetContentClient()->GetUserAgent());
  start_data.wait_for_debugger_mode =
      params->wait_for_debugger
          ? blink::WebEmbeddedWorkerStartData::kWaitForDebugger
          : blink::WebEmbeddedWorkerStartData::kDontWaitForDebugger;
  start_data.devtools_frame_token =
      blink::WebString::FromUTF8(params->devtools_worker_token.ToString());
  start_data.v8_cache_options =
      static_cast<blink::WebSettings::V8CacheOptions>(params->v8_cache_options);
  start_data.pause_after_download_mode =
      params->pause_after_download
          ? blink::WebEmbeddedWorkerStartData::kPauseAfterDownload
          : blink::WebEmbeddedWorkerStartData::kDontPauseAfterDownload;

  wrapper->worker()->StartWorkerContext(start_data);
  return wrapper;
}

}  // namespace content
