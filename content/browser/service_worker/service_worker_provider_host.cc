// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include <utility>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "content/browser/service_worker/browser_side_service_worker_event_dispatcher.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/browser/service_worker/service_worker_script_url_loader_factory.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/resource_request_body.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "net/base/url_util.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

// Provider host for navigation with PlzNavigate or when service worker's
// context is created on the browser side. This function provides the next
// ServiceWorkerProviderHost ID for them, starts at -2 and keeps going down.
int NextBrowserProvidedProviderId() {
  static int g_next_browser_provided_provider_id = -2;
  return g_next_browser_provided_provider_id--;
}

// A request handler derivative used to handle navigation requests when
// skip_service_worker flag is set. It tracks the document URL and sets the url
// to the provider host.
class ServiceWorkerURLTrackingRequestHandler
    : public ServiceWorkerRequestHandler {
 public:
  ServiceWorkerURLTrackingRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      ResourceType resource_type)
      : ServiceWorkerRequestHandler(context,
                                    provider_host,
                                    blob_storage_context,
                                    resource_type) {}
  ~ServiceWorkerURLTrackingRequestHandler() override {}

  // Called via custom URLRequestJobFactory.
  net::URLRequestJob* MaybeCreateJob(net::URLRequest* request,
                                     net::NetworkDelegate*,
                                     ResourceContext*) override {
    // |provider_host_| may have been deleted when the request is resumed.
    if (!provider_host_)
      return nullptr;
    const GURL stripped_url = net::SimplifyUrlForRequest(request->url());
    provider_host_->SetDocumentUrl(stripped_url);
    provider_host_->SetTopmostFrameUrl(request->site_for_cookies());
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLTrackingRequestHandler);
};

void RemoveProviderHost(base::WeakPtr<ServiceWorkerContextCore> context,
                        int process_id,
                        int provider_id) {
  // Temporary CHECK for debugging https://crbug.com/750267.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerProviderHost::RemoveProviderHost");
  if (!context || !context->GetProviderHost(process_id, provider_id)) {
    // In some cancellation of navigation cases, it is possible for the
    // pre-created host, whose |provider_id| is assigned by the browser process,
    // to have been destroyed before being claimed by the renderer. The provider
    // is then destroyed in the renderer, and no matching host will be found.
    return;
  }
  context->RemoveProviderHost(process_id, provider_id);
}

}  // anonymous namespace

ServiceWorkerProviderHost::OneShotGetReadyCallback::OneShotGetReadyCallback(
    const GetRegistrationForReadyCallback& callback)
    : callback(callback),
      called(false) {
}

ServiceWorkerProviderHost::OneShotGetReadyCallback::~OneShotGetReadyCallback() {
}

// static
std::unique_ptr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::PreCreateNavigationHost(
    base::WeakPtr<ServiceWorkerContextCore> context,
    bool are_ancestors_secure,
    const WebContentsGetter& web_contents_getter) {
  CHECK(IsBrowserSideNavigationEnabled());
  auto host = base::WrapUnique(new ServiceWorkerProviderHost(
      ChildProcessHost::kInvalidUniqueID,
      ServiceWorkerProviderHostInfo(
          NextBrowserProvidedProviderId(), MSG_ROUTING_NONE,
          SERVICE_WORKER_PROVIDER_FOR_WINDOW, are_ancestors_secure),
      context, nullptr /* dispatcher_host */));
  host->web_contents_getter_ = web_contents_getter;
  return host;
}

// static
std::unique_ptr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::PreCreateForController(
    base::WeakPtr<ServiceWorkerContextCore> context) {
  auto host = base::WrapUnique(new ServiceWorkerProviderHost(
      ChildProcessHost::kInvalidUniqueID,
      ServiceWorkerProviderHostInfo(NextBrowserProvidedProviderId(),
                                    MSG_ROUTING_NONE,
                                    SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,
                                    true /* is_parent_frame_secure */),
      std::move(context), nullptr));
  return host;
}

