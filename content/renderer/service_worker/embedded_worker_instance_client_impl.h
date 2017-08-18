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
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/web/worker_content_settings_proxy.mojom.h"

namespace blink {

class WebEmbeddedWorker;

}  // namespace blink

namespace service_manager {
struct BindSourceInfo;
}

namespace content {

class EmbeddedWorkerDevToolsAgent;
class ServiceWorkerContextClient;

// This class exposes interfaces of WebEmbeddedWorker to the browser process.
// Unless otherwise noted, all methods should be called on the main thread.
class EmbeddedWorkerInstanceClientImpl
    : public mojom::EmbeddedWorkerInstanceClient {
 public:
  static void Create(
      base::TimeTicks blink_initialized_time,
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner,
      mojom::EmbeddedWorkerInstanceClientRequest request,
      const service_manager::BindSourceInfo& source_info);

  ~EmbeddedWorkerInstanceClientImpl() override;

  // Called from ServiceWorkerContextClient.
  void WorkerContextDestroyed();
  EmbeddedWorkerDevToolsAgent* devtools_agent() {
    return wrapper_->devtools_agent();
  };

 private:
  // A thin wrapper of WebEmbeddedWorker which also adds and releases process
  // references automatically.
  class WorkerWrapper {
   public:
    WorkerWrapper(std::unique_ptr<blink::WebEmbeddedWorker> worker,
                  int devtools_agent_route_id);
    ~WorkerWrapper();

    blink::WebEmbeddedWorker* worker() { return worker_.get(); }
    EmbeddedWorkerDevToolsAgent* devtools_agent() {
      return devtools_agent_.get();
    }

   private:
    ScopedChildProcessReference process_ref_;
    std::unique_ptr<blink::WebEmbeddedWorker> worker_;
    std::unique_ptr<EmbeddedWorkerDevToolsAgent> devtools_agent_;
  };

  EmbeddedWorkerInstanceClientImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner,
      mojo::InterfaceRequest<mojom::EmbeddedWorkerInstanceClient> request);

  // mojom::EmbeddedWorkerInstanceClient implementation
  void StartWorker(
      const EmbeddedWorkerStartParams& params,
      mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
      mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
      blink::mojom::WorkerContentSettingsProxyPtr content_settings_proxy)
      override;
  void StopWorker() override;
  void ResumeAfterDownload() override;
  void AddMessageToConsole(blink::WebConsoleMessage::Level level,
                           const std::string& message) override;

  // Handler of connection error bound to |binding_|
  void OnError();

  std::unique_ptr<WorkerWrapper> StartWorkerContext(
      const EmbeddedWorkerStartParams& params,
      mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info,
      std::unique_ptr<ServiceWorkerContextClient> context_client,
      blink::mojom::WorkerContentSettingsProxyPtr content_settings_proxy);

  mojo::Binding<mojom::EmbeddedWorkerInstanceClient> binding_;

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
