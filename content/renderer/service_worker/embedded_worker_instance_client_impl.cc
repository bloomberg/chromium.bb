// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/common/content_client.h"
#include "content/renderer/service_worker/embedded_worker_devtools_agent.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorker.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorkerStartData.h"

namespace content {

EmbeddedWorkerInstanceClientImpl::WorkerWrapper::WorkerWrapper(
    blink::WebEmbeddedWorker* worker,
    int devtools_agent_route_id)
    : worker_(worker),
      devtools_agent_(base::MakeUnique<EmbeddedWorkerDevToolsAgent>(
          worker,
          devtools_agent_route_id)) {}

EmbeddedWorkerInstanceClientImpl::WorkerWrapper::~WorkerWrapper() = default;

// static
void EmbeddedWorkerInstanceClientImpl::Create(
    const service_manager::BindSourceInfo& source_info,
    mojom::EmbeddedWorkerInstanceClientRequest request) {
  // This won't be leaked because the lifetime will be managed internally.
  new EmbeddedWorkerInstanceClientImpl(std::move(request));
}

void EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed() {
  DCHECK(wrapper_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed");

  wrapper_.reset();
}

void EmbeddedWorkerInstanceClientImpl::StartWorker(
    const EmbeddedWorkerStartParams& params,
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
    mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info) {
  DCHECK(ChildThreadImpl::current());
  DCHECK(!wrapper_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::StartWorker");

  wrapper_ = StartWorkerContext(
      params, base::MakeUnique<ServiceWorkerContextClient>(
                  params.embedded_worker_id, params.service_worker_version_id,
                  params.scope, params.script_url,
                  std::move(dispatcher_request), std::move(instance_host),
                  std::move(provider_info), std::move(temporal_self_)));
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
    mojo::InterfaceRequest<mojom::EmbeddedWorkerInstanceClient> request)
    : binding_(this, std::move(request)), temporal_self_(this) {
  binding_.set_connection_error_handler(base::Bind(
      &EmbeddedWorkerInstanceClientImpl::OnError, base::Unretained(this)));
}

EmbeddedWorkerInstanceClientImpl::~EmbeddedWorkerInstanceClientImpl() {}

void EmbeddedWorkerInstanceClientImpl::OnError() {
  // Removes myself if it's owned by myself.
  temporal_self_.reset();
}

std::unique_ptr<EmbeddedWorkerInstanceClientImpl::WorkerWrapper>
EmbeddedWorkerInstanceClientImpl::StartWorkerContext(
    const EmbeddedWorkerStartParams& params,
    std::unique_ptr<ServiceWorkerContextClient> context_client) {
  auto wrapper = base::MakeUnique<WorkerWrapper>(
      blink::WebEmbeddedWorker::Create(context_client.release(), nullptr),
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