// static
std::unique_ptr<ServiceWorkerProviderHost> ServiceWorkerProviderHost::Create(
    int process_id,
    ServiceWorkerProviderHostInfo info,
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerDispatcherHost> dispatcher_host) {
  return base::WrapUnique(new ServiceWorkerProviderHost(
      process_id, std::move(info), context, dispatcher_host));
}

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int render_process_id,
    ServiceWorkerProviderHostInfo info,
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerDispatcherHost> dispatcher_host)
    : client_uuid_(base::GenerateGUID()),
      create_time_(base::TimeTicks::Now()),
      render_process_id_(render_process_id),
      render_thread_id_(kDocumentMainThreadId),
      info_(std::move(info)),
      context_(context),
      dispatcher_host_(dispatcher_host),
      allow_association_(true),
      binding_(this) {
  DCHECK_NE(SERVICE_WORKER_PROVIDER_UNKNOWN, info_.type);

  if (info_.type == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER) {
    // Actual |render_process_id| will be set after choosing a process for the
    // controller, and |render_thread id| will be set when the service worker
    // context gets started.
    DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, render_process_id);
    render_thread_id_ = kInvalidEmbeddedWorkerThreadId;
  } else {
    // PlzNavigate
    DCHECK(render_process_id != ChildProcessHost::kInvalidUniqueID ||
           IsBrowserSideNavigationEnabled());
  }

  context_->RegisterProviderHostByClientID(client_uuid_, this);

  // |client_| and |binding_| will be bound on CompleteNavigationInitialized
  // (PlzNavigate) or on CompleteStartWorkerPreparation (providers for
  // controller).
  if (!info_.client_ptr_info.is_valid() && !info_.host_request.is_pending()) {
    DCHECK(IsBrowserSideNavigationEnabled() ||
           info_.type == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER);
    return;
  }

  container_.Bind(std::move(info_.client_ptr_info));
  binding_.Bind(std::move(info_.host_request));
  binding_.set_connection_error_handler(base::BindOnce(
      &RemoveProviderHost, context_, render_process_id, info_.provider_id));
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  // Temporary CHECK for debugging https://crbug.com/750267.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (context_)
    context_->UnregisterProviderHostByClientID(client_uuid_);

  // Clear docurl so the deferred activation of a waiting worker
  // won't associate the new version with a provider being destroyed.
  document_url_ = GURL();
  if (controller_.get())
    controller_->RemoveControllee(this);

  RemoveAllMatchingRegistrations();

  for (const GURL& pattern : associated_patterns_)
    DecreaseProcessReference(pattern);
}

int ServiceWorkerProviderHost::frame_id() const {
  if (info_.type == SERVICE_WORKER_PROVIDER_FOR_WINDOW)
    return info_.route_id;
  return MSG_ROUTING_NONE;
}

bool ServiceWorkerProviderHost::IsContextSecureForServiceWorker() const {
  // |document_url_| may be empty if loading has not begun, or
  // ServiceWorkerRequestHandler didn't handle the load (because e.g. another
  // handler did first, or the initial request URL was such that
  // OriginCanAccessServiceWorkers returned false).
  if (!document_url_.is_valid())
    return false;
  if (!OriginCanAccessServiceWorkers(document_url_))
    return false;

  if (is_parent_frame_secure())
    return true;

  std::set<std::string> schemes;
  GetContentClient()->browser()->GetSchemesBypassingSecureContextCheckWhitelist(
      &schemes);
  return schemes.find(document_url().scheme()) != schemes.end();
}

void ServiceWorkerProviderHost::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    ChangedVersionAttributesMask changed_mask,
    const ServiceWorkerRegistrationInfo& /* info */) {
  if (!get_ready_callback_ || get_ready_callback_->called)
    return;
  if (changed_mask.active_changed() && registration->active_version()) {
    // Wait until the state change so we don't send the get for ready
    // registration complete message before set version attributes message.
    registration->active_version()->RegisterStatusChangeCallback(base::BindOnce(
        &ServiceWorkerProviderHost::ReturnRegistrationForReadyIfNeeded,
        AsWeakPtr()));
  }
}

