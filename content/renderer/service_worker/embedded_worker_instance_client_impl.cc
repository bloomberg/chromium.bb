// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/common/content_client.h"
#include "content/renderer/service_worker/embedded_worker_devtools_agent.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorker.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorkerStartData.h"

namespace content {

namespace {

void OnError() {
  // TODO(shimazu): Implement here
  NOTIMPLEMENTED();
}

}  // namespace

// static
void EmbeddedWorkerInstanceClientImpl::Create(
    EmbeddedWorkerDispatcher* dispatcher,
    mojo::InterfaceRequest<mojom::EmbeddedWorkerInstanceClient> request) {
  auto binding = mojo::MakeStrongBinding(
      base::MakeUnique<EmbeddedWorkerInstanceClientImpl>(dispatcher),
      std::move(request));
  binding->set_connection_error_handler(base::Bind(&OnError));
}

void EmbeddedWorkerInstanceClientImpl::StartWorker(
    const EmbeddedWorkerStartParams& params) {
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::StartWorker");

  std::unique_ptr<EmbeddedWorkerDispatcher::WorkerWrapper> wrapper =
      dispatcher_->StartWorkerContext(
          params,
          base::MakeUnique<ServiceWorkerContextClient>(
              params.embedded_worker_id, params.service_worker_version_id,
              params.scope, params.script_url,
              params.worker_devtools_agent_route_id));
  wrapper_ = wrapper.get();
  dispatcher_->RegisterWorker(params.embedded_worker_id, std::move(wrapper));
}

EmbeddedWorkerInstanceClientImpl::EmbeddedWorkerInstanceClientImpl(
    EmbeddedWorkerDispatcher* dispatcher)
    : dispatcher_(dispatcher) {}

EmbeddedWorkerInstanceClientImpl::~EmbeddedWorkerInstanceClientImpl() {}

}  // namespace content
