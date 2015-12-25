// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NAVIGATOR_CONNECT_SERVICE_PORT_PROVIDER_H_
#define CONTENT_CHILD_NAVIGATOR_CONNECT_SERVICE_PORT_PROVIDER_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/child/worker_thread_registry.h"
#include "content/common/service_port_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/navigator_services/WebServicePortProvider.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebServicePortProviderClient;
class WebString;
}

namespace IPC {
class Message;
}

namespace content {
class ServiceRegistry;
struct TransferredMessagePort;

// Main entry point for the navigator.services API in a child process. This
// implementes the blink API and passes calls on to the browser process, as
// well as receives events from the browser process to pass back on to blink.
// One instance of this class is created for each ServicePortCollection, the
// instance lives on the worker thread it is associated with, and is
// conceptually owned by the ServicePortCollection in blink. Refcounted because
// some operations might require asynchronous operations that require this class
// to potentially stick around for a bit longer after a ServicePortCollection is
// destroyed.
// Uses mojo to communicate with the browser process.
class ServicePortProvider
    : public blink::WebServicePortProvider,
      public ServicePortServiceClient,
      public base::RefCountedThreadSafe<ServicePortProvider> {
 public:
  ServicePortProvider(
      blink::WebServicePortProviderClient* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_loop);

  // WebServicePortProvider implementation.
  void destroy() override;
  void connect(const blink::WebURL& target_url,
               const blink::WebString& origin,
               blink::WebServicePortConnectCallbacks* callbacks) override;
  void postMessage(blink::WebServicePortID port_id,
                   const blink::WebString& message,
                   blink::WebMessagePortChannelArray* channels) override;
  void closePort(blink::WebServicePortID port_id) override;

  // ServicePortServiceClient implementation.
  void PostMessageToPort(int32_t port_id,
                         const mojo::String& message,
                         mojo::Array<MojoTransferredMessagePortPtr> ports,
                         mojo::Array<int32_t> new_routing_ids) override;

 private:
  ~ServicePortProvider() override;
  friend class base::RefCountedThreadSafe<ServicePortProvider>;

  void PostMessageToBrowser(int port_id,
                            const base::string16& message,
                            const std::vector<TransferredMessagePort> ports);

  void OnConnectResult(
      scoped_ptr<blink::WebServicePortConnectCallbacks> callbacks,
      ServicePortConnectResult result,
      int32_t port_id);

  blink::WebServicePortProviderClient* client_;

  // Helper method that returns an initialized ServicePortServicePtr.
  ServicePortServicePtr& GetServicePortServicePtr();
  mojo::Binding<ServicePortServiceClient> binding_;

  ServicePortServicePtr service_port_service_;

  scoped_refptr<base::SingleThreadTaskRunner> main_loop_;

  DISALLOW_COPY_AND_ASSIGN(ServicePortProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_NAVIGATOR_CONNECT_SERVICE_PORT_PROVIDER_H_