void ServiceWorkerProviderHost::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  if (associated_registration_ == registration)
    DisassociateRegistration();
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerProviderHost::OnRegistrationFinishedUninstalling(
    ServiceWorkerRegistration* registration) {
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerProviderHost::OnSkippedWaiting(
    ServiceWorkerRegistration* registration) {
  if (associated_registration_ != registration)
    return;
  // A client is "using" a registration if it is controlled by the active
  // worker of the registration. skipWaiting doesn't cause a client to start
  // using the registration.
  if (!controller_)
    return;
  ServiceWorkerVersion* active_version = registration->active_version();
  DCHECK(active_version);
  DCHECK_EQ(active_version->status(), ServiceWorkerVersion::ACTIVATING);
  SetControllerVersionAttribute(active_version,
                                true /* notify_controllerchange */);
}

void ServiceWorkerProviderHost::SetDocumentUrl(const GURL& url) {
  DCHECK(!url.has_ref());
  document_url_ = url;
  if (IsProviderForClient())
    SyncMatchingRegistrations();
}

void ServiceWorkerProviderHost::SetTopmostFrameUrl(const GURL& url) {
  topmost_frame_url_ = url;
}

void ServiceWorkerProviderHost::SetControllerVersionAttribute(
    ServiceWorkerVersion* version,
    bool notify_controllerchange) {
  CHECK(!version || IsContextSecureForServiceWorker());
  if (version == controller_.get())
    return;

  scoped_refptr<ServiceWorkerVersion> previous_version = controller_;
  controller_ = version;

  // This will drop the message pipes to the client pages as well.
  controller_event_dispatcher_.reset();

  if (version) {
    version->AddControllee(this);
    controller_event_dispatcher_ =
        base::MakeUnique<BrowserSideServiceWorkerEventDispatcher>(version);
  }
  if (previous_version.get())
    previous_version->RemoveControllee(this);

  // SetController message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  SendSetControllerServiceWorker(version, notify_controllerchange);
}

bool ServiceWorkerProviderHost::IsProviderForClient() const {
  switch (info_.type) {
    case SERVICE_WORKER_PROVIDER_FOR_WINDOW:
    case SERVICE_WORKER_PROVIDER_FOR_WORKER:
    case SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      return true;
    case SERVICE_WORKER_PROVIDER_FOR_CONTROLLER:
      return false;
    case SERVICE_WORKER_PROVIDER_UNKNOWN:
      NOTREACHED() << info_.type;
  }
  NOTREACHED() << info_.type;
  return false;
}

blink::WebServiceWorkerClientType ServiceWorkerProviderHost::client_type()
    const {
  switch (info_.type) {
    case SERVICE_WORKER_PROVIDER_FOR_WINDOW:
      return blink::kWebServiceWorkerClientTypeWindow;
    case SERVICE_WORKER_PROVIDER_FOR_WORKER:
      return blink::kWebServiceWorkerClientTypeWorker;
    case SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      return blink::kWebServiceWorkerClientTypeSharedWorker;
    case SERVICE_WORKER_PROVIDER_FOR_CONTROLLER:
    case SERVICE_WORKER_PROVIDER_UNKNOWN:
      NOTREACHED() << info_.type;
  }
  NOTREACHED() << info_.type;
  return blink::kWebServiceWorkerClientTypeWindow;
}

void ServiceWorkerProviderHost::AssociateRegistration(
    ServiceWorkerRegistration* registration,
    bool notify_controllerchange) {
  CHECK(IsContextSecureForServiceWorker());
  DCHECK(CanAssociateRegistration(registration));
  associated_registration_ = registration;
  AddMatchingRegistration(registration);
  SetControllerVersionAttribute(registration->active_version(),
                                notify_controllerchange);
}

void ServiceWorkerProviderHost::DisassociateRegistration() {
  queued_events_.clear();
  if (!associated_registration_.get())
    return;
  associated_registration_ = nullptr;
  SetControllerVersionAttribute(nullptr, false /* notify_controllerchange */);
}

void ServiceWorkerProviderHost::AddMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(
      ServiceWorkerUtils::ScopeMatches(registration->pattern(), document_url_));
  if (!IsContextSecureForServiceWorker())
    return;
  size_t key = registration->pattern().spec().size();
  if (base::ContainsKey(matching_registrations_, key))
    return;
  IncreaseProcessReference(registration->pattern());
  registration->AddListener(this);
  matching_registrations_[key] = registration;
  ReturnRegistrationForReadyIfNeeded();
}

