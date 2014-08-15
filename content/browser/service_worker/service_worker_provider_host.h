// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/resource_type.h"

namespace IPC {
class Sender;
}

namespace webkit_blob {
class BlobStorageContext;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerDispatcherHost;
class ServiceWorkerRequestHandler;
class ServiceWorkerVersion;

// This class is the browser-process representation of a service worker
// provider. There is a provider per document and the lifetime of this
// object is tied to the lifetime of its document in the renderer process.
// This class holds service worker state that is scoped to an individual
// document.
//
// Note this class can also host a running service worker, in which
// case it will observe resource loads made directly by the service worker.
class CONTENT_EXPORT ServiceWorkerProviderHost
    : public ServiceWorkerRegistration::Listener,
      public base::SupportsWeakPtr<ServiceWorkerProviderHost> {
 public:
  ServiceWorkerProviderHost(int process_id,
                            int provider_id,
                            base::WeakPtr<ServiceWorkerContextCore> context,
                            ServiceWorkerDispatcherHost* dispatcher_host);
  virtual ~ServiceWorkerProviderHost();

  int process_id() const { return process_id_; }
  int provider_id() const { return provider_id_; }

  bool IsHostToRunningServiceWorker() {
    return running_hosted_version_ != NULL;
  }

  // Getters for the navigator.serviceWorker attribute values.
  ServiceWorkerVersion* controlling_version() const {
    return controlling_version_.get();
  }
  ServiceWorkerVersion* active_version() const {
    return active_version_.get();
  }
  ServiceWorkerVersion* waiting_version() const {
    return waiting_version_.get();
  }
  ServiceWorkerVersion* installing_version() const {
    return installing_version_.get();
  }

  // The running version, if any, that this provider is providing resource
  // loads for.
  ServiceWorkerVersion* running_hosted_version() const {
    return running_hosted_version_.get();
  }

  void SetDocumentUrl(const GURL& url);
  const GURL& document_url() const { return document_url_; }

  // Associates to |registration| to listen for its version change events.
  void AssociateRegistration(ServiceWorkerRegistration* registration);

  // Clears the associated registration and stop listening to it.
  void UnassociateRegistration();

  // Returns false if the version is not in the expected STARTING in our
  // process state. That would be indicative of a bad IPC message.
  bool SetHostedVersionId(int64 versions_id);

  // Returns a handler for a request, the handler may return NULL if
  // the request doesn't require special handling.
  scoped_ptr<ServiceWorkerRequestHandler> CreateRequestHandler(
      ResourceType resource_type,
      base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context);

  // Returns true if |registration| can be associated with this provider.
  bool CanAssociateRegistration(ServiceWorkerRegistration* registration);

  // Returns true if the context referred to by this host (i.e. |context_|) is
  // still alive.
  bool IsContextAlive();

  // Dispatches message event to the document.
  void PostMessage(const base::string16& message,
                   const std::vector<int>& sent_message_port_ids);

 private:
  friend class ServiceWorkerProviderHostTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTest,
                           UpdateBefore24Hours);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTest,
                           UpdateAfter24Hours);

  // ServiceWorkerRegistration::Listener overrides.
  virtual void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      ChangedVersionAttributesMask changed_mask,
      const ServiceWorkerRegistrationInfo& info) OVERRIDE;
  virtual void OnRegistrationFailed(
      ServiceWorkerRegistration* registration) OVERRIDE;

  // Sets the corresponding version field to the given version or if the given
  // version is NULL, clears the field.
  void SetVersionAttributes(
      ServiceWorkerVersion* installing_version,
      ServiceWorkerVersion* waiting_version,
      ServiceWorkerVersion* active_version);
  void SetVersionAttributesInternal(
      ServiceWorkerVersion* version,
      scoped_refptr<ServiceWorkerVersion>* data_member);

  // Sets the controller version field to |version| or if |version| is NULL,
  // clears the field.
  void SetControllerVersionAttribute(ServiceWorkerVersion* version);

  // Clears all version fields.
  void ClearVersionAttributes();

  // Creates a ServiceWorkerHandle to retain |version| and returns a
  // ServiceWorkerInfo with the handle ID to pass to the provider. The
  // provider is responsible for releasing the handle.
  ServiceWorkerObjectInfo CreateHandleAndPass(ServiceWorkerVersion* version);

  const int process_id_;
  const int provider_id_;
  GURL document_url_;

  scoped_refptr<ServiceWorkerRegistration> associated_registration_;

  scoped_refptr<ServiceWorkerVersion> controlling_version_;
  scoped_refptr<ServiceWorkerVersion> active_version_;
  scoped_refptr<ServiceWorkerVersion> waiting_version_;
  scoped_refptr<ServiceWorkerVersion> installing_version_;
  scoped_refptr<ServiceWorkerVersion> running_hosted_version_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ServiceWorkerDispatcherHost* dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
