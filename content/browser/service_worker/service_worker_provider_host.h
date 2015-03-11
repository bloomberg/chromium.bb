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
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
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
  using GetClientInfoCallback =
      base::Callback<void(const ServiceWorkerClientInfo&)>;
  using GetRegistrationForReadyCallback =
      base::Callback<void(ServiceWorkerRegistration* reigstration)>;

  // If |render_frame_id| is MSG_ROUTING_NONE, this provider host works for the
  // worker context, i.e. ServiceWorker or SharedWorker.
  // |provider_type| gives additional information whether the provider is
  // created for controller (ServiceWorker) or controllee (Document or
  // SharedWorker).
  ServiceWorkerProviderHost(int render_process_id,
                            int render_frame_id,
                            int provider_id,
                            ServiceWorkerProviderType provider_type,
                            base::WeakPtr<ServiceWorkerContextCore> context,
                            ServiceWorkerDispatcherHost* dispatcher_host);
  virtual ~ServiceWorkerProviderHost();

  int process_id() const { return render_process_id_; }
  int provider_id() const { return provider_id_; }
  int frame_id() const { return render_frame_id_; }

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

  ServiceWorkerRegistration* associated_registration() const {
    return associated_registration_.get();
  }

  // The running version, if any, that this provider is providing resource
  // loads for.
  ServiceWorkerVersion* running_hosted_version() const {
    return running_hosted_version_.get();
  }

  void SetDocumentUrl(const GURL& url);
  const GURL& document_url() const { return document_url_; }

  void SetTopmostFrameUrl(const GURL& url);
  const GURL& topmost_frame_url() const { return topmost_frame_url_; }

  ServiceWorkerProviderType provider_type() const { return provider_type_; }

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
      FetchRequestMode request_mode,
      FetchCredentialsMode credentials_mode,
      ResourceType resource_type,
      RequestContextType request_context_type,
      RequestContextFrameType frame_type,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      scoped_refptr<ResourceRequestBody> body);

  // Creates a ServiceWorkerHandle to retain |version| and returns a
  // ServiceWorkerInfo with a newly created handle ID. The handle is held in
  // the dispatcher host until its ref-count becomes zero.
  ServiceWorkerObjectInfo CreateAndRegisterServiceWorkerHandle(
      ServiceWorkerVersion* version);

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
  void PostMessage(
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports);

  // Activates the WebContents associated with
  // { render_process_id_, render_frame_id_ }.
  // Runs the |callback| with the updated ServiceWorkerClientInfo in parameter.
  void Focus(const GetClientInfoCallback& callback);

  // Asks the renderer to send back the document information.
  void GetClientInfo(const GetClientInfoCallback& callback) const;

  // Same as above but has to be called from the UI thread.
  // It is taking the process and frame ids in parameter because |this| is meant
  // to live on the IO thread.
  static ServiceWorkerClientInfo GetClientInfoOnUI(int render_process_id,
                                                   int render_frame_id);

  // Adds reference of this host's process to the |pattern|, the reference will
  // be removed in destructor.
  void AddScopedProcessReferenceToPattern(const GURL& pattern);

  // |registration| claims the document to be controlled.
  void ClaimedByRegistration(ServiceWorkerRegistration* registration);

  // Called by dispatcher host to get the registration for the "ready" property.
  // Returns false if there's a completed or ongoing request for the document.
  // https://slightlyoff.github.io/ServiceWorker/spec/service_worker/#navigator-service-worker-ready
  bool GetRegistrationForReady(const GetRegistrationForReadyCallback& callback);

  // Methods to support cross site navigations.
  void PrepareForCrossSiteTransfer();
  void CompleteCrossSiteTransfer(
      int new_process_id,
      int new_frame_id,
      int new_provider_id,
      ServiceWorkerProviderType new_provider_type,
      ServiceWorkerDispatcherHost* dispatcher_host);
  ServiceWorkerDispatcherHost* dispatcher_host() const {
    return dispatcher_host_;
  }

  // Sends event messages to the renderer. Events for the worker are queued up
  // until the worker thread id is known via SetReadyToSendMessagesToWorker().
  void SendUpdateFoundMessage(
      int registration_handle_id);
  void SendSetVersionAttributesMessage(
      int registration_handle_id,
      ChangedVersionAttributesMask changed_mask,
      ServiceWorkerVersion* installing_version,
      ServiceWorkerVersion* waiting_version,
      ServiceWorkerVersion* active_version);
  void SendServiceWorkerStateChangedMessage(
      int worker_handle_id,
      blink::WebServiceWorkerState state);

  // Sets the worker thread id and flushes queued events.
  void SetReadyToSendMessagesToWorker(int render_thread_id);

  void AddMatchingRegistration(ServiceWorkerRegistration* registration);
  void RemoveMatchingRegistration(ServiceWorkerRegistration* registration);

  // An optimized implementation of [[Match Service Worker Registration]]
  // for current document.
  ServiceWorkerRegistration* MatchRegistration() const;

 private:
  friend class ServiceWorkerProviderHostTest;
  friend class ServiceWorkerWriteToCacheJobTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTest,
                           UpdateBefore24Hours);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTest,
                           UpdateAfter24Hours);

  struct OneShotGetReadyCallback {
    GetRegistrationForReadyCallback callback;
    bool called;

    explicit OneShotGetReadyCallback(
        const GetRegistrationForReadyCallback& callback);
    ~OneShotGetReadyCallback();
  };

  // ServiceWorkerRegistration::Listener overrides.
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      ChangedVersionAttributesMask changed_mask,
      const ServiceWorkerRegistrationInfo& info) override;
  void OnRegistrationFailed(ServiceWorkerRegistration* registration) override;
  void OnRegistrationFinishedUninstalling(
      ServiceWorkerRegistration* registration) override;
  void OnSkippedWaiting(ServiceWorkerRegistration* registration) override;

  // Sets the controller version field to |version| or if |version| is NULL,
  // clears the field.
  void SetControllerVersionAttribute(ServiceWorkerVersion* version);

  void SendAssociateRegistrationMessage();

  // Increase/decrease this host's process reference for |pattern|.
  void IncreaseProcessReference(const GURL& pattern);
  void DecreaseProcessReference(const GURL& pattern);

  void ReturnRegistrationForReadyIfNeeded();

  bool IsReadyToSendMessages() const;
  void Send(IPC::Message* message) const;

  int render_process_id_;
  int render_frame_id_;
  int render_thread_id_;
  int provider_id_;
  ServiceWorkerProviderType provider_type_;
  GURL document_url_;
  GURL topmost_frame_url_;

  std::vector<GURL> associated_patterns_;
  scoped_refptr<ServiceWorkerRegistration> associated_registration_;

  // Keyed by registration scope URL length.
  typedef std::map<size_t, scoped_refptr<ServiceWorkerRegistration>>
      ServiceWorkerRegistrationMap;
  // Contains all living registrations which has pattern this document's
  // URL starts with.
  ServiceWorkerRegistrationMap matching_registrations_;

  scoped_ptr<OneShotGetReadyCallback> get_ready_callback_;
  scoped_refptr<ServiceWorkerVersion> controlling_version_;
  scoped_refptr<ServiceWorkerVersion> running_hosted_version_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ServiceWorkerDispatcherHost* dispatcher_host_;
  bool allow_association_;
  bool is_claiming_;

  std::vector<base::Closure> queued_events_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