void ServiceWorkerProviderHost::RemoveMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  size_t key = registration->pattern().spec().size();
  DCHECK(base::ContainsKey(matching_registrations_, key));
  DecreaseProcessReference(registration->pattern());
  registration->RemoveListener(this);
  matching_registrations_.erase(key);
}

ServiceWorkerRegistration*
ServiceWorkerProviderHost::MatchRegistration() const {
  ServiceWorkerRegistrationMap::const_reverse_iterator it =
      matching_registrations_.rbegin();
  for (; it != matching_registrations_.rend(); ++it) {
    if (it->second->is_uninstalled())
      continue;
    if (it->second->is_uninstalling())
      return nullptr;
    return it->second.get();
  }
  return nullptr;
}

void ServiceWorkerProviderHost::NotifyControllerLost() {
  SetControllerVersionAttribute(nullptr, true /* notify_controllerchange */);
}

std::unique_ptr<ServiceWorkerRequestHandler>
ServiceWorkerProviderHost::CreateRequestHandler(
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    FetchRedirectMode redirect_mode,
    const std::string& integrity,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    scoped_refptr<ResourceRequestBody> body,
    bool skip_service_worker) {
  // |skip_service_worker| is meant to apply to requests that could be handled
  // by a service worker, as opposed to requests for the service worker script
  // itself. So ignore it here for the service worker script and its imported
  // scripts.
  // TODO(falken): Really it should be treated as an error to set
  // |skip_service_worker| for requests to start the service worker, but it's
  // difficult to fix that renderer-side, since we don't know whether a request
  // is for a service worker without access to IsHostToRunningServiceWorker() as
  // that state is stored browser-side.
  if (IsHostToRunningServiceWorker() &&
      (resource_type == RESOURCE_TYPE_SERVICE_WORKER ||
       resource_type == RESOURCE_TYPE_SCRIPT)) {
    skip_service_worker = false;
  }
  if (skip_service_worker) {
    if (!ServiceWorkerUtils::IsMainResourceType(resource_type))
      return std::unique_ptr<ServiceWorkerRequestHandler>();
    return base::MakeUnique<ServiceWorkerURLTrackingRequestHandler>(
        context_, AsWeakPtr(), blob_storage_context, resource_type);
  }
  if (IsHostToRunningServiceWorker()) {
    return base::MakeUnique<ServiceWorkerContextRequestHandler>(
        context_, AsWeakPtr(), blob_storage_context, resource_type);
  }
  if (ServiceWorkerUtils::IsMainResourceType(resource_type) || controller()) {
    return base::MakeUnique<ServiceWorkerControlleeRequestHandler>(
        context_, AsWeakPtr(), blob_storage_context, request_mode,
        credentials_mode, redirect_mode, integrity, resource_type,
        request_context_type, frame_type, body);
  }
  return std::unique_ptr<ServiceWorkerRequestHandler>();
}

