// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_client_utils.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "ipc/ipc_message_macros.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_error_type.mojom.h"
#include "url/gurl.h"

using blink::MessagePortChannel;
using blink::WebServiceWorkerError;

namespace content {

namespace {

const char kNoDocumentURLErrorMessage[] =
    "No URL is associated with the caller's document.";
const char kShutdownErrorMessage[] =
    "The Service Worker system has shutdown.";
const char kUserDeniedPermissionMessage[] =
    "The user denied permission to use Service Worker.";
const char kEnableNavigationPreloadErrorPrefix[] =
    "Failed to enable or disable navigation preload: ";
const char kGetNavigationPreloadStateErrorPrefix[] =
    "Failed to get navigation preload state: ";
const char kSetNavigationPreloadHeaderErrorPrefix[] =
    "Failed to set navigation preload header: ";
const char kNoActiveWorkerErrorMessage[] =
    "The registration does not have an active worker.";
const char kDatabaseErrorMessage[] = "Failed to access storage.";

const uint32_t kFilteredMessageClasses[] = {
    ServiceWorkerMsgStart, EmbeddedWorkerMsgStart,
};

void RunSoon(const base::Closure& callback) {
  if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

WebContents* GetWebContents(int render_process_id, int render_frame_id) {
  RenderFrameHost* rfh =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  return WebContents::FromRenderFrameHost(rfh);
}

}  // namespace

ServiceWorkerDispatcherHost::ServiceWorkerDispatcherHost(
    int render_process_id,
    ResourceContext* resource_context)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      BrowserAssociatedInterface<mojom::ServiceWorkerDispatcherHost>(this,
                                                                     this),
      render_process_id_(render_process_id),
      resource_context_(resource_context),
      channel_ready_(false),
      weak_ptr_factory_(this) {}

ServiceWorkerDispatcherHost::~ServiceWorkerDispatcherHost() {
  // Temporary CHECK for debugging https://crbug.com/736203.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (GetContext() && phase_ == Phase::kAddedToContext)
    GetContext()->RemoveDispatcherHost(render_process_id_);
}

void ServiceWorkerDispatcherHost::Init(
    ServiceWorkerContextWrapper* context_wrapper) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ServiceWorkerDispatcherHost::Init, this,
                       base::RetainedRef(context_wrapper)));
    return;
  }

  // Just speculating that maybe we were destructed before Init() was called on
  // the IO thread in order to try to fix https://crbug.com/750267.
  if (phase_ != Phase::kInitial)
    return;

  context_wrapper_ = context_wrapper;
  if (!GetContext())
    return;
  GetContext()->AddDispatcherHost(render_process_id_, this);
  phase_ = Phase::kAddedToContext;
}

void ServiceWorkerDispatcherHost::OnFilterAdded(IPC::Channel* channel) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnFilterAdded");
  // Temporary CHECK for debugging https://crbug.com/736203.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  channel_ready_ = true;
  std::vector<std::unique_ptr<IPC::Message>> messages;
  messages.swap(pending_messages_);
  for (auto& message : messages) {
    BrowserMessageFilter::Send(message.release());
  }
}

void ServiceWorkerDispatcherHost::OnFilterRemoved() {
  // Temporary CHECK for debugging https://crbug.com/736203.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Don't wait until the destructor to teardown since a new dispatcher host
  // for this process might be created before then.
  if (GetContext() && phase_ == Phase::kAddedToContext) {
    GetContext()->RemoveDispatcherHost(render_process_id_);
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
  phase_ = Phase::kRemovedFromContext;
  context_wrapper_ = nullptr;
  channel_ready_ = false;
}

void ServiceWorkerDispatcherHost::OnDestruct() const {
  // Destruct on the IO thread since |context_wrapper_| should only be accessed
  // on the IO thread.
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool ServiceWorkerDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcherHost, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_UnregisterServiceWorker,
                        OnUnregisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessageToWorker,
                        OnPostMessageToWorker)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_CountFeature, OnCountFeature)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount,
                        OnIncrementServiceWorkerRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount,
                        OnDecrementServiceWorkerRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_TerminateWorker, OnTerminateWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_EnableNavigationPreload,
                        OnEnableNavigationPreload)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetNavigationPreloadState,
                        OnGetNavigationPreloadState)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SetNavigationPreloadHeader,
                        OnSetNavigationPreloadHeader)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled && GetContext()) {
    handled = GetContext()->embedded_worker_registry()->OnMessageReceived(
        message, render_process_id_);
    if (!handled)
      bad_message::ReceivedBadMessage(this, bad_message::SWDH_NOT_HANDLED);
  }

  return handled;
}

