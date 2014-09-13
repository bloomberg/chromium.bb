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

namespace storage {
class BlobStorageContext;
}

namespace content {

class ResourceRequestBody;
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
    : public NON_EXPORTED_BASE(ServiceWorkerRegistration::Listener),
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
    return running_hosted_version_.get() != NULL;
  }

  ServiceWorkerVersion* controlling_version() const {
    return controlling_version_.get();
  }
  ServiceWorkerVersion* active_version() const {
    return associated_registration_.get() ?
        associated_registration_->active_version() : NULL;
  }
  ServiceWorkerVersion* waiting_version() const {
    return associated_registration_.get() ?
        associated_registration_->waiting_version() : NULL;
  }
  ServiceWorkerVersion* installing_version() const {
    return associated_registration_.get() ?
        associated_registration_->installing_version() : NULL;
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
  void DisassociateRegistration();

  // Returns false if the version is not in the expected STARTING in our
  // process state. That would be indicative of a bad IPC message.
  bool SetHostedVersionId(int64 versions_id);

  // Returns a handler for a request, the handler may return NULL if
  // the request doesn't require special handling.
  scoped_ptr<ServiceWorkerRequestHandler> CreateRequestHandler(
      ResourceType resource_type,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      scoped_refptr<ResourceRequestBody> body);

  // Returns true if |registration| can be associated with this provider.
  bool CanAssociateRegistration(ServiceWorkerRegistration* registration);

  // For use by the ServiceWorkerControlleeRequestHandler to disallow
  // new registration association while a navigation is occurring and
  // an existing registration is being looked for.
  void SetAllowAssociation(bool allow) { allow_association_ = allow; }

  // Returns true if the context referred to by this host (i.e. |context_|) is
  // still alive.
  bool IsContextAlive();

  // Dispatches message event to the document.
  void PostMessage(const base::string16& message,
                   const std::vector<int>& sent_message_port_ids);

  // Adds reference of this host's process to the |pattern|, the reference will
  // be removed in destructor.
  void AddScopedProcessReferenceToPattern(const GURL& pattern);

 private:
  friend class ServiceWorkerProviderHostTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTest,
                           UpdateBefore24Hours);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTest,
                           UpdateAfter24Hours);

  // ServiceWorkerRegistration::Listener overrides.
  virtual void OnRegistrationFailed(
      ServiceWorkerRegistration* registration) OVERRIDE;

  // Sets the controller version field to |version| or if |version| is NULL,
  // clears the field.
  void SetControllerVersionAttribute(ServiceWorkerVersion* version);

  // Creates a ServiceWorkerHandle to retain |version| and returns a
  // ServiceWorkerInfo with the handle ID to pass to the provider. The
  // provider is responsible for releasing the handle.
  ServiceWorkerObjectInfo CreateHandleAndPass(ServiceWorkerVersion* version);

  // Increase/decrease this host's process reference for |pattern|.
  void IncreaseProcessReference(const GURL& pattern);
  void DecreaseProcessReference(const GURL& pattern);

  const int process_id_;
  const int provider_id_;
  GURL document_url_;

  std::vector<GURL> associated_patterns_;
  scoped_refptr<ServiceWorkerRegistration> associated_registration_;

  scoped_refptr<ServiceWorkerVersion> controlling_version_;
  scoped_refptr<ServiceWorkerVersion> running_hosted_version_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ServiceWorkerDispatcherHost* dispatcher_host_;
  bool allow_association_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