ServiceWorkerObjectInfo
ServiceWorkerProviderHost::GetOrCreateServiceWorkerHandle(
    ServiceWorkerVersion* version) {
  DCHECK(dispatcher_host_);
  if (!context_ || !version)
    return ServiceWorkerObjectInfo();
  ServiceWorkerHandle* handle = dispatcher_host_->FindServiceWorkerHandle(
      provider_id(), version->version_id());
  if (handle) {
    handle->IncrementRefCount();
    return handle->GetObjectInfo();
  }

  std::unique_ptr<ServiceWorkerHandle> new_handle(
      ServiceWorkerHandle::Create(context_, AsWeakPtr(), version));
  handle = new_handle.get();
  dispatcher_host_->RegisterServiceWorkerHandle(std::move(new_handle));
  return handle->GetObjectInfo();
}

bool ServiceWorkerProviderHost::CanAssociateRegistration(
    ServiceWorkerRegistration* registration) {
  if (!context_)
    return false;
  if (running_hosted_version_.get())
    return false;
  if (!registration || associated_registration_.get() || !allow_association_)
    return false;
  return true;
}

void ServiceWorkerProviderHost::PostMessageToClient(
    ServiceWorkerVersion* version,
    const base::string16& message,
    const std::vector<MessagePort>& sent_message_ports) {
  if (!dispatcher_host_)
    return;

  ServiceWorkerMsg_MessageToDocument_Params params;
  params.thread_id = kDocumentMainThreadId;
  params.provider_id = provider_id();
  params.service_worker_info = GetOrCreateServiceWorkerHandle(version);
  params.message = message;
  params.message_ports = sent_message_ports;
  Send(new ServiceWorkerMsg_MessageToDocument(params));
}

void ServiceWorkerProviderHost::CountFeature(uint32_t feature) {
  if (!dispatcher_host_)
    return;

  // CountFeature message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  Send(new ServiceWorkerMsg_CountFeature(render_thread_id_, provider_id(),
                                         feature));
}

void ServiceWorkerProviderHost::AddScopedProcessReferenceToPattern(
    const GURL& pattern) {
  associated_patterns_.push_back(pattern);
  IncreaseProcessReference(pattern);
}

void ServiceWorkerProviderHost::ClaimedByRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(registration->active_version());
  if (registration == associated_registration_) {
    SetControllerVersionAttribute(registration->active_version(),
                                  true /* notify_controllerchange */);
  } else if (allow_association_) {
    DisassociateRegistration();
    AssociateRegistration(registration, true /* notify_controllerchange */);
  }
}

bool ServiceWorkerProviderHost::GetRegistrationForReady(
    const GetRegistrationForReadyCallback& callback) {
  if (get_ready_callback_)
    return false;
  get_ready_callback_.reset(new OneShotGetReadyCallback(callback));
  ReturnRegistrationForReadyIfNeeded();
  return true;
}

std::unique_ptr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::PrepareForCrossSiteTransfer() {
  DCHECK(!IsBrowserSideNavigationEnabled());
  DCHECK(!ServiceWorkerUtils::IsServicificationEnabled());
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_NE(MSG_ROUTING_NONE, info_.route_id);
  DCHECK_EQ(kDocumentMainThreadId, render_thread_id_);
  DCHECK_NE(SERVICE_WORKER_PROVIDER_UNKNOWN, info_.type);

  std::unique_ptr<ServiceWorkerProviderHost> provisional_host =
      base::WrapUnique(new ServiceWorkerProviderHost(
          process_id(),
          ServiceWorkerProviderHostInfo(std::move(info_), binding_.Unbind(),
                                        container_.PassInterface()),
          context_, dispatcher_host_));

  for (const GURL& pattern : associated_patterns_)
    DecreaseProcessReference(pattern);

  RemoveAllMatchingRegistrations();

  // Clear the controller from the renderer-side provider, since no one knows
  // what's going to happen until after cross-site transfer finishes.
  if (controller_) {
    SendSetControllerServiceWorker(nullptr,
                                   false /* notify_controllerchange */);
  }

  render_process_id_ = ChildProcessHost::kInvalidUniqueID;
  render_thread_id_ = kInvalidEmbeddedWorkerThreadId;
  dispatcher_host_ = nullptr;
  return provisional_host;
}

