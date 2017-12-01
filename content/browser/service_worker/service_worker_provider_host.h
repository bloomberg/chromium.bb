// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_provider_host_info.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/service_worker_modes.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace blink {
class MessagePortChannel;
}

namespace storage {
class BlobStorageContext;
}

namespace content {

class ResourceRequestBody;
class ServiceWorkerContextCore;
class ServiceWorkerDispatcherHost;
class ServiceWorkerRegistrationObjectHost;
class ServiceWorkerRequestHandler;
class ServiceWorkerVersion;
class WebContents;

// This class is the browser-process representation of a service worker
// provider. There are two general types of providers: 1) those for a client
// (windows or shared workers), and 2) those for hosting a running service
// worker.
//
// For client providers, there is a provider per document or a worker and the
// lifetime of this object is tied to the lifetime of its document or the worker
// in the renderer process. This class holds service worker state that is scoped
// to an individual document or a worker.
//
// For providers hosting a running service worker, this class will observe
// resource loads made directly by the service worker.
//
// A ServiceWorkerProviderHost is created in the following situations:
// 1) When it's for a document or worker (i.e., a service
// worker client), the provider host is created when
// ServiceWorkerNetworkProvider is created on the renderer process. Mojo's
// connection from ServiceWorkerNetworkProvider is established on the creation
// time.
// 2) When it's for a running service worker, the provider host is created on
// the browser process before launching the service worker's thread. Mojo's
// connection to the renderer is established with the StartWorker message.
// 3) When PlzNavigate is turned on, an instance is pre-created on the browser
// before ServiceWorkerNetworkProvider is created on the renderer because
// navigation is initiated on the browser side. In that case, establishment of
// Mojo's connection will be deferred until ServiceWorkerNetworkProvider is
// created on the renderer.
// Destruction of the ServiceWorkerProviderHost instance happens on
// disconnection of the Mojo's pipe from the renderer side regardless of what
// the provider is for.
class CONTENT_EXPORT ServiceWorkerProviderHost
    : public ServiceWorkerRegistration::Listener,
      public base::SupportsWeakPtr<ServiceWorkerProviderHost>,
      public mojom::ServiceWorkerContainerHost,
      public service_manager::mojom::InterfaceProvider {
 public:
  using WebContentsGetter = base::Callback<WebContents*(void)>;

  // PlzNavigate
  // Used to pre-create a ServiceWorkerProviderHost for a navigation. The
  // ServiceWorkerNetworkProvider will later be created in the renderer, should
  // the navigation succeed. |is_parent_frame_is_secure| should be true for main
  // frames. Otherwise it is true iff all ancestor frames of this frame have a
  // secure origin. |web_contents_getter| indicates the tab where the navigation
  // is occurring.
  static std::unique_ptr<ServiceWorkerProviderHost> PreCreateNavigationHost(
      base::WeakPtr<ServiceWorkerContextCore> context,
      bool are_ancestors_secure,
      const WebContentsGetter& web_contents_getter);

  // Creates a ServiceWorkerProviderHost for hosting a running service worker.
  // Information about this provider host is passed down to the service worker
  // via StartWorker message.
  static std::unique_ptr<ServiceWorkerProviderHost> PreCreateForController(
      base::WeakPtr<ServiceWorkerContextCore> context);

  // Used to create a ServiceWorkerProviderHost when the renderer-side provider
  // is created. This ProviderHost will be created for the process specified by
  // |process_id|.
  static std::unique_ptr<ServiceWorkerProviderHost> Create(
      int process_id,
      ServiceWorkerProviderHostInfo info,
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerDispatcherHost> dispatcher_host);

  ~ServiceWorkerProviderHost() override;

  const std::string& client_uuid() const { return client_uuid_; }
  base::TimeTicks create_time() const { return create_time_; }
  int process_id() const { return render_process_id_; }
  int provider_id() const { return info_.provider_id; }
  int frame_id() const;
  int route_id() const { return info_.route_id; }
  const WebContentsGetter& web_contents_getter() const {
    return web_contents_getter_;
  }

  bool is_parent_frame_secure() const { return info_.is_parent_frame_secure; }

  // Returns whether this provider host is secure enough to have a service
  // worker controller.
  // Analogous to Blink's Document::isSecureContext. Because of how service
  // worker intercepts main resource requests, this check must be done
  // browser-side once the URL is known (see comments in
  // ServiceWorkerNetworkProvider::CreateForNavigation). This function uses
  // |document_url_| and |is_parent_frame_secure_| to determine context
  // security, so they must be set properly before calling this function.
  bool IsContextSecureForServiceWorker() const;

  bool IsHostToRunningServiceWorker() {
    return running_hosted_version_.get() != nullptr;
  }

  // Returns this provider's controller. The controller is typically the same as
  // active_version() but can differ in the following cases:
  // (1) The client was created before the registration existed or had an active
  // version (in spec language, it is not "using" the registration).
  // (2) The client had a controller but NotifyControllerLost() was called due
  // to an exceptional circumstance (here also it is not "using" the
  // registration).
  // (3) During algorithms such as the update, skipWaiting(), and claim() steps,
  // the active version and controller may temporarily differ. For example, to
  // perform skipWaiting(), the registration's active version is updated first
  // and then the provider host's controlling version is updated to match it.
  ServiceWorkerVersion* controller() const {
    // Only clients can have controllers.
    DCHECK(!controller_ || IsProviderForClient());
    return controller_.get();
  }

  ServiceWorkerVersion* active_version() const {
    return associated_registration_.get()
               ? associated_registration_->active_version()
               : nullptr;
  }

  ServiceWorkerVersion* waiting_version() const {
    return associated_registration_.get()
               ? associated_registration_->waiting_version()
               : nullptr;
  }

  ServiceWorkerVersion* installing_version() const {
    return associated_registration_.get()
               ? associated_registration_->installing_version()
               : nullptr;
  }

  // Returns the associated registration. The provider host listens to this
  // registration to resolve the .ready promise and set its controller.
  ServiceWorkerRegistration* associated_registration() const {
    // Only clients can have an associated registration.
    DCHECK(!associated_registration_ || IsProviderForClient());
    return associated_registration_.get();
  }

  // The running version, if any, that this provider is providing resource
  // loads for.
  ServiceWorkerVersion* running_hosted_version() const {
    DCHECK(!running_hosted_version_ ||
           info_.type ==
               blink::mojom::ServiceWorkerProviderType::kForServiceWorker);
    return running_hosted_version_.get();
  }

  // Sets the |document_url_|.  When this object is for a client,
  // |matching_registrations_| gets also updated to ensure that |document_url_|
  // is in scope of all |matching_registrations_|.
  void SetDocumentUrl(const GURL& url);
  const GURL& document_url() const { return document_url_; }

  void SetTopmostFrameUrl(const GURL& url);
  const GURL& topmost_frame_url() const { return topmost_frame_url_; }

  blink::mojom::ServiceWorkerProviderType provider_type() const {
    return info_.type;
  }
  bool IsProviderForClient() const;
  blink::mojom::ServiceWorkerClientType client_type() const;

  // For service worker clients. Associates to |registration| to listen for its
  // version change events and sets the controller. If |notify_controllerchange|
  // is true, instructs the renderer to dispatch a 'controllerchange' event.
  void AssociateRegistration(ServiceWorkerRegistration* registration,
                             bool notify_controllerchange);

  // For service worker clients. Clears the associated registration and stops
  // listening to it.
  void DisassociateRegistration();

  // Returns a handler for a request. May return nullptr if the request doesn't
  // require special handling.
  std::unique_ptr<ServiceWorkerRequestHandler> CreateRequestHandler(
      network::mojom::FetchRequestMode request_mode,
      network::mojom::FetchCredentialsMode credentials_mode,
      FetchRedirectMode redirect_mode,
      const std::string& integrity,
      bool keepalive,
      ResourceType resource_type,
      RequestContextType request_context_type,
      RequestContextFrameType frame_type,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      scoped_refptr<ResourceRequestBody> body,
      bool skip_service_worker);

  // Used to get a ServiceWorkerObjectInfo to send to the renderer. Finds an
  // existing ServiceWorkerHandle, and increments its reference count, or else
  // creates a new one (initialized to ref count 1). Returns the
  // ServiceWorkerObjectInfo from the handle. The renderer is expected to use
  // ServiceWorkerHandleReference::Adopt to balance out the ref count.
  blink::mojom::ServiceWorkerObjectInfoPtr GetOrCreateServiceWorkerHandle(
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
  void PostMessageToClient(
      ServiceWorkerVersion* version,
      const base::string16& message,
      const std::vector<blink::MessagePortChannel>& sent_message_ports);

  // Notifies the client that its controller used a feature, for UseCounter
  // purposes. This can only be called if IsProviderForClient() is true.
  void CountFeature(uint32_t feature);

  // Adds reference of this host's process to the |pattern|, the reference will
  // be removed in destructor.
  void AddScopedProcessReferenceToPattern(const GURL& pattern);

  // |registration| claims the document to be controlled.
  void ClaimedByRegistration(ServiceWorkerRegistration* registration);

  // Methods to support cross site navigations.
  std::unique_ptr<ServiceWorkerProviderHost> PrepareForCrossSiteTransfer();
  void CompleteCrossSiteTransfer(ServiceWorkerProviderHost* provisional_host);
  ServiceWorkerDispatcherHost* dispatcher_host() const {
    return dispatcher_host_.get();
  }

  // PlzNavigate
  // Completes initialization of provider hosts used for navigation requests.
  void CompleteNavigationInitialized(
      int process_id,
      ServiceWorkerProviderHostInfo info,
      base::WeakPtr<ServiceWorkerDispatcherHost> dispatcher_host);

  // Completes initialization of this provider host (which is for hosting a
  // service worker). It is called once a renderer process has been found to
  // host the worker. Returns the info needed for creating a
  // ServiceWorkerNetworkProvider on the renderer which will be connected to
  // this instance. This instance will keep the reference to |hosted_version|,
  // so please be careful not to create a reference cycle.
  mojom::ServiceWorkerProviderInfoForStartWorkerPtr
  CompleteStartWorkerPreparation(
      int process_id,
      scoped_refptr<ServiceWorkerVersion> hosted_version);

  // Sends event messages to the renderer. Events for the worker are queued up
  // until the worker thread id is known via SetReadyToSendMessagesToWorker().
  void SendServiceWorkerStateChangedMessage(
      int worker_handle_id,
      blink::mojom::ServiceWorkerState state);

  // Sets the worker thread id and flushes queued events.
  void SetReadyToSendMessagesToWorker(int render_thread_id);

  void AddMatchingRegistration(ServiceWorkerRegistration* registration);
  void RemoveMatchingRegistration(ServiceWorkerRegistration* registration);

  // An optimized implementation of [[Match Service Worker Registration]]
  // for current document.
  ServiceWorkerRegistration* MatchRegistration() const;

  // Removes the ServiceWorkerRegistrationObjectHost corresponding to
  // |registration_id|.
  void RemoveServiceWorkerRegistrationObjectHost(int64_t registration_id);

  // Calls ContentBrowserClient::AllowServiceWorker(). Returns true if content
  // settings allows service workers to run at |scope|. If this provider is for
  // a window client, the check involves the topmost frame url as well as
  // |scope|, and may display tab-level UI.
  bool AllowServiceWorker(const GURL& scope);

  // Called when our controller has been terminated and doomed due to an
  // exceptional condition like it could no longer be read from the script
  // cache.
  void NotifyControllerLost();

 private:
  friend class LinkHeaderServiceWorkerTest;
  friend class ServiceWorkerProviderHostTest;
  friend class ServiceWorkerWriteToCacheJobTest;
  friend class ServiceWorkerContextRequestHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest, Update_SameScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_SameSizeScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_TruncatedScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_ElongatedScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_EmptyScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDispatcherHostTest,
                           DispatchExtendableMessageEvent);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDispatcherHostTest,
                           DispatchExtendableMessageEvent_Fail);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProviderHostTest, ContextSecurity);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Unregister);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, RegisterDuplicateScript);
  FRIEND_TEST_ALL_PREFIXES(BackgroundSyncManagerTest,
                           RegisterWithoutLiveSWRegistration);

  ServiceWorkerProviderHost(
      int process_id,
      ServiceWorkerProviderHostInfo info,
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerDispatcherHost> dispatcher_host);

  // ServiceWorkerRegistration::Listener overrides.
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      ChangedVersionAttributesMask changed_mask,
      const ServiceWorkerRegistrationInfo& info) override;
  void OnRegistrationFailed(ServiceWorkerRegistration* registration) override;
  void OnRegistrationFinishedUninstalling(
      ServiceWorkerRegistration* registration) override;
  void OnSkippedWaiting(ServiceWorkerRegistration* registration) override;

  // Sets the controller field to |version| or if |version| is nullptr, clears
  // the field. If |notify_controllerchange| is true, instructs the renderer to
  // dispatch a 'controller' change event.
  void SetControllerVersionAttribute(ServiceWorkerVersion* version,
                                     bool notify_controllerchange);

  void SendAssociateRegistrationMessage();

  // Syncs matching registrations with live registrations.
  void SyncMatchingRegistrations();

  // Discards all references to matching registrations.
  void RemoveAllMatchingRegistrations();

  // Increase/decrease this host's process reference for |pattern|.
  void IncreaseProcessReference(const GURL& pattern);
  void DecreaseProcessReference(const GURL& pattern);

  void ReturnRegistrationForReadyIfNeeded();

  bool IsReadyToSendMessages() const;
  void Send(IPC::Message* message) const;

  // Sends information about the controller to the providers of the service
  // worker clients in the renderer. If |notify_controllerchange| is true,
  // instructs the renderer to dispatch a 'controllerchange' event.  If
  // |version| is non-null, it must be the same as |controller_|. |version| can
  // be null while |controller_| is non-null in the strange case of cross-site
  // transfer, which will be removed when the non-PlzNavigate code path is
  // removed.
  void SendSetControllerServiceWorker(ServiceWorkerVersion* version,
                                      bool notify_controllerchange);

  // Implements mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                RegisterCallback callback) override;
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override;
  void GetRegistrations(GetRegistrationsCallback callback) override;
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override;
  void GetControllerServiceWorker(
      mojom::ControllerServiceWorkerRequest controller_request) override;
  void CloneForWorker(
      mojom::ServiceWorkerContainerHostRequest container_host_request) override;

  // Callback for ServiceWorkerContextCore::RegisterServiceWorker().
  void RegistrationComplete(RegisterCallback callback,
                            int64_t trace_id,
                            ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            int64_t registration_id);
  // Callback for ServiceWorkerStorage::FindRegistrationForDocument().
  void GetRegistrationComplete(
      GetRegistrationCallback callback,
      int64_t trace_id,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  // Callback for ServiceWorkerStorage::GetRegistrationsForOrigin().
  void GetRegistrationsComplete(
      GetRegistrationsCallback callback,
      int64_t trace_id,
      ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations);

  bool IsValidRegisterMessage(
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      std::string* out_error) const;
  bool IsValidGetRegistrationMessage(const GURL& client_url,
                                     std::string* out_error) const;
  bool IsValidGetRegistrationsMessage(std::string* out_error) const;
  bool IsValidGetRegistrationForReadyMessage(std::string* out_error) const;

  // service_manager::mojom::InterfaceProvider:
  // For provider hosts that are hosting a running service worker.
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // Returns an object info representing |registration|. The object info holds a
  // Mojo connection to the ServiceWorkerRegistrationObjectHost for the
  // |registration| to ensure the host stays alive while the object info is
  // alive. A new ServiceWorkerRegistrationObjectHost instance is created if one
  // can not be found in |registration_object_hosts_|.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  CreateServiceWorkerRegistrationObjectInfo(
      scoped_refptr<ServiceWorkerRegistration> registration);

  // Perform common checks that need to run before ContainerHost methods that
  // come from a child process are handled.
  // |scope| is checked if it is allowed to run a service worker.
  // Returns true if all checks have passed.
  // If anything looks wrong |callback| will run with an error
  // message prefixed by |error_prefix| and |args|, and false is returned.
  template <typename CallbackType, typename... Args>
  bool CanServeContainerHostMethods(CallbackType* callback,
                                    const GURL& scope,
                                    const char* error_prefix,
                                    Args... args);

  const std::string client_uuid_;
  const base::TimeTicks create_time_;
  int render_process_id_;

  // For provider hosts that are hosting a running service worker, the id of the
  // service worker thread. Otherwise, |kDocumentMainThreadId|. May be
  // |kInvalidEmbeddedWorkerThreadId| before the hosted service worker starts
  // up, or during cross-site transfers.
  int render_thread_id_;

  // Keeps the basic provider's info provided from the renderer side.
  ServiceWorkerProviderHostInfo info_;

  // PlzNavigate
  // Only set when this object is pre-created for a navigation. It indicates the
  // tab where the navigation occurs.
  WebContentsGetter web_contents_getter_;

  GURL document_url_;
  GURL topmost_frame_url_;

  std::vector<GURL> associated_patterns_;
  scoped_refptr<ServiceWorkerRegistration> associated_registration_;

  // Keyed by registration scope URL length.
  using ServiceWorkerRegistrationMap =
      std::map<size_t, scoped_refptr<ServiceWorkerRegistration>>;
  // Contains all living registrations whose pattern this document's URL
  // starts with. It is empty if IsContextSecureForServiceWorker() is
  // false.
  ServiceWorkerRegistrationMap matching_registrations_;

  // Contains all ServiceWorkerRegistrationObjectHost instances corresponding to
  // the service worker registration JavaScript objects for the hosted execution
  // context (service worker global scope or service worker client) in the
  // renderer process.
  std::map<int64_t /* registration_id */,
           std::unique_ptr<ServiceWorkerRegistrationObjectHost>>
      registration_object_hosts_;

  // The ready() promise is only allowed to be created once.
  // |get_ready_callback_| has three states:
  // 1. |get_ready_callback_| is null when ready() has not yet been called.
  // 2. |*get_ready_callback_| is a valid OnceCallback after ready() has been
  //    called and the callback has not yet been run.
  // 3. |*get_ready_callback_| is a null OnceCallback after the callback has
  //    been run.
  std::unique_ptr<GetRegistrationForReadyCallback> get_ready_callback_;

  scoped_refptr<ServiceWorkerVersion> controller_;
  scoped_refptr<ServiceWorkerVersion> running_hosted_version_;
  base::WeakPtr<ServiceWorkerContextCore> context_;

  // |dispatcher_host_| is expected to outlive |this| because it destroys
  // |this| upon destruction. However, it may be null in several cases:
  // 1) In some tests.
  // 2) PlzNavigate and service worker startup pre-create a
  // ServiceWorkerProviderHost instance before there is a renderer assigned to
  // it. The dispatcher host is set once the instance starts hosting a
  // renderer.
  // 3) During cross-site transfer.
  base::WeakPtr<ServiceWorkerDispatcherHost> dispatcher_host_;

  bool allow_association_;

  // |container_| is the Mojo endpoint to the renderer-side
  // ServiceWorkerContainer that |this| is a ServiceWorkerContainerHost for.
  mojom::ServiceWorkerContainerAssociatedPtr container_;
  // |binding_| is the Mojo binding that keeps the connection to the
  // renderer-side counterpart (content::ServiceWorkerNetworkProvider). When the
  // connection bound on |binding_| gets killed from the renderer side, or the
  // bound |ServiceWorkerProviderInfoForStartWorker::host_ptr_info| is otherwise
  // destroyed before being passed to the renderer, this
  // content::ServiceWorkerProviderHost will be destroyed.
  mojo::AssociatedBinding<mojom::ServiceWorkerContainerHost> binding_;

  // Mojo bindings for provider host pointers which are used from (dedicated or
  // shared) worker threads.
  // When this is hosting a shared worker, |bindings_for_worker_threads_|
  // contains exactly one element for the shared worker thread. This binding is
  // needed because the host pointer which is bound to |binding_| can only be
  // used from the main thread.
  // When this is hosting a document, |bindings_for_worker_threads_| contains
  // all dedicated workers associated with the document. This binding is needed
  // for the host pointers which are used from the dedicated worker threads.
  mojo::BindingSet<mojom::ServiceWorkerContainerHost>
      bindings_for_worker_threads_;

  std::vector<base::Closure> queued_events_;

  // For provider hosts that are hosting a running service worker.
  mojo::Binding<service_manager::mojom::InterfaceProvider>
      interface_provider_binding_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