bool ServiceWorkerDispatcherHost::Send(IPC::Message* message) {
  if (channel_ready_) {
    BrowserMessageFilter::Send(message);
    // Don't bother passing through Send()'s result: it's not reliable.
    return true;
  }

  pending_messages_.push_back(base::WrapUnique(message));
  return true;
}

void ServiceWorkerDispatcherHost::RegisterServiceWorkerHandle(
    std::unique_ptr<ServiceWorkerHandle> handle) {
  int handle_id = handle->handle_id();
  handles_.AddWithID(std::move(handle), handle_id);
}

void ServiceWorkerDispatcherHost::RegisterServiceWorkerRegistrationHandle(
    ServiceWorkerRegistrationHandle* handle) {
  int handle_id = handle->handle_id();
  registration_handles_.AddWithID(base::WrapUnique(handle), handle_id);
}

void ServiceWorkerDispatcherHost::UnregisterServiceWorkerRegistrationHandle(
    int handle_id) {
  registration_handles_.Remove(handle_id);
}

ServiceWorkerHandle* ServiceWorkerDispatcherHost::FindServiceWorkerHandle(
    int provider_id,
    int64_t version_id) {
  for (base::IDMap<std::unique_ptr<ServiceWorkerHandle>>::iterator iter(
           &handles_);
       !iter.IsAtEnd(); iter.Advance()) {
    ServiceWorkerHandle* handle = iter.GetCurrentValue();
    DCHECK(handle);
    DCHECK(handle->version());
    if (handle->provider_id() == provider_id &&
        handle->version()->version_id() == version_id) {
      return handle;
    }
  }
  return nullptr;
}

blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
ServiceWorkerDispatcherHost::CreateRegistrationObjectInfo(
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration) {
  DCHECK(provider_host);
  ServiceWorkerRegistrationHandle* existing_handle =
      FindRegistrationHandle(provider_host->provider_id(), registration->id());
  if (existing_handle) {
    return existing_handle->CreateObjectInfo();
  }
  // ServiceWorkerRegistrationHandle ctor will register itself into
  // |registration_handles_|.
  auto* new_handle = new ServiceWorkerRegistrationHandle(
      GetContext()->AsWeakPtr(), this, provider_host, registration);
  return new_handle->CreateObjectInfo();
}

base::WeakPtr<ServiceWorkerDispatcherHost>
ServiceWorkerDispatcherHost::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ServiceWorkerDispatcherHost::OnUnregisterServiceWorker(
    int thread_id,
    int request_id,
    int provider_id,
    int64_t registration_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnUnregisterServiceWorker");
  ProviderStatus provider_status;
  ServiceWorkerProviderHost* provider_host =
      GetProviderHostForRequest(&provider_status, provider_id);
  switch (provider_status) {
    case ProviderStatus::NO_CONTEXT:  // fallthrough
    case ProviderStatus::DEAD_HOST:
      Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
          thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kAbort,
          base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
              base::ASCIIToUTF16(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(this,
                                      bad_message::SWDH_UNREGISTER_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
          thread_id, request_id,
          blink::mojom::ServiceWorkerErrorType::kSecurity,
          base::ASCIIToUTF16(kServiceWorkerUnregisterErrorPrefix) +
              base::ASCIIToUTF16(kNoDocumentURLErrorMessage)));
      return;
    case ProviderStatus::OK:
      break;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  if (!registration) {
    // |registration| must be alive because a renderer retains a registration
    // reference at this point.
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_UNREGISTER_BAD_REGISTRATION_ID);
    return;
  }

  std::vector<GURL> urls = {provider_host->document_url(),
                            registration->pattern()};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_UNREGISTER_CANNOT);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(), provider_host->topmost_frame_url(),
          resource_context_, base::Bind(&GetWebContents, render_process_id_,
                                        provider_host->frame_id()))) {
    Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
        thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kDisabled,
        base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1(
      "ServiceWorker", "ServiceWorkerDispatcherHost::UnregisterServiceWorker",
      request_id, "Scope", registration->pattern().spec());
  GetContext()->UnregisterServiceWorker(
      registration->pattern(),
      base::Bind(&ServiceWorkerDispatcherHost::UnregistrationComplete, this,
                 thread_id, request_id));
}