void ServiceWorkerProviderHost::CompleteCrossSiteTransfer(
    ServiceWorkerProviderHost* provisional_host) {
  DCHECK(!IsBrowserSideNavigationEnabled());
  DCHECK(!ServiceWorkerUtils::IsServicificationEnabled());
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, provisional_host->process_id());
  DCHECK_NE(MSG_ROUTING_NONE, provisional_host->frame_id());

  render_process_id_ = provisional_host->process_id();
  render_thread_id_ = kDocumentMainThreadId;
  dispatcher_host_ = provisional_host->dispatcher_host()
                         ? provisional_host->dispatcher_host()->AsWeakPtr()
                         : nullptr;
  info_ = std::move(provisional_host->info_);

  // Take the connection over from the provisional host.
  DCHECK(!container_.is_bound());
  DCHECK(!binding_.is_bound());
  container_.Bind(provisional_host->container_.PassInterface());
  binding_.Bind(provisional_host->binding_.Unbind());
  binding_.set_connection_error_handler(
      base::BindOnce(&RemoveProviderHost, context_,
                     provisional_host->process_id(), provider_id()));

  for (const GURL& pattern : associated_patterns_)
    IncreaseProcessReference(pattern);
  SyncMatchingRegistrations();

  // Now that the provider is stable and the connection is established,
  // send it the SetController IPC if there is a controller.
  if (controller_) {
    SendSetControllerServiceWorker(controller_.get(),
                                   false /* notify_controllerchange */);
  }
}

// PlzNavigate
void ServiceWorkerProviderHost::CompleteNavigationInitialized(
    int process_id,
    ServiceWorkerProviderHostInfo info,
    base::WeakPtr<ServiceWorkerDispatcherHost> dispatcher_host) {
  CHECK(IsBrowserSideNavigationEnabled());
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_EQ(SERVICE_WORKER_PROVIDER_FOR_WINDOW, info_.type);
  DCHECK_EQ(kDocumentMainThreadId, render_thread_id_);

  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);
  DCHECK_EQ(info_.provider_id, info.provider_id);
  DCHECK_NE(MSG_ROUTING_NONE, info.route_id);

  // Connect with the mojom::ServiceWorkerContainer on the renderer.
  DCHECK(!container_.is_bound());
  DCHECK(!binding_.is_bound());
  container_.Bind(std::move(info.client_ptr_info));
  binding_.Bind(std::move(info.host_request));
  binding_.set_connection_error_handler(
      base::BindOnce(&RemoveProviderHost, context_, process_id, provider_id()));
  info_.route_id = info.route_id;
  render_process_id_ = process_id;
  dispatcher_host_ = dispatcher_host;

  // Increase the references because the process which this provider host will
  // host has been decided.
  for (const GURL& pattern : associated_patterns_)
    IncreaseProcessReference(pattern);
  for (auto& key_registration : matching_registrations_)
    IncreaseProcessReference(key_registration.second->pattern());

  // Now that there is a connection with the renderer-side provider,
  // send it the SetController IPC.
  if (controller_) {
    SendSetControllerServiceWorker(controller_.get(),
                                   false /* notify_controllerchange */);
  }
}

