// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"

#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerEventResult.h"

namespace blink {
class WebServiceWorkerContextProxy;
}

namespace IPC {
class Message;
}

namespace content {

class EmbeddedWorkerContextClient;

// TODO(kinuko): This should implement WebServiceWorkerContextClient
// rather than having EmbeddedWorkerContextClient implement it.
// See the header comment in embedded_worker_context_client.h for the
// potential EW/SW layering concerns.
class ServiceWorkerScriptContext {
 public:
  ServiceWorkerScriptContext(
      EmbeddedWorkerContextClient* embedded_context,
      blink::WebServiceWorkerContextProxy* proxy);
  ~ServiceWorkerScriptContext();

  void OnMessageReceived(int request_id, const IPC::Message& message);

  void DidHandleActivateEvent(int request_id,
                              blink::WebServiceWorkerEventResult);
  void DidHandleInstallEvent(int request_id,
                             blink::WebServiceWorkerEventResult result);
  void DidHandleFetchEvent(int request_id,
                           ServiceWorkerFetchEventResult result,
                           const ServiceWorkerResponse& response);
  void DidHandleSyncEvent(int request_id);

 private:
  // Send a message to the browser.
  void Send(IPC::Message* message);

  // Send a reply message (for |request_id|) back to the browser.
  // TODO(kinuko): Deprecate this.
  void Reply(int request_id, const IPC::Message& message);

  void OnActivateEvent();
  void OnInstallEvent(int active_version_id);
  void OnFetchEvent(const ServiceWorkerFetchRequest& request);
  void OnPostMessage(const base::string16& message,
                     const std::vector<int>& sent_message_port_ids,
                     const std::vector<int>& new_routing_ids);
  void OnSyncEvent();

  // Not owned; embedded_context_ owns this.
  EmbeddedWorkerContextClient* embedded_context_;

  // Not owned; this object is destroyed when proxy_ becomes invalid.
  blink::WebServiceWorkerContextProxy* proxy_;

  // Used for incoming messages from the browser for which an outgoing response
  // back to the browser is expected, the id must be sent back with the
  // response.
  int current_request_id_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptContext);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_H_
