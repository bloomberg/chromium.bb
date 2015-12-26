// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/navigator_connect/service_port_dispatcher_impl.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"

namespace content {

namespace {

class WebConnectCallbacksImpl
    : public blink::WebServicePortConnectEventCallbacks {
 public:
  WebConnectCallbacksImpl(
      const ServicePortDispatcher::ConnectCallback& callback)
      : callback_(callback) {}

  ~WebConnectCallbacksImpl() override {}

  void onSuccess(const blink::WebServicePort& port) override {
    callback_.Run(SERVICE_PORT_CONNECT_RESULT_ACCEPT,
                  mojo::String::From<base::string16>(port.name),
                  mojo::String::From<base::string16>(port.data));
  }

  void onError() override {
    callback_.Run(SERVICE_PORT_CONNECT_RESULT_REJECT, mojo::String(""),
                  mojo::String(""));
  }

 private:
  ServicePortDispatcher::ConnectCallback callback_;
};

}  // namespace

void ServicePortDispatcherImpl::Create(
    base::WeakPtr<blink::WebServiceWorkerContextProxy> proxy,
    mojo::InterfaceRequest<ServicePortDispatcher> request) {
  new ServicePortDispatcherImpl(proxy, std::move(request));
}

ServicePortDispatcherImpl::~ServicePortDispatcherImpl() {
  WorkerThread::RemoveObserver(this);
}

ServicePortDispatcherImpl::ServicePortDispatcherImpl(
    base::WeakPtr<blink::WebServiceWorkerContextProxy> proxy,
    mojo::InterfaceRequest<ServicePortDispatcher> request)
    : binding_(this, std::move(request)), proxy_(proxy) {
  WorkerThread::AddObserver(this);
}

void ServicePortDispatcherImpl::WillStopCurrentWorkerThread() {
  delete this;
}

void ServicePortDispatcherImpl::Connect(const mojo::String& target_url,
                                        const mojo::String& origin,
                                        int32_t port_id,
                                        const ConnectCallback& callback) {
  if (!proxy_) {
    callback.Run(SERVICE_PORT_CONNECT_RESULT_REJECT, mojo::String(""),
                 mojo::String(""));
    return;
  }
  TRACE_EVENT0("ServiceWorker", "ServicePortDispatcherImpl::Connect");
  proxy_->dispatchServicePortConnectEvent(new WebConnectCallbacksImpl(callback),
                                          target_url.To<GURL>(),
                                          origin.To<base::string16>(), port_id);
}

}  // namespace content
