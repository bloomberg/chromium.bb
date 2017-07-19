// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include <utility>

#include "base/feature_list.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/resource_request_body.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "net/base/url_util.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

// PlzNavigate
// Next ServiceWorkerProviderHost ID for navigations, starts at -2 and keeps
// going down.
int g_next_navigation_provider_id = -2;

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
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* /* network_delegate */,
      ResourceContext* /* resource_context */) override {
    // |provider_host_| may have been deleted when the request is resumed.
    if (!provider_host_)
      return nullptr;
    const GURL stripped_url = net::SimplifyUrlForRequest(request->url());
    provider_host_->SetDocumentUrl(stripped_url);
    provider_host_->SetTopmostFrameUrl(request->first_party_for_cookies());
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLTrackingRequestHandler);
};

void RemoveProviderHost(base::WeakPtr<ServiceWorkerContextCore> context,
                        int process_id,
                        int provider_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerProviderHost::RemoveProviderHost");
  if (!context)
    return;
  if (!context->GetProviderHost(process_id, provider_id)) {
    // PlzNavigate: in some cancellation of navigation cases, it is possible
    // for the pre-created host, whose |provider_id| is assigned by the browser
    // process, to have been destroyed before being claimed by the renderer. The
    // provider is then destroyed in the renderer, and no matching host will be
    // found.
    DCHECK(IsBrowserSideNavigationEnabled() &&
           ServiceWorkerUtils::IsBrowserAssignedProviderId(provider_id));
    return;
  }
  context->RemoveProviderHost(process_id, provider_id);
}

// Used by a Service Worker for script loading (for all script loading for now,
// but to be used only during installation once script streaming lands).
// For now this is just a proxy loader for the network loader.
// Eventually this should replace the existing URLRequestJob-based request
// interception for script loading, namely ServiceWorkerWriteToCacheJob.
// TODO(kinuko): Implement this. Hook up the existing code in
// ServiceWorkerContextRequestHandler.
class ScriptURLLoader : public mojom::URLLoader, public mojom::URLLoaderClient {
 public:
  ScriptURLLoader(
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& resource_request,
      mojom::URLLoaderClientPtr client,
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      : network_client_binding_(this),
        forwarding_client_(std::move(client)),
        provider_host_(provider_host) {
    mojom::URLLoaderClientPtr network_client;
    network_client_binding_.Bind(mojo::MakeRequest(&network_client));
    loader_factory_getter->GetNetworkFactory()->get()->CreateLoaderAndStart(
        mojo::MakeRequest(&network_loader_), routing_id, request_id, options,
        resource_request, std::move(network_client), traffic_annotation);
  }
  ~ScriptURLLoader() override {}

  // mojom::URLLoader:
  void FollowRedirect() override { network_loader_->FollowRedirect(); }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    network_loader_->SetPriority(priority, intra_priority_value);
  }

  // mojom::URLLoaderClient for simply proxying network:
  void OnReceiveResponse(
      const ResourceResponseHead& response_head,
      const base::Optional<net::SSLInfo>& ssl_info,
      mojom::DownloadedTempFilePtr downloaded_file) override {
    if (provider_host_) {
      // We don't have complete info here, but fill in what we have now.
      // At least we need headers and SSL info.
      net::HttpResponseInfo response_info;
      response_info.headers = response_head.headers;
      if (ssl_info.has_value())
        response_info.ssl_info = *ssl_info;
      response_info.was_fetched_via_spdy = response_head.was_fetched_via_spdy;
      response_info.was_alpn_negotiated = response_head.was_alpn_negotiated;
      response_info.alpn_negotiated_protocol =
          response_head.alpn_negotiated_protocol;
      response_info.connection_info = response_head.connection_info;
      response_info.socket_address = response_head.socket_address;

      DCHECK(provider_host_->IsHostToRunningServiceWorker());
      provider_host_->running_hosted_version()->SetMainScriptHttpResponseInfo(
          response_info);
    }
    forwarding_client_->OnReceiveResponse(response_head, ssl_info,
                                          std::move(downloaded_file));
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override {
    forwarding_client_->OnReceiveRedirect(redirect_info, response_head);
  }
  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override {
    forwarding_client_->OnDataDownloaded(data_len, encoded_data_len);
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    forwarding_client_->OnUploadProgress(current_position, total_size,
                                         std::move(ack_callback));
  }
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {
    forwarding_client_->OnReceiveCachedMetadata(data);
  }
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
  }
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    forwarding_client_->OnStartLoadingResponseBody(std::move(body));
  }
  void OnComplete(const ResourceRequestCompletionStatus& status) override {
    forwarding_client_->OnComplete(status);
  }

 private:
  mojom::URLLoaderAssociatedPtr network_loader_;
  mojo::Binding<mojom::URLLoaderClient> network_client_binding_;
  mojom::URLLoaderClientPtr forwarding_client_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;

  DISALLOW_COPY_AND_ASSIGN(ScriptURLLoader);
};

