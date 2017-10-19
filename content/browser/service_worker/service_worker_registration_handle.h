// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerDispatcherHost;
class ServiceWorkerVersion;

// TODO(leonhsl): Merge this class into ServiceWorkerRegistration.

// ServiceWorkerRegistrationHandle has a 1:1 correspondence to
// WebServiceWorkerRegistration in the renderer process.
// WebServiceWorkerRegistration owns ServiceWorkerRegistrationHandle via an
// associated interface pointer to
// blink::mojom::ServiceWorkerRegistrationObjectHost.
//
// Has a reference to the corresponding ServiceWorkerRegistration in order to
// ensure that the registration is alive while this handle is around.
class CONTENT_EXPORT ServiceWorkerRegistrationHandle
    : public blink::mojom::ServiceWorkerRegistrationObjectHost,
      public ServiceWorkerRegistration::Listener {
 public:
  ServiceWorkerRegistrationHandle(
      base::WeakPtr<ServiceWorkerContextCore> context,
      ServiceWorkerDispatcherHost* dispatcher_host,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerRegistration* registration);
  ~ServiceWorkerRegistrationHandle() override;

  // Establishes a new mojo connection into |bindings_|.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr CreateObjectInfo();

  int provider_id() const { return provider_id_; }
  int handle_id() const { return handle_id_; }

  ServiceWorkerRegistration* registration() { return registration_.get(); }

 private:
  // ServiceWorkerRegistration::Listener overrides.
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      ChangedVersionAttributesMask changed_mask,
      const ServiceWorkerRegistrationInfo& info) override;
  void OnRegistrationFailed(ServiceWorkerRegistration* registration) override;
  void OnUpdateFound(ServiceWorkerRegistration* registration) override;

  // Implements blink::mojom::ServiceWorkerRegistrationObjectHost.
  void Update(UpdateCallback callback) override;

  // Called back from ServiceWorkerContextCore when an update is complete.
  void UpdateComplete(UpdateCallback callback,
                      ServiceWorkerStatusCode status,
                      const std::string& status_message,
                      int64_t registration_id);

  // Sets the corresponding version field to the given version or if the given
  // version is nullptr, clears the field.
  void SetVersionAttributes(
      ChangedVersionAttributesMask changed_mask,
      ServiceWorkerVersion* installing_version,
      ServiceWorkerVersion* waiting_version,
      ServiceWorkerVersion* active_version);

  void OnConnectionError();

  // |dispatcher_host_| is valid throughout lifetime of |this| because it owns
  // |this|.
  ServiceWorkerDispatcherHost* dispatcher_host_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  const int provider_id_;
  const int handle_id_;
  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerRegistrationObjectHost>
      bindings_;
  // Mojo connection to the content::WebServiceWorkerRegistrationImpl in the
  // renderer, which corresponds to the ServiceWorkerRegistration JavaScript
  // object.
  blink::mojom::ServiceWorkerRegistrationObjectAssociatedPtr
      remote_registration_;

  // This handle is the primary owner of this registration.
  scoped_refptr<ServiceWorkerRegistration> registration_;

  base::WeakPtrFactory<ServiceWorkerRegistrationHandle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_H_
