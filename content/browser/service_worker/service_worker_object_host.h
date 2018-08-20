// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_OBJECT_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_OBJECT_HOST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;

namespace service_worker_object_host_unittest {
class ServiceWorkerObjectHostTest;
}  // namespace service_worker_object_host_unittest

// Roughly corresponds to one WebServiceWorker object in the renderer process.
//
// The WebServiceWorker object in the renderer process maintains a reference to
// |this| by owning an associated interface pointer to
// blink::mojom::ServiceWorkerObjectHost. When all Mojo connections bound with
// |bindings_| are disconnected, |this| will be deleted. See also comments on
// |bindings_|.
//
// Has references to the corresponding ServiceWorkerVersion in order to ensure
// that the version is alive while this handle is around.
class CONTENT_EXPORT ServiceWorkerObjectHost
    : public blink::mojom::ServiceWorkerObjectHost,
      public ServiceWorkerVersion::Observer {
 public:
  ServiceWorkerObjectHost(base::WeakPtr<ServiceWorkerContextCore> context,
                          ServiceWorkerProviderHost* provider_host,
                          scoped_refptr<ServiceWorkerVersion> version);
  ~ServiceWorkerObjectHost() override;

  // ServiceWorkerVersion::Observer overrides.
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;

  // Returns an info for the ServiceWorker object. The info contains a Mojo
  // ptr to |this| which ensures |this| stays alive while the info is alive.
  // Furthermore, it contains a Mojo request for the ServiceWorkerObject
  // interface in the renderer. |this| will make calls to the
  // ServiceWorkerObject to update its state.
  //
  // WARNING: The returned info must be sent immediately over Mojo, because
  // |this| will start making calls on an associated interface ptr to
  // ServiceWorkerObject, which crashes unless the request inside the info has
  // been sent. If the info cannot be sent immediately, use
  // CreateIncompleteObjectInfo() instead.
  blink::mojom::ServiceWorkerObjectInfoPtr CreateCompleteObjectInfoToSend();

  // Similar to CreateCompleteObjectInfoToSend(), except the returned info has
  // an empty Mojo request for ServiceWorkerObject. To make the info usable, the
  // caller should add a request to the info, send the info over Mojo, and
  // then call AddRemoteObjectPtrAndUpdateState().
  //
  // This function is useful when an info is needed before it can be sent over
  // Mojo.
  blink::mojom::ServiceWorkerObjectInfoPtr CreateIncompleteObjectInfo();

  // Starts to use the |remote_object_ptr_info| as a valid Mojo pipe endpoint,
  // and triggers statechanged event if |sent_state| is old and needs to be
  // updated.
  void AddRemoteObjectPtrAndUpdateState(
      blink::mojom::ServiceWorkerObjectAssociatedPtrInfo remote_object_ptr_info,
      blink::mojom::ServiceWorkerState sent_state);

  ServiceWorkerVersion* version() { return version_.get(); }

  base::WeakPtr<ServiceWorkerObjectHost> AsWeakPtr();

 private:
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;

  // Implements blink::mojom::ServiceWorkerObjectHost.
  void PostMessageToServiceWorker(
      ::blink::TransferableMessage message) override;
  void TerminateForTesting(TerminateForTestingCallback callback) override;

  // TODO(leonhsl): Remove |callback| parameter because it's just for unit tests
  // and production code does not use it. We need to figure out another way to
  // observe the dispatch result in unit tests.
  void DispatchExtendableMessageEvent(
      ::blink::TransferableMessage message,
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback);

  void OnConnectionError();

  base::WeakPtr<ServiceWorkerContextCore> context_;
  // |provider_host_| is valid throughout lifetime of |this| because it owns
  // |this|.
  ServiceWorkerProviderHost* const provider_host_;
  // The origin of the |provider_host_|. Note that this is const because once a
  // JavaScript ServiceWorker object is created for an execution context, we
  // don't expect that context to change origins and still hold on to the
  // object.
  const url::Origin provider_origin_;
  scoped_refptr<ServiceWorkerVersion> version_;
  // Typically both |bindings_| and |remote_objects_| contain only one Mojo
  // connection, corresponding to the content::WebServiceWorkerImpl in the
  // renderer which corresponds to the ServiceWorker JavaScript object. However,
  // multiple Mojo connections may exist while propagating multiple service
  // worker object infos to the renderer process, but only the first one that
  // arrived there will be used to create the new content::WebServiceWorkerImpl
  // instance and be bound to it.
  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerObjectHost> bindings_;
  mojo::AssociatedInterfacePtrSet<blink::mojom::ServiceWorkerObject>
      remote_objects_;

  base::WeakPtrFactory<ServiceWorkerObjectHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerObjectHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_OBJECT_HOST_H_