// Created per one controller worker for script loading (only during
// installation, eventually). This is kept alive while
// ServiceWorkerNetworkProvider in the renderer process is alive.
// Used only when IsServicificationEnabled is true.
class ScriptURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  ScriptURLLoaderFactory(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter)
      : context_(context),
        provider_host_(provider_host),
        blob_storage_context_(blob_storage_context),
        loader_factory_getter_(loader_factory_getter) {}
  ~ScriptURLLoaderFactory() override {}

  // mojom::URLLoaderFactory:
  void CreateLoaderAndStart(mojom::URLLoaderAssociatedRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    mojo::MakeStrongAssociatedBinding(
        base::MakeUnique<ScriptURLLoader>(
            routing_id, request_id, options, resource_request,
            std::move(client), context_, provider_host_, blob_storage_context_,
            loader_factory_getter_, traffic_annotation),
        std::move(request));
  }

  void SyncLoad(int32_t routing_id,
                int32_t request_id,
                const ResourceRequest& request,
                SyncLoadCallback callback) override {
    NOTREACHED();
  }

 private:
  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;
  DISALLOW_COPY_AND_ASSIGN(ScriptURLLoaderFactory);
};

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
  // Generate a new browser-assigned id for the host.
  int provider_id = g_next_navigation_provider_id--;
  auto host = base::WrapUnique(new ServiceWorkerProviderHost(
      ChildProcessHost::kInvalidUniqueID,
      ServiceWorkerProviderHostInfo(provider_id, MSG_ROUTING_NONE,
                                    SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                    are_ancestors_secure),
      context, nullptr));
  host->web_contents_getter_ = web_contents_getter;
  return host;
}

// static
std::unique_ptr<ServiceWorkerProviderHost> ServiceWorkerProviderHost::Create(
    int process_id,
    ServiceWorkerProviderHostInfo info,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host) {
  return base::WrapUnique(new ServiceWorkerProviderHost(
      process_id, std::move(info), context, dispatcher_host));
}

void ServiceWorkerProviderHost::BindWorkerFetchContext(
    mojom::ServiceWorkerWorkerClientAssociatedPtrInfo client_ptr_info) {
  DCHECK(base::FeatureList::IsEnabled(features::kOffMainThreadFetch));
  mojom::ServiceWorkerWorkerClientAssociatedPtr client;
  client.Bind(std::move(client_ptr_info));
  client.set_connection_error_handler(
      base::Bind(&ServiceWorkerProviderHost::UnregisterWorkerFetchContext,
                 base::Unretained(this), client.get()));

  if (controlling_version_)
    client->SetControllerServiceWorker(controlling_version_->version_id());

  auto result = worker_clients_.insert(
      std::make_pair<mojom::ServiceWorkerWorkerClient*,
                     mojom::ServiceWorkerWorkerClientAssociatedPtr>(
          client.get(), std::move(client)));
  DCHECK(result.second);
}

