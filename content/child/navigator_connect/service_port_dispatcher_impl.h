// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NAVIGATOR_CONNECT_SERVICE_PORT_DISPATCHER_IMPL_H_
#define CONTENT_CHILD_NAVIGATOR_CONNECT_SERVICE_PORT_DISPATCHER_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_port_service.mojom.h"
#include "content/public/child/worker_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace blink {
class WebServiceWorkerContextProxy;
}

namespace content {

// Mojo service that dispatches ServicePort related events to a service worker.
// Instances are always created on a worker thread, and all methods are only
// called on that same thread. Lifetime of this class is tied to both the mojo
// channel and the lifetime of the worker thread. If either the channel is
// disconnected or the worker thread stops the instance deletes itself.
class ServicePortDispatcherImpl : public ServicePortDispatcher,
                                  public WorkerThread::Observer {
 public:
  static void Create(base::WeakPtr<blink::WebServiceWorkerContextProxy> proxy,
                     mojo::InterfaceRequest<ServicePortDispatcher> request);

  ~ServicePortDispatcherImpl() override;

 private:
  ServicePortDispatcherImpl(
      base::WeakPtr<blink::WebServiceWorkerContextProxy> proxy,
      mojo::InterfaceRequest<ServicePortDispatcher> request);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  // ServicePortDispatcher implementation.
  void Connect(const mojo::String& target_url,
               const mojo::String& origin,
               int32_t port_id,
               const ConnectCallback& callback) override;

  mojo::StrongBinding<ServicePortDispatcher> binding_;
  base::WeakPtr<blink::WebServiceWorkerContextProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(ServicePortDispatcherImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_NAVIGATOR_CONNECT_SERVICE_PORT_DISPATCHER_IMPL_H_