mojom::ServiceWorkerProviderInfoForStartWorkerPtr
ServiceWorkerProviderHost::CompleteStartWorkerPreparation(
    int process_id,
    scoped_refptr<ServiceWorkerVersion> hosted_version) {
  DCHECK(context_);
  DCHECK_EQ(kInvalidEmbeddedWorkerThreadId, render_thread_id_);
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_EQ(SERVICE_WORKER_PROVIDER_FOR_CONTROLLER, provider_type());
  DCHECK(!running_hosted_version_);

  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);

  running_hosted_version_ = std::move(hosted_version);

  ServiceWorkerDispatcherHost* dispatcher_host =
      context_->GetDispatcherHost(process_id);
  DCHECK(dispatcher_host);
  render_process_id_ = process_id;
  dispatcher_host_ = dispatcher_host->AsWeakPtr();

  // Retrieve the registration associated with |version|. The registration
  // must be alive because the version keeps it during starting worker.
  ServiceWorkerRegistration* registration = context_->GetLiveRegistration(
      running_hosted_version()->registration_id());
  DCHECK(registration);
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  dispatcher_host->GetRegistrationObjectInfoAndVersionAttributes(
      AsWeakPtr(), registration, &info, &attrs);

  // Initialize provider_info.
  mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info =
      mojom::ServiceWorkerProviderInfoForStartWorker::New();
  provider_info->provider_id = provider_id();
  provider_info->attributes = std::move(attrs);
  provider_info->registration = std::move(info);
  provider_info->client_request = mojo::MakeRequest(&container_);

  mojom::URLLoaderFactoryAssociatedPtrInfo script_loader_factory_ptr_info;
  if (ServiceWorkerUtils::IsServicificationEnabled()) {
    mojo::MakeStrongAssociatedBinding(
        base::MakeUnique<ServiceWorkerScriptURLLoaderFactory>(
            context_, AsWeakPtr(), context_->blob_storage_context(),
            context_->loader_factory_getter()),
        mojo::MakeRequest(&script_loader_factory_ptr_info));
    provider_info->script_loader_factory_ptr_info =
        std::move(script_loader_factory_ptr_info);
  }

  binding_.Bind(mojo::MakeRequest(&provider_info->host_ptr_info));
  binding_.set_connection_error_handler(
      base::BindOnce(&RemoveProviderHost, context_, process_id, provider_id()));

  // Set the document URL to the script url in order to allow
  // register/unregister/getRegistration on ServiceWorkerGlobalScope.
  SetDocumentUrl(running_hosted_version()->script_url());

  return provider_info;
}

void ServiceWorkerProviderHost::SendUpdateFoundMessage(
    int registration_handle_id) {
  if (!dispatcher_host_)
    return;

  if (!IsReadyToSendMessages()) {
    queued_events_.push_back(
        base::Bind(&ServiceWorkerProviderHost::SendUpdateFoundMessage,
                   AsWeakPtr(), registration_handle_id));
    return;
  }

  Send(new ServiceWorkerMsg_UpdateFound(
      render_thread_id_, registration_handle_id));
}

void ServiceWorkerProviderHost::SendSetVersionAttributesMessage(
    int registration_handle_id,
    ChangedVersionAttributesMask changed_mask,
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
  if (!dispatcher_host_)
    return;
  if (!changed_mask.changed())
    return;

  if (!IsReadyToSendMessages()) {
    queued_events_.push_back(base::Bind(
        &ServiceWorkerProviderHost::SendSetVersionAttributesMessage,
        AsWeakPtr(), registration_handle_id, changed_mask,
        base::RetainedRef(installing_version),
        base::RetainedRef(waiting_version), base::RetainedRef(active_version)));
    return;
  }

  ServiceWorkerVersionAttributes attrs;
  if (changed_mask.installing_changed())
    attrs.installing = GetOrCreateServiceWorkerHandle(installing_version);
  if (changed_mask.waiting_changed())
    attrs.waiting = GetOrCreateServiceWorkerHandle(waiting_version);
  if (changed_mask.active_changed())
    attrs.active = GetOrCreateServiceWorkerHandle(active_version);

  Send(new ServiceWorkerMsg_SetVersionAttributes(
      render_thread_id_, registration_handle_id, changed_mask.changed(),
      attrs));
}