void ServiceWorkerProviderHost::UnregisterWorkerFetchContext(
    mojom::ServiceWorkerWorkerClient* client) {
  DCHECK(worker_clients_.count(client));
  worker_clients_.erase(client);
}

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int render_process_id,
    ServiceWorkerProviderHostInfo info,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host)
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

  // PlzNavigate
  CHECK(render_process_id != ChildProcessHost::kInvalidUniqueID ||
        IsBrowserSideNavigationEnabled());

  if (info_.type == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER) {
    // Actual thread id is set when the service worker context gets started.
    render_thread_id_ = kInvalidEmbeddedWorkerThreadId;
  }
  context_->RegisterProviderHostByClientID(client_uuid_, this);

  // PlzNavigate
  // |provider_| and |binding_| will be bound on CompleteNavigationInitialized.
  if (IsBrowserSideNavigationEnabled()) {
    DCHECK(!info.client_ptr_info.is_valid() && !info.host_request.is_pending());
    return;
  }

  provider_.Bind(std::move(info_.client_ptr_info));
  binding_.Bind(std::move(info_.host_request));
  binding_.set_connection_error_handler(base::Bind(
      &RemoveProviderHost, context_, render_process_id, info_.provider_id));
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  if (context_)
    context_->UnregisterProviderHostByClientID(client_uuid_);

  // Clear docurl so the deferred activation of a waiting worker
  // won't associate the new version with a provider being destroyed.
  document_url_ = GURL();
  if (controlling_version_.get())
    controlling_version_->RemoveControllee(this);

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
    registration->active_version()->RegisterStatusChangeCallback(base::Bind(
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
  if (!controlling_version_)
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
  if (version == controlling_version_.get())
    return;

  scoped_refptr<ServiceWorkerVersion> previous_version = controlling_version_;
  controlling_version_ = version;
  if (version) {
    version->AddControllee(this);
    for (const auto& pair : worker_clients_) {
      pair.second->SetControllerServiceWorker(version->version_id());
    }
  }
  if (previous_version.get())
    previous_version->RemoveControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  // SetController message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  Send(new ServiceWorkerMsg_SetControllerServiceWorker(
      render_thread_id_, provider_id(), GetOrCreateServiceWorkerHandle(version),
      notify_controllerchange,
      version ? version->used_features() : std::set<uint32_t>()));
}

void ServiceWorkerProviderHost::SetHostedVersion(
    ServiceWorkerVersion* version) {
  DCHECK(!IsProviderForClient());
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, version->running_status());
  DCHECK_EQ(render_process_id_, version->embedded_worker()->process_id());
  running_hosted_version_ = version;
}

void ServiceWorkerProviderHost::CreateScriptURLLoaderFactory(
    mojom::URLLoaderFactoryAssociatedRequest script_loader_factory_request) {
  DCHECK(ServiceWorkerUtils::IsServicificationEnabled());
  mojo::MakeStrongAssociatedBinding(
      base::MakeUnique<ScriptURLLoaderFactory>(
          context_, AsWeakPtr(), context_->blob_storage_context(),
          context_->loader_factory_getter()),
      std::move(script_loader_factory_request));
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
  SendAssociateRegistrationMessage();
  SetControllerVersionAttribute(registration->active_version(),
                                notify_controllerchange);
}

