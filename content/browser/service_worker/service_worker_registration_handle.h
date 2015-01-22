// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerDispatcherHost;
class ServiceWorkerVersion;

// Roughly Corresponds to one ServiceWorkerRegistration object in the renderer
// process (WebServiceWorkerRegistrationImpl).
// Has references to the corresponding ServiceWorkerRegistration and
// ServiceWorkerVersions (therefore they're guaranteed to be alive while this
// handle is around).
class ServiceWorkerRegistrationHandle
    : public ServiceWorkerRegistration::Listener {
 public:
  CONTENT_EXPORT ServiceWorkerRegistrationHandle(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerRegistration* registration);
  virtual ~ServiceWorkerRegistrationHandle();

  ServiceWorkerRegistrationObjectInfo GetObjectInfo();

  bool HasNoRefCount() const { return ref_count_ <= 0; }
  void IncrementRefCount();
  void DecrementRefCount();

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

  // Sets the corresponding version field to the given version or if the given
  // version is nullptr, clears the field.
  void SetVersionAttributes(
      ChangedVersionAttributesMask changed_mask,
      ServiceWorkerVersion* installing_version,
      ServiceWorkerVersion* waiting_version,
      ServiceWorkerVersion* active_version);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  const int provider_id_;
  const int handle_id_;
  int ref_count_;  // Created with 1.

  scoped_refptr<ServiceWorkerRegistration> registration_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_H_
