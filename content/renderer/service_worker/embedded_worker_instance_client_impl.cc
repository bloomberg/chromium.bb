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

// static
void EmbeddedWorkerInstanceClientImpl::Create(
    EmbeddedWorkerDispatcher* dispatcher,
    mojo::InterfaceRequest<mojom::EmbeddedWorkerInstanceClient> request) {
  // This won't be leaked because the lifetime will be managed internally.
  new EmbeddedWorkerInstanceClientImpl(dispatcher, std::move(request));
}

void EmbeddedWorkerInstanceClientImpl::StopWorkerCompleted() {
  DCHECK(embedded_worker_id_);
  DCHECK(stop_callback_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::StopWorkerCompleted");
  // TODO(falken): The signals to the browser should be in the order:
  // (1) WorkerStopped (via stop_callback_)
  // (2) ProviderDestroyed (via UnregisterWorker destroying
  //     WebEmbeddedWorkerImpl)
  // But this ordering is currently not guaranteed since the Mojo pipes are
  // different. https://crbug.com/676526
  stop_callback_.Run();
  stop_callback_.Reset();
  dispatcher_->UnregisterWorker(embedded_worker_id_.value());
  embedded_worker_id_.reset();
  wrapper_ = nullptr;
}

void EmbeddedWorkerInstanceClientImpl::StartWorker(
    const EmbeddedWorkerStartParams& params,
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request) {
  DCHECK(ChildThreadImpl::current());
  DCHECK(!wrapper_);
  DCHECK(!embedded_worker_id_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::StartWorker");
  embedded_worker_id_ = params.embedded_worker_id;

  std::unique_ptr<EmbeddedWorkerDispatcher::WorkerWrapper> wrapper =
      dispatcher_->StartWorkerContext(
          params,
          base::MakeUnique<ServiceWorkerContextClient>(
              params.embedded_worker_id, params.service_worker_version_id,
              params.scope, params.script_url,
              std::move(dispatcher_request), std::move(temporal_self_)));
  wrapper_ = wrapper.get();
  dispatcher_->RegisterWorker(params.embedded_worker_id, std::move(wrapper));
}

void EmbeddedWorkerInstanceClientImpl::StopWorker(
    const StopWorkerCallback& callback) {
  // StopWorker must be called after StartWorker is called.
  DCHECK(ChildThreadImpl::current());
  DCHECK(wrapper_);
  DCHECK(embedded_worker_id_);
  DCHECK(!stop_callback_);

  TRACE_EVENT0("ServiceWorker", "EmbeddedWorkerInstanceClientImpl::StopWorker");
  stop_callback_ = callback;
  dispatcher_->RecordStopWorkerTimer(embedded_worker_id_.value());
  wrapper_->worker()->terminateWorkerContext();
}

void EmbeddedWorkerInstanceClientImpl::ResumeAfterDownload() {
  DCHECK(wrapper_);
  DCHECK(wrapper_->worker());
  wrapper_->worker()->resumeAfterDownload();
}

void EmbeddedWorkerInstanceClientImpl::AddMessageToConsole(
    blink::WebConsoleMessage::Level level,
    const std::string& message) {
  DCHECK(wrapper_);
  DCHECK(wrapper_->worker());
  wrapper_->worker()->addMessageToConsole(
      blink::WebConsoleMessage(level, blink::WebString::fromUTF8(message)));
}

EmbeddedWorkerInstanceClientImpl::EmbeddedWorkerInstanceClientImpl(
    EmbeddedWorkerDispatcher* dispatcher,
    mojo::InterfaceRequest<mojom::EmbeddedWorkerInstanceClient> request)
    : dispatcher_(dispatcher),
      binding_(this, std::move(request)),
      temporal_self_(std::unique_ptr<EmbeddedWorkerInstanceClientImpl>(this)),
      wrapper_(nullptr) {
  binding_.set_connection_error_handler(base::Bind(
      &EmbeddedWorkerInstanceClientImpl::OnError, base::Unretained(this)));
}

EmbeddedWorkerInstanceClientImpl::~EmbeddedWorkerInstanceClientImpl() {}

void EmbeddedWorkerInstanceClientImpl::OnError() {
  // Removes myself if it's owned by myself.
  temporal_self_.reset();
}

}  // namespace content