void ServiceWorkerDispatcherHost::OnEnableNavigationPreload(
    int thread_id,
    int request_id,
    int provider_id,
    int64_t registration_id,
    bool enable) {
  ProviderStatus provider_status;
  ServiceWorkerProviderHost* provider_host =
      GetProviderHostForRequest(&provider_status, provider_id);
  switch (provider_status) {
    case ProviderStatus::NO_CONTEXT:  // fallthrough
    case ProviderStatus::DEAD_HOST:
      Send(new ServiceWorkerMsg_EnableNavigationPreloadError(
          thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kAbort,
          std::string(kEnableNavigationPreloadErrorPrefix) +
              std::string(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_ENABLE_NAVIGATION_PRELOAD_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_EnableNavigationPreloadError(
          thread_id, request_id,
          blink::mojom::ServiceWorkerErrorType::kSecurity,
          std::string(kEnableNavigationPreloadErrorPrefix) +
              std::string(kNoDocumentURLErrorMessage)));
      return;
    case ProviderStatus::OK:
      break;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  if (!registration) {
    // |registration| must be alive because a renderer retains a registration
    // reference at this point.
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_ENABLE_NAVIGATION_PRELOAD_BAD_REGISTRATION_ID);
    return;
  }
  if (!registration->active_version()) {
    Send(new ServiceWorkerMsg_EnableNavigationPreloadError(
        thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kState,
        std::string(kEnableNavigationPreloadErrorPrefix) +
            std::string(kNoActiveWorkerErrorMessage)));
    return;
  }

  std::vector<GURL> urls = {provider_host->document_url(),
                            registration->pattern()};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_ENABLE_NAVIGATION_PRELOAD_INVALID_ORIGIN);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(), provider_host->topmost_frame_url(),
          resource_context_, base::Bind(&GetWebContents, render_process_id_,
                                        provider_host->frame_id()))) {
    Send(new ServiceWorkerMsg_EnableNavigationPreloadError(
        thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kDisabled,
        std::string(kEnableNavigationPreloadErrorPrefix) +
            std::string(kUserDeniedPermissionMessage)));
    return;
  }

  GetContext()->storage()->UpdateNavigationPreloadEnabled(
      registration->id(), registration->pattern().GetOrigin(), enable,
      base::Bind(
          &ServiceWorkerDispatcherHost::DidUpdateNavigationPreloadEnabled, this,
          thread_id, request_id, registration->id(), enable));
}

void ServiceWorkerDispatcherHost::OnGetNavigationPreloadState(
    int thread_id,
    int request_id,
    int provider_id,
    int64_t registration_id) {
  ProviderStatus provider_status;
  ServiceWorkerProviderHost* provider_host =
      GetProviderHostForRequest(&provider_status, provider_id);
  switch (provider_status) {
    case ProviderStatus::NO_CONTEXT:  // fallthrough
    case ProviderStatus::DEAD_HOST:
      Send(new ServiceWorkerMsg_GetNavigationPreloadStateError(
          thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kAbort,
          std::string(kGetNavigationPreloadStateErrorPrefix) +
              std::string(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_GET_NAVIGATION_PRELOAD_STATE_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_GetNavigationPreloadStateError(
          thread_id, request_id,
          blink::mojom::ServiceWorkerErrorType::kSecurity,
          std::string(kGetNavigationPreloadStateErrorPrefix) +
              std::string(kNoDocumentURLErrorMessage)));
      return;
    case ProviderStatus::OK:
      break;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  if (!registration) {
    // |registration| must be alive because a renderer retains a registration
    // reference at this point.
    bad_message::ReceivedBadMessage(
        this,
        bad_message::SWDH_GET_NAVIGATION_PRELOAD_STATE_BAD_REGISTRATION_ID);
    return;
  }

  std::vector<GURL> urls = {provider_host->document_url(),
                            registration->pattern()};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_GET_NAVIGATION_PRELOAD_STATE_INVALID_ORIGIN);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(), provider_host->topmost_frame_url(),
          resource_context_, base::Bind(&GetWebContents, render_process_id_,
                                        provider_host->frame_id()))) {
    Send(new ServiceWorkerMsg_GetNavigationPreloadStateError(
        thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kDisabled,
        std::string(kGetNavigationPreloadStateErrorPrefix) +
            std::string(kUserDeniedPermissionMessage)));
    return;
  }

  Send(new ServiceWorkerMsg_DidGetNavigationPreloadState(
      thread_id, request_id, registration->navigation_preload_state()));
}

