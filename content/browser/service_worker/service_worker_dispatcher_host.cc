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
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/web_feature.mojom.h"
#include "url/gurl.h"

using blink::MessagePortChannel;
using blink::WebServiceWorkerError;

namespace content {

namespace {

const uint32_t kServiceWorkerFilteredMessageClasses[] = {
    ServiceWorkerMsgStart, EmbeddedWorkerMsgStart,
};

}  // namespace

ServiceWorkerDispatcherHost::ServiceWorkerDispatcherHost(
    int render_process_id,
    ResourceContext* resource_context)
    : BrowserMessageFilter(kServiceWorkerFilteredMessageClasses,
                           arraysize(kServiceWorkerFilteredMessageClasses)),
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
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessageToWorker,
                        OnPostMessageToWorker)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_CountFeature, OnCountFeature)
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

void ServiceWorkerDispatcherHost::UnregisterServiceWorkerHandle(int handle_id) {
  handles_.Remove(handle_id);
}

base::WeakPtr<ServiceWorkerDispatcherHost>
ServiceWorkerDispatcherHost::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ServiceWorkerDispatcherHost::OnPostMessageToWorker(
    int handle_id,
    int provider_id,
    const scoped_refptr<base::RefCountedData<blink::TransferableMessage>>&
        message,
    const url::Origin& source_origin) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnPostMessageToWorker");
  if (!GetContext())
    return;

  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_POST_MESSAGE);
    return;
  }
  // TODO(leonhsl): Remove this unnecessary |provider_id| when mojofying
  // ServiceWorkerHostMsg_PostMessageToWorker later.
  DCHECK_EQ(provider_id, handle->provider_id());

  // When this method is called the encoded_message inside message could just
  // point to the IPC message's buffer. But that buffer can become invalid
  // before the message is passed on to the service worker, so make sure
  // message owns its data.
  message->data.EnsureDataIsOwned();

  DispatchExtendableMessageEvent(
      base::WrapRefCounted(handle->version()), std::move(message->data),
      source_origin, handle->provider_id(),
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
}

