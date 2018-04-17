// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/platform/modules/serviceworker/web_service_worker.h"
#include "third_party/blink/public/web/web_frame.h"

namespace blink {
class WebServiceWorkerProxy;
}

namespace content {

// WebServiceWorkerImpl represents a ServiceWorker object in JavaScript.
// https://w3c.github.io/ServiceWorker/#serviceworker-interface
//
// Only one WebServiceWorkerImpl can exist at a time to represent a given
// service worker in a given execution context. This is because the standard
// requires JavaScript equality between ServiceWorker objects in the same
// execution context that represent the same service worker.
//
// This class is ref counted and owned by WebServiceWorker::Handle, which is
// passed to Blink. Generally, Blink keeps only one Handle to an instance of
// this class. However, since //content can't know if Blink already has a
// Handle, it passes a new Handle whenever passing this class to Blink. Blink
// discards the new Handle if it already has one.
//
// When a blink::mojom::ServiceWorkerObjectInfo arrives at the renderer, there
// are two ways to handle it.
// 1) If there is no WebServiceWorkerImpl which represents the ServiceWorker, a
// new WebServiceWorkerImpl is created using the
// blink::mojom::ServiceWorkerObjectInfo.
// 2) If there is a WebServiceWorkerImpl which represents the ServiceWorker, the
// WebServiceWorkerImpl starts to use the new browser->renderer connection
// (blink::mojom::ServiceWorkerObject interface) and the information about the
// service worker.
//
// WebServiceWorkerImpl holds a Mojo connection (|host_|). The connection keeps
// the ServiceWorkerHandle in the browser process alive, which in turn keeps the
// relevant ServiceWorkerVersion alive.
class CONTENT_EXPORT WebServiceWorkerImpl
    : public blink::mojom::ServiceWorkerObject,
      public blink::WebServiceWorker,
      public base::RefCounted<WebServiceWorkerImpl> {
 public:
  explicit WebServiceWorkerImpl(blink::mojom::ServiceWorkerObjectInfoPtr info);

  // Closes the existing binding and binds to |request| instead. Called when the
  // browser process sends a new blink::mojom::ServiceWorkerObjectInfo and
  // |this| already exists for the described ServiceWorker (see the class
  // comment).
  void RefreshConnection(
      blink::mojom::ServiceWorkerObjectAssociatedRequest request);

  // Implements blink::mojom::ServiceWorkerObject.
  void StateChanged(blink::mojom::ServiceWorkerState new_state) override;

  // blink::WebServiceWorker overrides.
  void SetProxy(blink::WebServiceWorkerProxy* proxy) override;
  blink::WebServiceWorkerProxy* Proxy() override;
  blink::WebURL Url() const override;
  blink::mojom::ServiceWorkerState GetState() const override;
  void PostMessageToServiceWorker(blink::TransferableMessage message) override;
  void TerminateForTesting(
      std::unique_ptr<TerminateForTestingCallback> callback) override;

  // Creates WebServiceWorker::Handle object that owns a reference to the given
  // WebServiceWorkerImpl object.
  static std::unique_ptr<blink::WebServiceWorker::Handle> CreateHandle(
      scoped_refptr<WebServiceWorkerImpl> worker);

 private:
  friend class base::RefCounted<WebServiceWorkerImpl>;
  ~WebServiceWorkerImpl() override;

  // Both |host_| and |binding_| are bound on the main
  // thread for service worker clients (document), and are bound on the service
  // worker thread for service worker execution contexts.
  //
  // |host_| keeps the Mojo connection to the
  // browser-side ServiceWorkerHandle, whose lifetime is bound
  // to |host_| via the Mojo connection.
  blink::mojom::ServiceWorkerObjectHostAssociatedPtr host_;
  // |binding_| keeps the Mojo binding to serve its other Mojo endpoint (i.e.
  // the caller end) held by the content::ServiceWorkerHandle in the browser
  // process.
  mojo::AssociatedBinding<blink::mojom::ServiceWorkerObject> binding_;

  blink::mojom::ServiceWorkerObjectInfoPtr info_;
  blink::mojom::ServiceWorkerState state_;
  blink::WebServiceWorkerProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