void ServiceWorkerDispatcherHost::OnSetNavigationPreloadHeader(
    int thread_id,
    int request_id,
    int provider_id,
    int64_t registration_id,
    const std::string& value) {
  ProviderStatus provider_status;
  ServiceWorkerProviderHost* provider_host =
      GetProviderHostForRequest(&provider_status, provider_id);
  switch (provider_status) {
    case ProviderStatus::NO_CONTEXT:  // fallthrough
    case ProviderStatus::DEAD_HOST:
      Send(new ServiceWorkerMsg_SetNavigationPreloadHeaderError(
          thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kAbort,
          std::string(kSetNavigationPreloadHeaderErrorPrefix) +
              std::string(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_SET_NAVIGATION_PRELOAD_HEADER_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_SetNavigationPreloadHeaderError(
          thread_id, request_id,
          blink::mojom::ServiceWorkerErrorType::kSecurity,
          std::string(kSetNavigationPreloadHeaderErrorPrefix) +
              std::string(kNoDocumentURLErrorMessage)));
      return;
    case ProviderStatus::OK:
      break;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  if (!registration) {
    // |registration| must be alive because a renderer retains a registration
    // reference at this point.
    bad_message::ReceivedBadMessage(
        this,
        bad_message::SWDH_SET_NAVIGATION_PRELOAD_HEADER_BAD_REGISTRATION_ID);
    return;
  }
  if (!registration->active_version()) {
    Send(new ServiceWorkerMsg_SetNavigationPreloadHeaderError(
        thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kState,
        std::string(kSetNavigationPreloadHeaderErrorPrefix) +
            std::string(kNoActiveWorkerErrorMessage)));
    return;
  }

  std::vector<GURL> urls = {provider_host->document_url(),
                            registration->pattern()};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_SET_NAVIGATION_PRELOAD_HEADER_INVALID_ORIGIN);
    return;
  }

  // TODO(falken): Ideally this would match Blink's isValidHTTPHeaderValue.
  // Chrome's check is less restrictive: it allows non-latin1 characters.
  if (!net::HttpUtil::IsValidHeaderValue(value)) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_SET_NAVIGATION_PRELOAD_HEADER_BAD_VALUE);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(), provider_host->topmost_frame_url(),
          resource_context_, base::Bind(&GetWebContents, render_process_id_,
                                        provider_host->frame_id()))) {
    Send(new ServiceWorkerMsg_SetNavigationPreloadHeaderError(
        thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kDisabled,
        std::string(kSetNavigationPreloadHeaderErrorPrefix) +
            std::string(kUserDeniedPermissionMessage)));
    return;
  }

  GetContext()->storage()->UpdateNavigationPreloadHeader(
      registration->id(), registration->pattern().GetOrigin(), value,
      base::Bind(&ServiceWorkerDispatcherHost::DidUpdateNavigationPreloadHeader,
                 this, thread_id, request_id, registration->id(), value));
}