void ServiceWorkerDispatcherHost::DispatchExtendableMessageEvent(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    int source_provider_id,
    StatusCallback callback) {
  ServiceWorkerProviderHost* source_provider_host =
      GetContext()->GetProviderHost(render_process_id_, source_provider_id);
  if (!source_provider_host) {
    // This may occur when destruction of the source provider overtakes
    // postMessage() because of thread hopping on WebServiceWorkerImpl.
    return;
  }

  switch (source_provider_host->provider_type()) {
    case blink::mojom::ServiceWorkerProviderType::kForWindow:
    case blink::mojom::ServiceWorkerProviderType::kForSharedWorker:
      service_worker_client_utils::GetClient(
          source_provider_host,
          base::BindOnce(&ServiceWorkerDispatcherHost::
                             DispatchExtendableMessageEventInternal,
                         this, worker, std::move(message), source_origin,
                         base::nullopt, std::move(callback),
                         kInvalidServiceWorkerProviderId));
      break;
    case blink::mojom::ServiceWorkerProviderType::kForServiceWorker: {
      // Clamp timeout to the sending worker's remaining timeout, to prevent
      // postMessage from keeping workers alive forever.
      base::TimeDelta timeout =
          source_provider_host->running_hosted_version()->remaining_timeout();

      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&ServiceWorkerDispatcherHost::
                             DispatchExtendableMessageEventInternal,
                         this, worker, std::move(message), source_origin,
                         base::make_optional(timeout), std::move(callback),
                         source_provider_id, nullptr /* source_client_info */));
      break;
    }
    case blink::mojom::ServiceWorkerProviderType::kUnknown:
    default:
      NOTREACHED() << source_provider_host->provider_type();
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
    if (info.type != blink::mojom::ServiceWorkerProviderType::kForWindow) {
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_PROVIDER_CREATED_ILLEGAL_TYPE_NOT_WINDOW);
      return;
    }
    provider_host->CompleteNavigationInitialized(render_process_id_,
                                                 std::move(info), AsWeakPtr());
    GetContext()->AddProviderHost(std::move(provider_host));
  } else {
    // Provider hosts for service workers should be pre-created in StartWorker
    // in ServiceWorkerVersion.
    if (info.type ==
        blink::mojom::ServiceWorkerProviderType::kForServiceWorker) {
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

void ServiceWorkerDispatcherHost::DispatchExtendableMessageEventInternal(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const base::Optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    int source_service_worker_provider_id,
    blink::mojom::ServiceWorkerClientInfoPtr source_client_info) {
  DCHECK(source_service_worker_provider_id != kInvalidServiceWorkerProviderId ||
         source_client_info);
  if (source_service_worker_provider_id == kInvalidServiceWorkerProviderId) {
    DCHECK(source_client_info);
    if (source_client_info->client_uuid.empty()) {
      std::move(callback).Run(SERVICE_WORKER_ERROR_FAILED);
      return;
    }
  }

  // If not enough time is left to actually process the event don't even
  // bother starting the worker and sending the event.
  if (timeout && *timeout < base::TimeDelta::FromMilliseconds(100)) {
    std::move(callback).Run(SERVICE_WORKER_ERROR_TIMEOUT);
    return;
  }

  worker->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::MESSAGE,
      base::BindOnce(&ServiceWorkerDispatcherHost::
                         DispatchExtendableMessageEventAfterStartWorker,
                     this, worker, std::move(message), source_origin,
                     source_service_worker_provider_id,
                     std::move(source_client_info), timeout,
                     std::move(callback)));
}

void ServiceWorkerDispatcherHost::
    DispatchExtendableMessageEventAfterStartWorker(
        scoped_refptr<ServiceWorkerVersion> worker,
        blink::TransferableMessage message,
        const url::Origin& source_origin,
        int source_service_worker_provider_id,
        blink::mojom::ServiceWorkerClientInfoPtr source_client_info,
        const base::Optional<base::TimeDelta>& timeout,
        StatusCallback callback,
        ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != SERVICE_WORKER_OK) {
    std::move(callback).Run(start_worker_status);
    return;
  }
  if (!GetContext()) {
    std::move(callback).Run(SERVICE_WORKER_ERROR_FAILED);
    return;
  }

  mojom::ExtendableMessageEventPtr event = mojom::ExtendableMessageEvent::New();
  event->message = std::move(message);
  event->source_origin = source_origin;
  if (source_service_worker_provider_id != kInvalidServiceWorkerProviderId) {
    DCHECK(!source_client_info);
    // The source is a service worker global scope.
    ServiceWorkerProviderHost* source_service_worker_provider_host =
        GetContext()->GetProviderHost(render_process_id_,
                                      source_service_worker_provider_id);
    if (!source_service_worker_provider_host) {
      std::move(callback).Run(SERVICE_WORKER_ERROR_FAILED);
      return;
    }
    DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
              source_service_worker_provider_host->provider_type());
    blink::mojom::ServiceWorkerObjectInfoPtr source_worker_info =
        worker->provider_host()->GetOrCreateServiceWorkerHandle(
            source_service_worker_provider_host->running_hosted_version());

    event->source_info_for_service_worker = std::move(source_worker_info);
    // Hide the service worker url if the service worker has a unique origin.
    if (event->source_origin.unique())
      event->source_info_for_service_worker->url = GURL();
  } else {
    // The source is a service worker client.
    DCHECK(source_client_info && !source_client_info->client_uuid.empty());
    event->source_info_for_client = std::move(source_client_info);
    // Hide the client url if the client has a unique origin.
    if (event->source_origin.unique())
      event->source_info_for_client->url = GURL();
  }

  int request_id;
  if (timeout) {
    request_id = worker->StartRequestWithCustomTimeout(
        ServiceWorkerMetrics::EventType::MESSAGE, std::move(callback), *timeout,
        ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  } else {
    request_id = worker->StartRequest(ServiceWorkerMetrics::EventType::MESSAGE,
                                      std::move(callback));
  }
  worker->event_dispatcher()->DispatchExtendableMessageEvent(
      std::move(event), worker->CreateSimpleEventCallback(request_id));
}

void ServiceWorkerDispatcherHost::OnCountFeature(int64_t version_id,
                                                 uint32_t feature) {
  if (!GetContext())
    return;
  ServiceWorkerVersion* version = GetContext()->GetLiveVersion(version_id);
  if (!version)
    return;
  if (feature >=
      static_cast<uint32_t>(blink::mojom::WebFeature::kNumberOfFeatures)) {
    // We don't use BadMessageReceived here since this IPC will be converted to
    // a Mojo method call soon, which will validate inputs for us.
    // TODO(xiaofeng.zhang): Convert the OnCountFeature IPC into a Mojo method
    // call.
    return;
  }
  version->CountFeature(feature);
}

ServiceWorkerContextCore* ServiceWorkerDispatcherHost::GetContext() {
  // Temporary CHECK for debugging https://crbug.com/736203.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!context_wrapper_.get())
    return nullptr;
  return context_wrapper_->context();
}

}  // namespace content
