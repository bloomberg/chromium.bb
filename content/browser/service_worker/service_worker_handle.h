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

namespace url {
class Origin;
}  // namespace url

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerDispatcherHost;
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
  // Creates a newly created instance for a live version. |out_info| holds the
  // first ServiceWorkerObjectHost Mojo connection to this instance, which will
  // delete itself once it detects that all the Mojo connections have gone
  // away.
  //
  // This instance registers itself into |dispatcher_host| to be owned by the
  // dispatcher host. S13nServiceWorker: |dispatcher_host| may be null.
  // RegisterIntoDispatcherHost() should be called later to register the handle
  // once the host is known.
  static base::WeakPtr<ServiceWorkerHandle> Create(
      ServiceWorkerDispatcherHost* dispatcher_host,
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerVersion* version,
      blink::mojom::ServiceWorkerObjectInfoPtr* out_info);

  ~ServiceWorkerHandle() override;

  // ServiceWorkerVersion::Listener overrides.
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;

  // Establishes a new mojo connection into |bindings_|.
  blink::mojom::ServiceWorkerObjectInfoPtr CreateObjectInfo();

  // Should only be called on a ServiceWorkerHandle instance constructed with
  // null |dispatcher_host| before.
  void RegisterIntoDispatcherHost(ServiceWorkerDispatcherHost* dispatcher_host);

  int provider_id() const { return provider_id_; }
  int handle_id() const { return handle_id_; }
  ServiceWorkerVersion* version() { return version_.get(); }

 private:
  friend class service_worker_handle_unittest::ServiceWorkerHandleTest;

  ServiceWorkerHandle(ServiceWorkerDispatcherHost* dispatcher_host,
                      base::WeakPtr<ServiceWorkerContextCore> context,
                      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
                      ServiceWorkerVersion* version);

  // Implements blink::mojom::ServiceWorkerObjectHost.
  void PostMessage(::blink::TransferableMessage message,
                   const url::Origin& source_origin) override;
  void TerminateForTesting(TerminateForTestingCallback callback) override;

  void DispatchExtendableMessageEvent(
      ::blink::TransferableMessage message,
      const url::Origin& source_origin,
      base::OnceCallback<void(ServiceWorkerStatusCode)> callback);

  base::WeakPtr<ServiceWorkerHandle> AsWeakPtr();

  void OnConnectionError();

  // |dispatcher_host_| may get a valid value via ctor or
  // RegisterIntoDispatcherHost() function, after that |dispatcher_host_| starts
  // to own |this|, then, |dispatcher_host_| is valid throughout the lifetime of
  // |this|.
  ServiceWorkerDispatcherHost* dispatcher_host_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  const int provider_id_;
  const int handle_id_;
  scoped_refptr<ServiceWorkerVersion> version_;
  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerObjectHost> bindings_;

  base::WeakPtrFactory<ServiceWorkerHandle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_