void ServiceWorkerDispatcherHost::OnPostMessageToWorker(
    int handle_id,
    int provider_id,
    const base::string16& message,
    const url::Origin& source_origin,
    const std::vector<MessagePortChannel>& sent_message_ports) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnPostMessageToWorker");
  if (!GetContext())
    return;

  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_POST_MESSAGE);
    return;
  }

  ServiceWorkerProviderHost* sender_provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!sender_provider_host) {
    // This may occur when destruction of the sender provider overtakes
    // postMessage() because of thread hopping on WebServiceWorkerImpl.
    return;
  }

  DispatchExtendableMessageEvent(
      base::WrapRefCounted(handle->version()), message, source_origin,
      sent_message_ports, sender_provider_host,
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

void ServiceWorkerDispatcherHost::DispatchExtendableMessageEvent(
    scoped_refptr<ServiceWorkerVersion> worker,
    const base::string16& message,
    const url::Origin& source_origin,
    const std::vector<MessagePortChannel>& sent_message_ports,
    ServiceWorkerProviderHost* sender_provider_host,
    const StatusCallback& callback) {
  switch (sender_provider_host->provider_type()) {
    case SERVICE_WORKER_PROVIDER_FOR_WINDOW:
    case SERVICE_WORKER_PROVIDER_FOR_WORKER:
    case SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      service_worker_client_utils::GetClient(
          sender_provider_host,
          base::Bind(&ServiceWorkerDispatcherHost::
                         DispatchExtendableMessageEventInternal<
                             ServiceWorkerClientInfo>,
                     this, worker, message, source_origin, sent_message_ports,
                     base::nullopt, callback));
      break;
    case SERVICE_WORKER_PROVIDER_FOR_CONTROLLER: {
      // Clamp timeout to the sending worker's remaining timeout, to prevent
      // postMessage from keeping workers alive forever.
      base::TimeDelta timeout =
          sender_provider_host->running_hosted_version()->remaining_timeout();
      RunSoon(base::Bind(
          &ServiceWorkerDispatcherHost::DispatchExtendableMessageEventInternal<
              ServiceWorkerObjectInfo>,
          this, worker, message, source_origin, sent_message_ports,
          base::make_optional(timeout), callback,
          sender_provider_host->GetOrCreateServiceWorkerHandle(
              sender_provider_host->running_hosted_version())));
      break;
    }
    case SERVICE_WORKER_PROVIDER_UNKNOWN:
    default:
      NOTREACHED() << sender_provider_host->provider_type();
      break;
  }
}

void ServiceWorkerDispatcherHost::OnProviderCreated(
    ServiceWorkerProviderHostInfo info) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnProviderCreated");
  if (!GetContext())
    return;
  if (GetContext()->GetProviderHost(render_process_id_, info.provider_id)) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_PROVIDER_CREATED_DUPLICATE_ID);
    return;
  }

  if (IsBrowserSideNavigationEnabled() &&
      ServiceWorkerUtils::IsBrowserAssignedProviderId(info.provider_id)) {
    std::unique_ptr<ServiceWorkerProviderHost> provider_host;
    // PlzNavigate
    // Retrieve the provider host previously created for navigation requests.
    ServiceWorkerNavigationHandleCore* navigation_handle_core =
        GetContext()->GetNavigationHandleCore(info.provider_id);
    if (navigation_handle_core != nullptr)
      provider_host = navigation_handle_core->RetrievePreCreatedHost();

    // If no host is found, create one.
    if (provider_host == nullptr) {
      GetContext()->AddProviderHost(ServiceWorkerProviderHost::Create(
          render_process_id_, std::move(info), GetContext()->AsWeakPtr(),
          AsWeakPtr()));
      return;
    }

    // Otherwise, completed the initialization of the pre-created host.
    if (info.type != SERVICE_WORKER_PROVIDER_FOR_WINDOW) {
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_PROVIDER_CREATED_ILLEGAL_TYPE_NOT_WINDOW);
      return;
    }
    provider_host->CompleteNavigationInitialized(render_process_id_,
                                                 std::move(info), AsWeakPtr());
    GetContext()->AddProviderHost(std::move(provider_host));
  } else {
    // Provider host for controller should be pre-created on StartWorker in
    // ServiceWorkerVersion.
    if (info.type == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER) {
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_PROVIDER_CREATED_ILLEGAL_TYPE_CONTROLLER);
      return;
    }
    if (ServiceWorkerUtils::IsBrowserAssignedProviderId(info.provider_id)) {
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_PROVIDER_CREATED_BAD_ID);
      return;
    }
    GetContext()->AddProviderHost(ServiceWorkerProviderHost::Create(
        render_process_id_, std::move(info), GetContext()->AsWeakPtr(),
        AsWeakPtr()));
  }
}