void ServiceWorkerProviderHost::SendServiceWorkerStateChangedMessage(
    int worker_handle_id,
    blink::WebServiceWorkerState state) {
  if (!dispatcher_host_)
    return;

  if (!IsReadyToSendMessages()) {
    queued_events_.push_back(base::Bind(
        &ServiceWorkerProviderHost::SendServiceWorkerStateChangedMessage,
        AsWeakPtr(), worker_handle_id, state));
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerStateChanged(
      render_thread_id_, worker_handle_id, state));
}

void ServiceWorkerProviderHost::SetReadyToSendMessagesToWorker(
    int render_thread_id) {
  DCHECK(!IsReadyToSendMessages());
  render_thread_id_ = render_thread_id;

  for (const auto& event : queued_events_)
    event.Run();
  queued_events_.clear();
}

void ServiceWorkerProviderHost::SyncMatchingRegistrations() {
  DCHECK(context_);
  RemoveAllMatchingRegistrations();
  const auto& registrations = context_->GetLiveRegistrations();
  for (const auto& key_registration : registrations) {
    ServiceWorkerRegistration* registration = key_registration.second;
    if (!registration->is_uninstalled() &&
        ServiceWorkerUtils::ScopeMatches(registration->pattern(),
                                         document_url_))
      AddMatchingRegistration(registration);
  }
}

void ServiceWorkerProviderHost::RemoveAllMatchingRegistrations() {
  for (const auto& it : matching_registrations_) {
    ServiceWorkerRegistration* registration = it.second.get();
    DecreaseProcessReference(registration->pattern());
    registration->RemoveListener(this);
  }
  matching_registrations_.clear();
}

void ServiceWorkerProviderHost::IncreaseProcessReference(
    const GURL& pattern) {
  if (context_ && context_->process_manager()) {
    context_->process_manager()->AddProcessReferenceToPattern(
        pattern, render_process_id_);
  }
}

void ServiceWorkerProviderHost::DecreaseProcessReference(
    const GURL& pattern) {
  if (context_ && context_->process_manager()) {
    context_->process_manager()->RemoveProcessReferenceFromPattern(
        pattern, render_process_id_);
  }
}

void ServiceWorkerProviderHost::ReturnRegistrationForReadyIfNeeded() {
  if (!get_ready_callback_ || get_ready_callback_->called)
    return;
  ServiceWorkerRegistration* registration = MatchRegistration();
  if (!registration)
    return;
  if (registration->active_version()) {
    get_ready_callback_->callback.Run(registration);
    get_ready_callback_->callback.Reset();
    get_ready_callback_->called = true;
    return;
  }
}

bool ServiceWorkerProviderHost::IsReadyToSendMessages() const {
  return render_thread_id_ != kInvalidEmbeddedWorkerThreadId;
}

bool ServiceWorkerProviderHost::IsContextAlive() {
  return context_ != nullptr;
}

void ServiceWorkerProviderHost::Send(IPC::Message* message) const {
  DCHECK(dispatcher_host_);
  DCHECK(IsReadyToSendMessages());
  dispatcher_host_->Send(message);
}

void ServiceWorkerProviderHost::SendSetControllerServiceWorker(
    ServiceWorkerVersion* version,
    bool notify_controllerchange) {
  if (!dispatcher_host_)
    return;

  if (version) {
    DCHECK(associated_registration_);
    DCHECK_EQ(associated_registration_->active_version(), version);
    DCHECK_EQ(controller_.get(), version);
  }

  ServiceWorkerMsg_SetControllerServiceWorker_Params params;
  params.thread_id = render_thread_id_;
  params.provider_id = provider_id();
  params.object_info = GetOrCreateServiceWorkerHandle(version);
  params.should_notify_controllerchange = notify_controllerchange;
  if (version) {
    params.used_features = version->used_features();
    if (ServiceWorkerUtils::IsServicificationEnabled()) {
      params.controller_event_dispatcher =
          controller_event_dispatcher_->CreateEventDispatcherPtrInfo()
              .PassHandle()
              .release();
    }
  }
  Send(new ServiceWorkerMsg_SetControllerServiceWorker(params));
}

}  // namespace content
