// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_

#include "base/id_map.h"
#include "base/optional.h"
#include "content/child/child_thread_impl.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"

namespace content {

// This class exposes interfaces of WebEmbeddedWorker to the browser process.
// Unless otherwise noted, all methods should be called on the main thread.
class EmbeddedWorkerInstanceClientImpl
    : public mojom::EmbeddedWorkerInstanceClient {
 public:
  static void Create(
      EmbeddedWorkerDispatcher* dispatcher,
      mojo::InterfaceRequest<mojom::EmbeddedWorkerInstanceClient> request);

  ~EmbeddedWorkerInstanceClientImpl() override;

  // Called from ServiceWorkerContextClient.
  void StopWorkerCompleted();
  EmbeddedWorkerDevToolsAgent* devtools_agent() {
    return wrapper_->devtools_agent();
  };

 private:
  EmbeddedWorkerInstanceClientImpl(
      EmbeddedWorkerDispatcher* dispatcher,
      mojo::InterfaceRequest<mojom::EmbeddedWorkerInstanceClient> request);

  // mojom::EmbeddedWorkerInstanceClient implementation
  void StartWorker(
      const EmbeddedWorkerStartParams& params,
      mojom::ServiceWorkerEventDispatcherRequest dispatcher_request) override;
  void StopWorker(const StopWorkerCallback& callback) override;
  void ResumeAfterDownload() override;
  void AddMessageToConsole(blink::WebConsoleMessage::Level level,
                           const std::string& message) override;

  // Handler of connection error bound to |binding_|
  void OnError();

  EmbeddedWorkerDispatcher* dispatcher_;
  mojo::Binding<mojom::EmbeddedWorkerInstanceClient> binding_;

  // This is valid before StartWorker is called. After that, this object
  // will be passed to ServiceWorkerContextClient.
  std::unique_ptr<EmbeddedWorkerInstanceClientImpl> temporal_self_;

  base::Optional<int> embedded_worker_id_;

  // Owned by EmbeddedWorkerDispatcher. This can be nullptr while a worker is
  // not running.
  EmbeddedWorkerDispatcher::WorkerWrapper* wrapper_;

  StopWorkerCallback stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