template <typename SourceInfo>
void ServiceWorkerDispatcherHost::DispatchExtendableMessageEventInternal(
    scoped_refptr<ServiceWorkerVersion> worker,
    const base::string16& message,
    const url::Origin& source_origin,
    const std::vector<MessagePortChannel>& sent_message_ports,
    const base::Optional<base::TimeDelta>& timeout,
    const StatusCallback& callback,
    const SourceInfo& source_info) {
  if (!source_info.IsValid()) {
    DidFailToDispatchExtendableMessageEvent<SourceInfo>(
        sent_message_ports, source_info, callback, SERVICE_WORKER_ERROR_FAILED);
    return;
  }

  // If not enough time is left to actually process the event don't even
  // bother starting the worker and sending the event.
  if (timeout && *timeout < base::TimeDelta::FromMilliseconds(100)) {
    DidFailToDispatchExtendableMessageEvent<SourceInfo>(
        sent_message_ports, source_info, callback,
        SERVICE_WORKER_ERROR_TIMEOUT);
    return;
  }

  worker->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::MESSAGE,
      base::BindOnce(&ServiceWorkerDispatcherHost::
                         DispatchExtendableMessageEventAfterStartWorker,
                     this, worker, message, source_origin, sent_message_ports,
                     ExtendableMessageEventSource(source_info), timeout,
                     callback),
      base::BindOnce(
          &ServiceWorkerDispatcherHost::DidFailToDispatchExtendableMessageEvent<
              SourceInfo>,
          this, sent_message_ports, source_info, callback));
}

void ServiceWorkerDispatcherHost::
    DispatchExtendableMessageEventAfterStartWorker(
        scoped_refptr<ServiceWorkerVersion> worker,
        const base::string16& message,
        const url::Origin& source_origin,
        const std::vector<MessagePortChannel>& sent_message_ports,
        const ExtendableMessageEventSource& source,
        const base::Optional<base::TimeDelta>& timeout,
        const StatusCallback& callback) {
  int request_id;
  if (timeout) {
    request_id = worker->StartRequestWithCustomTimeout(
        ServiceWorkerMetrics::EventType::MESSAGE, callback, *timeout,
        ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  } else {
    request_id = worker->StartRequest(ServiceWorkerMetrics::EventType::MESSAGE,
                                      callback);
  }

  mojom::ExtendableMessageEventPtr event = mojom::ExtendableMessageEvent::New();
  event->message = message;
  event->source_origin = source_origin;
  event->message_ports = MessagePortChannel::ReleaseHandles(sent_message_ports);
  event->source = source;

  // Hide the client url if the client has a unique origin.
  if (source_origin.unique()) {
    if (event->source.client_info.IsValid())
      event->source.client_info.url = GURL();
    else
      event->source.service_worker_info.url = GURL();
  }

  worker->event_dispatcher()->DispatchExtendableMessageEvent(
      std::move(event), worker->CreateSimpleEventCallback(request_id));
}

template <typename SourceInfo>
void ServiceWorkerDispatcherHost::DidFailToDispatchExtendableMessageEvent(
    const std::vector<MessagePortChannel>& sent_message_ports,
    const SourceInfo& source_info,
    const StatusCallback& callback,
    ServiceWorkerStatusCode status) {
  if (source_info.IsValid())
    ReleaseSourceInfo(source_info);
  callback.Run(status);
}

void ServiceWorkerDispatcherHost::ReleaseSourceInfo(
    const ServiceWorkerClientInfo& source_info) {
  // ServiceWorkerClientInfo is just a snapshot of the client. There is no need
  // to do anything for it.
}

void ServiceWorkerDispatcherHost::ReleaseSourceInfo(
    const ServiceWorkerObjectInfo& source_info) {
  ServiceWorkerHandle* handle = handles_.Lookup(source_info.handle_id);
  DCHECK(handle);
  handle->DecrementRefCount();
  if (handle->HasNoRefCount())
    handles_.Remove(source_info.handle_id);
}

ServiceWorkerRegistrationHandle*
ServiceWorkerDispatcherHost::FindRegistrationHandle(int provider_id,
                                                    int64_t registration_id) {
  for (RegistrationHandleMap::iterator iter(&registration_handles_);
       !iter.IsAtEnd(); iter.Advance()) {
    ServiceWorkerRegistrationHandle* handle = iter.GetCurrentValue();
    if (handle->provider_id() == provider_id &&
        handle->registration()->id() == registration_id) {
      return handle;
    }
  }
  return nullptr;
}

void ServiceWorkerDispatcherHost::GetRegistrationObjectInfoAndVersionAttributes(
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr* out_info,
    ServiceWorkerVersionAttributes* out_attrs) {
  *out_info = CreateRegistrationObjectInfo(provider_host, registration);

  out_attrs->installing = provider_host->GetOrCreateServiceWorkerHandle(
      registration->installing_version());
  out_attrs->waiting = provider_host->GetOrCreateServiceWorkerHandle(
      registration->waiting_version());
  out_attrs->active = provider_host->GetOrCreateServiceWorkerHandle(
      registration->active_version());
}

void ServiceWorkerDispatcherHost::OnCountFeature(int64_t version_id,
                                                 uint32_t feature) {
  if (!GetContext())
    return;
  ServiceWorkerVersion* version = GetContext()->GetLiveVersion(version_id);
  if (!version)
    return;
  version->CountFeature(feature);
}

void ServiceWorkerDispatcherHost::OnIncrementServiceWorkerRefCount(
    int handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnIncrementServiceWorkerRefCount");
  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_INCREMENT_WORKER_BAD_HANDLE);
    return;
  }
  handle->IncrementRefCount();
}

