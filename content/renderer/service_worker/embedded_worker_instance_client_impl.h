// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_

#include <memory>

#include "base/containers/id_map.h"
#include "content/child/child_thread_impl.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom.h"
#include "third_party/WebKit/public/web/worker_content_settings_proxy.mojom.h"

namespace blink {

class WebEmbeddedWorker;

}  // namespace blink

namespace content {

class ServiceWorkerContextClient;

// This class exposes interfaces of WebEmbeddedWorker to the browser process.
// Unless otherwise noted, all methods should be called on the main thread.
// EmbeddedWorkerInstanceClientImpl is created in order to start a service
// worker, and lives as long as the service worker is running.
//
// This class deletes itself when the worker stops (or if start failed). The
// ownership graph is a cycle like this:
// EmbeddedWorkerInstanceClientImpl -(owns)-> WorkerWrapper -(owns)->
// WebEmbeddedWorkerImpl -(owns)-> ServiceWorkerContextClient -(owns)->
// EmbeddedWorkerInstanceClientImpl. Therefore, an instance can delete itself by
// releasing its WorkerWrapper.
//
// Since starting/stopping service workers is initiated by the browser process,
// the browser process effectively controls the lifetime of this class.
//
// TODO(shimazu): Let EmbeddedWorkerInstanceClientImpl own itself instead of
// the big reference cycle.
class EmbeddedWorkerInstanceClientImpl
    : public mojom::EmbeddedWorkerInstanceClient {
 public:
  // Creates a new EmbeddedWorkerInstanceClientImpl instance bound to
  // |request|. The instance destroys itself when needed, see the class
  // documentation.
  static void Create(
      base::TimeTicks blink_initialized_time,
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner,
      mojom::EmbeddedWorkerInstanceClientAssociatedRequest request);

  ~EmbeddedWorkerInstanceClientImpl() override;

  // Called from ServiceWorkerContextClient.
  void WorkerContextDestroyed();

 private:
  // A thin wrapper of WebEmbeddedWorker which also adds and releases process
  // references automatically.
  class WorkerWrapper {
   public:
    explicit WorkerWrapper(std::unique_ptr<blink::WebEmbeddedWorker> worker);
    ~WorkerWrapper();

    blink::WebEmbeddedWorker* worker() { return worker_.get(); }

   private:
    ScopedChildProcessReference process_ref_;
    std::unique_ptr<blink::WebEmbeddedWorker> worker_;
  };

  EmbeddedWorkerInstanceClientImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner,
      mojom::EmbeddedWorkerInstanceClientAssociatedRequest request);

  // mojom::EmbeddedWorkerInstanceClient implementation
  void StartWorker(mojom::EmbeddedWorkerStartParamsPtr params) override;
  void StopWorker() override;
  void ResumeAfterDownload() override;
  void AddMessageToConsole(blink::WebConsoleMessage::Level level,
                           const std::string& message) override;
  void BindDevToolsAgent(
      blink::mojom::DevToolsAgentAssociatedRequest request) override;

  // Handler of connection error bound to |binding_|.
  void OnError();

  std::unique_ptr<WorkerWrapper> StartWorkerContext(
      mojom::EmbeddedWorkerStartParamsPtr params,
      std::unique_ptr<ServiceWorkerContextClient> context_client,
      service_manager::mojom::InterfaceProviderPtr interface_provider);

  mojo::AssociatedBinding<mojom::EmbeddedWorkerInstanceClient> binding_;

  // This is valid before StartWorker is called. After that, this object
  // will be passed to ServiceWorkerContextClient.
  std::unique_ptr<EmbeddedWorkerInstanceClientImpl> temporal_self_;

  // nullptr means the worker is not running.
  std::unique_ptr<WorkerWrapper> wrapper_;

  // For UMA.
  base::TimeTicks blink_initialized_time_;

  scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