void ServiceWorkerProviderHost::DisassociateRegistration() {
  queued_events_.clear();
  if (!associated_registration_.get())
    return;
  associated_registration_ = NULL;
  SetControllerVersionAttribute(NULL, false /* notify_controllerchange */);

  if (!dispatcher_host_)
    return;

  // Disassociation message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  Send(new ServiceWorkerMsg_DisassociateRegistration(
      render_thread_id_, provider_id()));
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
  if (ServiceWorkerUtils::IsMainResourceType(resource_type) ||
      controlling_version()) {
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
    return;  // Could be NULL in some tests.

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
    return;  // Could be nullptr in some tests.

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
                                        provider_.PassInterface()),
          context_, dispatcher_host()));

  for (const GURL& pattern : associated_patterns_)
    DecreaseProcessReference(pattern);

  RemoveAllMatchingRegistrations();

  if (associated_registration_.get()) {
    if (dispatcher_host_) {
      Send(new ServiceWorkerMsg_DisassociateRegistration(
          render_thread_id_, provider_id()));
    }
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
  dispatcher_host_ = provisional_host->dispatcher_host();
  info_ = std::move(provisional_host->info_);

  // Take the connection over from the provisional host.
  DCHECK(!provider_.is_bound());
  DCHECK(!binding_.is_bound());
  provider_.Bind(provisional_host->provider_.PassInterface());
  binding_.Bind(provisional_host->binding_.Unbind());
  binding_.set_connection_error_handler(
      base::Bind(&RemoveProviderHost, context_, provisional_host->process_id(),
                 provider_id()));

  for (const GURL& pattern : associated_patterns_)
    IncreaseProcessReference(pattern);
  SyncMatchingRegistrations();

  NotifyControllerToAssociatedProvider();
}

// PlzNavigate
void ServiceWorkerProviderHost::CompleteNavigationInitialized(
    int process_id,
    ServiceWorkerProviderHostInfo info,
    ServiceWorkerDispatcherHost* dispatcher_host) {
  CHECK(IsBrowserSideNavigationEnabled());
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_EQ(SERVICE_WORKER_PROVIDER_FOR_WINDOW, info_.type);
  DCHECK_EQ(kDocumentMainThreadId, render_thread_id_);

  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);
  DCHECK_EQ(info_.provider_id, info.provider_id);
  DCHECK_NE(MSG_ROUTING_NONE, info.route_id);

  // Connect with the provider on the renderer.
  DCHECK(!provider_.is_bound());
  DCHECK(!binding_.is_bound());
  provider_.Bind(std::move(info.client_ptr_info));
  binding_.Bind(std::move(info.host_request));
  binding_.set_connection_error_handler(
      base::Bind(&RemoveProviderHost, context_, process_id, provider_id()));
  info_.route_id = info.route_id;
  render_process_id_ = process_id;
  dispatcher_host_ = dispatcher_host;

  // Increase the references because the process which this provider host will
  // host has been decided.
  for (const GURL& pattern : associated_patterns_)
    IncreaseProcessReference(pattern);
  for (auto& key_registration : matching_registrations_)
    IncreaseProcessReference(key_registration.second->pattern());

  NotifyControllerToAssociatedProvider();
}

void ServiceWorkerProviderHost::SendUpdateFoundMessage(
    int registration_handle_id) {
  if (!dispatcher_host_)
    return;  // Could be nullptr in some tests.

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
    return;  // Could be nullptr in some tests.
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

void ServiceWorkerProviderHost::SendAssociateRegistrationMessage() {
  if (!dispatcher_host_)
    return;

  ServiceWorkerRegistrationHandle* handle =
      dispatcher_host_->GetOrCreateRegistrationHandle(
          AsWeakPtr(), associated_registration_.get());

  ServiceWorkerVersionAttributes attrs;
  attrs.installing = GetOrCreateServiceWorkerHandle(
      associated_registration_->installing_version());
  attrs.waiting = GetOrCreateServiceWorkerHandle(
      associated_registration_->waiting_version());
  attrs.active = GetOrCreateServiceWorkerHandle(
      associated_registration_->active_version());

  // Association message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  dispatcher_host_->Send(new ServiceWorkerMsg_AssociateRegistration(
      render_thread_id_, provider_id(), handle->GetObjectInfo(), attrs));
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

void ServiceWorkerProviderHost::NotifyControllerToAssociatedProvider() {
  if (associated_registration_.get()) {
    SendAssociateRegistrationMessage();
    if (dispatcher_host_ && associated_registration_->active_version()) {
      Send(new ServiceWorkerMsg_SetControllerServiceWorker(
          render_thread_id_, provider_id(),
          GetOrCreateServiceWorkerHandle(
              associated_registration_->active_version()),
          false /* shouldNotifyControllerChange */,
          associated_registration_->active_version()->used_features()));
    }
  }
}

}  // namespace content
