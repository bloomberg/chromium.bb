// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_object.mojom.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;

namespace service_worker_handle_unittest {
class ServiceWorkerHandleTest;
}  // namespace service_worker_handle_unittest

// Roughly corresponds to one WebServiceWorker object in the renderer process.
//
// The WebServiceWorker object in the renderer process maintains a reference to
// |this| by owning an associated interface pointer to
// blink::mojom::ServiceWorkerObjectHost.
//
// Has references to the corresponding ServiceWorkerVersion in order to ensure
// that the version is alive while this handle is around.
class CONTENT_EXPORT ServiceWorkerHandle
    : public blink::mojom::ServiceWorkerObjectHost,
      public ServiceWorkerVersion::Listener {
 public:
  ServiceWorkerHandle(base::WeakPtr<ServiceWorkerContextCore> context,
                      ServiceWorkerProviderHost* provider_host,
                      scoped_refptr<ServiceWorkerVersion> version);
  ~ServiceWorkerHandle() override;

  // ServiceWorkerVersion::Listener overrides.
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;

  // Establishes a new mojo connection into |bindings_|.
  blink::mojom::ServiceWorkerObjectInfoPtr CreateObjectInfo();

  int provider_id() const { return provider_id_; }
  int handle_id() const { return handle_id_; }
  ServiceWorkerVersion* version() { return version_.get(); }

 private:
  friend class service_worker_handle_unittest::ServiceWorkerHandleTest;

  // Implements blink::mojom::ServiceWorkerObjectHost.
  void PostMessageToServiceWorker(
      ::blink::TransferableMessage message) override;
  void TerminateForTesting(TerminateForTestingCallback callback) override;

  // TODO(leonhsl): Remove |callback| parameter because it's just for unit tests
  // and production code does not use it. We need to figure out another way to
  // observe the dispatch result in unit tests.
  void DispatchExtendableMessageEvent(
      ::blink::TransferableMessage message,
      base::OnceCallback<void(ServiceWorkerStatusCode)> callback);

  base::WeakPtr<ServiceWorkerHandle> AsWeakPtr();

  void OnConnectionError();

  base::WeakPtr<ServiceWorkerContextCore> context_;
  // |provider_host_| is valid throughout lifetime of |this| because it owns
  // |this|.
  ServiceWorkerProviderHost* provider_host_;
  // The origin of the |provider_host_|. Note that this is const because once a
  // JavaScript ServiceWorker object is created for an execution context, we
  // don't expect that context to change origins and still hold on to the
  // object.
  const url::Origin provider_origin_;
  const int provider_id_;
  const int handle_id_;
  scoped_refptr<ServiceWorkerVersion> version_;
  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerObjectHost> bindings_;

  base::WeakPtrFactory<ServiceWorkerHandle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_