void ServiceWorkerDispatcherHost::OnDecrementServiceWorkerRefCount(
    int handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnDecrementServiceWorkerRefCount");
  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_DECREMENT_WORKER_BAD_HANDLE);
    return;
  }
  handle->DecrementRefCount();
  if (handle->HasNoRefCount())
    handles_.Remove(handle_id);
}

void ServiceWorkerDispatcherHost::UnregistrationComplete(
    int thread_id,
    int request_id,
    ServiceWorkerStatusCode status) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerDispatcherHost::UnregisterServiceWorker",
                         request_id, "Status", status);
  if (status != SERVICE_WORKER_OK && status != SERVICE_WORKER_ERROR_NOT_FOUND) {
    base::string16 error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, std::string(),
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerUnregisterErrorPrefix) +
            error_message));
    return;
  }
  const bool is_success = (status == SERVICE_WORKER_OK);
  Send(new ServiceWorkerMsg_ServiceWorkerUnregistered(thread_id,
                                                      request_id,
                                                      is_success));
}

ServiceWorkerContextCore* ServiceWorkerDispatcherHost::GetContext() {
  // Temporary CHECK for debugging https://crbug.com/736203.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!context_wrapper_.get())
    return nullptr;
  return context_wrapper_->context();
}

ServiceWorkerProviderHost*
ServiceWorkerDispatcherHost::GetProviderHostForRequest(ProviderStatus* status,
                                                       int provider_id) {
  if (!GetContext()) {
    *status = ProviderStatus::NO_CONTEXT;
    return nullptr;
  }

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host) {
    *status = ProviderStatus::NO_HOST;
    return nullptr;
  }

  if (!provider_host->IsContextAlive()) {
    *status = ProviderStatus::DEAD_HOST;
    return nullptr;
  }

  // TODO(falken): This check can be removed once crbug.com/439697 is fixed.
  if (provider_host->document_url().is_empty()) {
    *status = ProviderStatus::NO_URL;
    return nullptr;
  }

  *status = ProviderStatus::OK;
  return provider_host;
}

void ServiceWorkerDispatcherHost::DidUpdateNavigationPreloadEnabled(
    int thread_id,
    int request_id,
    int registration_id,
    bool enable,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    Send(new ServiceWorkerMsg_EnableNavigationPreloadError(
        thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kUnknown,
        std::string(kEnableNavigationPreloadErrorPrefix) +
            std::string(kDatabaseErrorMessage)));
    return;
  }
  if (!GetContext())
    return;
  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  if (registration)
    registration->EnableNavigationPreload(enable);
  Send(new ServiceWorkerMsg_DidEnableNavigationPreload(thread_id, request_id));
}

void ServiceWorkerDispatcherHost::DidUpdateNavigationPreloadHeader(
    int thread_id,
    int request_id,
    int registration_id,
    const std::string& value,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    Send(new ServiceWorkerMsg_SetNavigationPreloadHeaderError(
        thread_id, request_id, blink::mojom::ServiceWorkerErrorType::kUnknown,
        std::string(kSetNavigationPreloadHeaderErrorPrefix) +
            std::string(kDatabaseErrorMessage)));
    return;
  }
  if (!GetContext())
    return;
  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  if (registration)
    registration->SetNavigationPreloadHeader(value);
  Send(new ServiceWorkerMsg_DidSetNavigationPreloadHeader(thread_id,
                                                          request_id));
}

void ServiceWorkerDispatcherHost::OnTerminateWorker(int handle_id) {
  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::SWDH_TERMINATE_BAD_HANDLE);
    return;
  }
  handle->version()->StopWorker(base::BindOnce(&base::DoNothing));
}

}  // namespace content
