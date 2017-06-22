// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include <utility>

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/scoped_tracker.h"
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
#include "url/gurl.h"

using blink::WebServiceWorkerError;

namespace content {

namespace {

const char kNoDocumentURLErrorMessage[] =
    "No URL is associated with the caller's document.";
const char kShutdownErrorMessage[] =
    "The Service Worker system has shutdown.";
const char kUserDeniedPermissionMessage[] =
    "The user denied permission to use Service Worker.";
const char kInvalidStateErrorMessage[] = "The object is in an invalid state.";
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
      render_process_id_(render_process_id),
      resource_context_(resource_context),
      channel_ready_(false),
      weak_factory_(this) {
  AddAssociatedInterface(
      mojom::ServiceWorkerDispatcherHost::Name_,
      base::Bind(&ServiceWorkerDispatcherHost::AddMojoBinding,
                 base::Unretained(this)));
}

ServiceWorkerDispatcherHost::~ServiceWorkerDispatcherHost() {
  if (GetContext())
    GetContext()->RemoveDispatcherHost(render_process_id_);
}

void ServiceWorkerDispatcherHost::Init(
    ServiceWorkerContextWrapper* context_wrapper) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&ServiceWorkerDispatcherHost::Init, this,
                                       base::RetainedRef(context_wrapper)));
    return;
  }

  context_wrapper_ = context_wrapper;
  if (!GetContext())
    return;
  GetContext()->AddDispatcherHost(render_process_id_, this);
}

void ServiceWorkerDispatcherHost::OnFilterAdded(IPC::Channel* channel) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnFilterAdded");
  channel_ready_ = true;
  std::vector<std::unique_ptr<IPC::Message>> messages;
  messages.swap(pending_messages_);
  for (auto& message : messages) {
    BrowserMessageFilter::Send(message.release());
  }
}

void ServiceWorkerDispatcherHost::OnFilterRemoved() {
  // Don't wait until the destructor to teardown since a new dispatcher host
  // for this process might be created before then.
  if (GetContext())
    GetContext()->RemoveDispatcherHost(render_process_id_);
  context_wrapper_ = nullptr;
  channel_ready_ = false;
}

void ServiceWorkerDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool ServiceWorkerDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcherHost, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_RegisterServiceWorker,
                        OnRegisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_UpdateServiceWorker,
                        OnUpdateServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_UnregisterServiceWorker,
                        OnUnregisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetRegistration,
                        OnGetRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetRegistrations,
                        OnGetRegistrations)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetRegistrationForReady,
                        OnGetRegistrationForReady)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessageToWorker,
                        OnPostMessageToWorker)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_CountFeature, OnCountFeature)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount,
                        OnIncrementServiceWorkerRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount,
                        OnDecrementServiceWorkerRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_IncrementRegistrationRefCount,
                        OnIncrementRegistrationRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_DecrementRegistrationRefCount,
                        OnDecrementRegistrationRefCount)
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

void ServiceWorkerDispatcherHost::AddMojoBinding(
    mojo::ScopedInterfaceEndpointHandle handle) {
  bindings_.AddBinding(
      this,
      mojom::ServiceWorkerDispatcherHostAssociatedRequest(std::move(handle)));
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
    std::unique_ptr<ServiceWorkerRegistrationHandle> handle) {
  int handle_id = handle->handle_id();
  registration_handles_.AddWithID(std::move(handle), handle_id);
}

ServiceWorkerHandle* ServiceWorkerDispatcherHost::FindServiceWorkerHandle(
    int provider_id,
    int64_t version_id) {
  for (IDMap<std::unique_ptr<ServiceWorkerHandle>>::iterator iter(&handles_);
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

ServiceWorkerRegistrationHandle*
ServiceWorkerDispatcherHost::GetOrCreateRegistrationHandle(
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration) {
  DCHECK(provider_host);
  ServiceWorkerRegistrationHandle* existing_handle =
      FindRegistrationHandle(provider_host->provider_id(), registration->id());
  if (existing_handle) {
    existing_handle->IncrementRefCount();
    return existing_handle;
  }
  std::unique_ptr<ServiceWorkerRegistrationHandle> new_handle(
      new ServiceWorkerRegistrationHandle(GetContext()->AsWeakPtr(),
                                          provider_host, registration));
  ServiceWorkerRegistrationHandle* new_handle_ptr = new_handle.get();
  RegisterServiceWorkerRegistrationHandle(std::move(new_handle));
  return new_handle_ptr;
}

void ServiceWorkerDispatcherHost::OnRegisterServiceWorker(
    int thread_id,
    int request_id,
    int provider_id,
    const GURL& pattern,
    const GURL& script_url) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnRegisterServiceWorker");
  ProviderStatus provider_status;
  ServiceWorkerProviderHost* provider_host =
      GetProviderHostForRequest(&provider_status, provider_id);
  switch (provider_status) {
    case ProviderStatus::NO_CONTEXT:  // fallthrough
    case ProviderStatus::DEAD_HOST:
      Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeAbort,
          base::ASCIIToUTF16(kServiceWorkerRegisterErrorPrefix) +
              base::ASCIIToUTF16(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(this, bad_message::SWDH_REGISTER_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeSecurity,
          base::ASCIIToUTF16(kServiceWorkerRegisterErrorPrefix) +
              base::ASCIIToUTF16(kNoDocumentURLErrorMessage)));
      return;
    case ProviderStatus::OK:
      break;
  }

  if (!pattern.is_valid() || !script_url.is_valid()) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_REGISTER_BAD_URL);
    return;
  }

  std::string error_message;
  if (ServiceWorkerUtils::ContainsDisallowedCharacter(pattern, script_url,
                                                      &error_message)) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_REGISTER_CANNOT);
    return;
  }

  std::vector<GURL> urls = {provider_host->document_url(), pattern, script_url};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_REGISTER_CANNOT);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          pattern, provider_host->topmost_frame_url(), resource_context_,
          base::Bind(&GetWebContents, render_process_id_,
                     provider_host->frame_id()))) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id, request_id, WebServiceWorkerError::kErrorTypeDisabled,
        base::ASCIIToUTF16(kServiceWorkerRegisterErrorPrefix) +
            base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerDispatcherHost::RegisterServiceWorker",
      request_id, "Scope", pattern.spec(), "Script URL", script_url.spec());
  GetContext()->RegisterServiceWorker(
      pattern,
      script_url,
      provider_host,
      base::Bind(&ServiceWorkerDispatcherHost::RegistrationComplete,
                 this,
                 thread_id,
                 provider_id,
                 request_id));
}

void ServiceWorkerDispatcherHost::OnUpdateServiceWorker(
    int thread_id,
    int request_id,
    int provider_id,
    int64_t registration_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnUpdateServiceWorker");
  ProviderStatus provider_status;
  ServiceWorkerProviderHost* provider_host =
      GetProviderHostForRequest(&provider_status, provider_id);
  switch (provider_status) {
    case ProviderStatus::NO_CONTEXT:  // fallthrough
    case ProviderStatus::DEAD_HOST:
      Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeAbort,
          base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
              base::ASCIIToUTF16(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(this, bad_message::SWDH_UPDATE_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeSecurity,
          base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
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
        this, bad_message::SWDH_UPDATE_BAD_REGISTRATION_ID);
    return;
  }

  std::vector<GURL> urls = {provider_host->document_url(),
                            registration->pattern()};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_UPDATE_CANNOT);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(), provider_host->topmost_frame_url(),
          resource_context_, base::Bind(&GetWebContents, render_process_id_,
                                        provider_host->frame_id()))) {
    Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
        thread_id, request_id, WebServiceWorkerError::kErrorTypeDisabled,
        base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
            base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  if (!registration->GetNewestVersion()) {
    // This can happen if update() is called during initial script evaluation.
    // Abort the following steps according to the spec.
    Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
        thread_id, request_id, WebServiceWorkerError::kErrorTypeState,
        base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
            base::ASCIIToUTF16(kInvalidStateErrorMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcherHost::UpdateServiceWorker",
                           request_id, "Scope", registration->pattern().spec());
  GetContext()->UpdateServiceWorker(
      registration, false /* force_bypass_cache */,
      false /* skip_script_comparison */, provider_host,
      base::Bind(&ServiceWorkerDispatcherHost::UpdateComplete, this, thread_id,
                 provider_id, request_id));
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
          thread_id, request_id, WebServiceWorkerError::kErrorTypeAbort,
          base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
              base::ASCIIToUTF16(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(this,
                                      bad_message::SWDH_UNREGISTER_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeSecurity,
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
        thread_id, request_id, WebServiceWorkerError::kErrorTypeDisabled,
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

void ServiceWorkerDispatcherHost::OnGetRegistration(
    int thread_id,
    int request_id,
    int provider_id,
    const GURL& document_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnGetRegistration");

  ProviderStatus provider_status;
  ServiceWorkerProviderHost* provider_host =
      GetProviderHostForRequest(&provider_status, provider_id);
  switch (provider_status) {
    case ProviderStatus::NO_CONTEXT:  // fallthrough
    case ProviderStatus::DEAD_HOST:
      Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationError(
          thread_id, request_id, blink::WebServiceWorkerError::kErrorTypeAbort,
          base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
              base::ASCIIToUTF16(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_GET_REGISTRATION_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeSecurity,
          base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
              base::ASCIIToUTF16(kNoDocumentURLErrorMessage)));
      return;
    case ProviderStatus::OK:
      break;
  }

  if (!document_url.is_valid()) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::SWDH_GET_REGISTRATION_BAD_URL);
    return;
  }

  std::vector<GURL> urls = {provider_host->document_url(), document_url};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::SWDH_GET_REGISTRATION_CANNOT);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          provider_host->document_url(), provider_host->topmost_frame_url(),
          resource_context_, base::Bind(&GetWebContents, render_process_id_,
                                        provider_host->frame_id()))) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationError(
        thread_id, request_id, WebServiceWorkerError::kErrorTypeDisabled,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
            base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcherHost::GetRegistration",
                           request_id, "Document URL", document_url.spec());
  GetContext()->storage()->FindRegistrationForDocument(
      document_url,
      base::Bind(&ServiceWorkerDispatcherHost::GetRegistrationComplete,
                 this,
                 thread_id,
                 provider_id,
                 request_id));
}

void ServiceWorkerDispatcherHost::OnGetRegistrations(int thread_id,
                                                     int request_id,
                                                     int provider_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnGetRegistrations");

  ProviderStatus provider_status;
  ServiceWorkerProviderHost* provider_host =
      GetProviderHostForRequest(&provider_status, provider_id);
  switch (provider_status) {
    case ProviderStatus::NO_CONTEXT:  // fallthrough
    case ProviderStatus::DEAD_HOST:
      Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationsError(
          thread_id, request_id, blink::WebServiceWorkerError::kErrorTypeAbort,
          base::ASCIIToUTF16(kServiceWorkerGetRegistrationsErrorPrefix) +
              base::ASCIIToUTF16(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_GET_REGISTRATIONS_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationsError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeSecurity,
          base::ASCIIToUTF16(kServiceWorkerGetRegistrationsErrorPrefix) +
              base::ASCIIToUTF16(kNoDocumentURLErrorMessage)));
      return;
    case ProviderStatus::OK:
      break;
  }

  if (!OriginCanAccessServiceWorkers(provider_host->document_url())) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_GET_REGISTRATIONS_INVALID_ORIGIN);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          provider_host->document_url(), provider_host->topmost_frame_url(),
          resource_context_, base::Bind(&GetWebContents, render_process_id_,
                                        provider_host->frame_id()))) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationsError(
        thread_id, request_id, WebServiceWorkerError::kErrorTypeDisabled,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationsErrorPrefix) +
            base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerDispatcherHost::GetRegistrations",
                           request_id);
  GetContext()->storage()->GetRegistrationsForOrigin(
      provider_host->document_url().GetOrigin(),
      base::Bind(&ServiceWorkerDispatcherHost::GetRegistrationsComplete, this,
                 thread_id, provider_id, request_id));
}

void ServiceWorkerDispatcherHost::OnGetRegistrationForReady(
    int thread_id,
    int request_id,
    int provider_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnGetRegistrationForReady");
  if (!GetContext())
    return;
  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_GET_REGISTRATION_FOR_READY_NO_HOST);
    return;
  }
  if (!provider_host->IsContextAlive())
    return;

  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerDispatcherHost::GetRegistrationForReady",
      request_id);
  if (!provider_host->GetRegistrationForReady(base::Bind(
          &ServiceWorkerDispatcherHost::GetRegistrationForReadyComplete,
          this, thread_id, request_id, provider_host->AsWeakPtr()))) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_GET_REGISTRATION_FOR_READY_ALREADY_IN_PROGRESS);
  }
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
          thread_id, request_id, WebServiceWorkerError::kErrorTypeAbort,
          std::string(kEnableNavigationPreloadErrorPrefix) +
              std::string(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_ENABLE_NAVIGATION_PRELOAD_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_EnableNavigationPreloadError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeSecurity,
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
  // The spec discussion consensus is to reject if there is no active worker:
  // https://github.com/w3c/ServiceWorker/issues/920#issuecomment-262212670
  // TODO(falken): Remove this comment when the spec is updated.
  if (!registration->active_version()) {
    Send(new ServiceWorkerMsg_EnableNavigationPreloadError(
        thread_id, request_id, WebServiceWorkerError::kErrorTypeState,
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
        thread_id, request_id, WebServiceWorkerError::kErrorTypeDisabled,
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
          thread_id, request_id, WebServiceWorkerError::kErrorTypeAbort,
          std::string(kGetNavigationPreloadStateErrorPrefix) +
              std::string(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_GET_NAVIGATION_PRELOAD_STATE_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_GetNavigationPreloadStateError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeSecurity,
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
        thread_id, request_id, WebServiceWorkerError::kErrorTypeDisabled,
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
          thread_id, request_id, WebServiceWorkerError::kErrorTypeAbort,
          std::string(kSetNavigationPreloadHeaderErrorPrefix) +
              std::string(kShutdownErrorMessage)));
      return;
    case ProviderStatus::NO_HOST:
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_SET_NAVIGATION_PRELOAD_HEADER_NO_HOST);
      return;
    case ProviderStatus::NO_URL:
      Send(new ServiceWorkerMsg_SetNavigationPreloadHeaderError(
          thread_id, request_id, WebServiceWorkerError::kErrorTypeSecurity,
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
  // The spec discussion consensus is to reject if there is no active worker:
  // https://github.com/w3c/ServiceWorker/issues/920#issuecomment-262212670
  // TODO(falken): Remove this comment when the spec is updated.
  if (!registration->active_version()) {
    Send(new ServiceWorkerMsg_SetNavigationPreloadHeaderError(
        thread_id, request_id, WebServiceWorkerError::kErrorTypeState,
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
        thread_id, request_id, WebServiceWorkerError::kErrorTypeDisabled,
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
    const std::vector<MessagePort>& sent_message_ports) {
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
      make_scoped_refptr(handle->version()), message, source_origin,
      sent_message_ports, sender_provider_host,
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

void ServiceWorkerDispatcherHost::DispatchExtendableMessageEvent(
    scoped_refptr<ServiceWorkerVersion> worker,
    const base::string16& message,
    const url::Origin& source_origin,
    const std::vector<MessagePort>& sent_message_ports,
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
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "477117 ServiceWorkerDispatcherHost::OnProviderCreated"));
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnProviderCreated");
  if (!GetContext())
    return;
  if (GetContext()->GetProviderHost(render_process_id_, info.provider_id)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::SWDH_PROVIDER_CREATED_NO_HOST);
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
      GetContext()->AddProviderHost(
          ServiceWorkerProviderHost::Create(render_process_id_, std::move(info),
                                            GetContext()->AsWeakPtr(), this));
      return;
    }

    // Otherwise, completed the initialization of the pre-created host.
    if (info.type != SERVICE_WORKER_PROVIDER_FOR_WINDOW) {
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_PROVIDER_CREATED_ILLEGAL_TYPE);
      return;
    }
    provider_host->CompleteNavigationInitialized(render_process_id_,
                                                 std::move(info), this);
    GetContext()->AddProviderHost(std::move(provider_host));
  } else {
    // Provider host for controller should be pre-created on StartWorker in
    // ServiceWorkerVersion.
    if (info.type == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER) {
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_PROVIDER_CREATED_ILLEGAL_TYPE);
      return;
    }
    if (ServiceWorkerUtils::IsBrowserAssignedProviderId(info.provider_id)) {
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_PROVIDER_CREATED_NO_HOST);
      return;
    }
    GetContext()->AddProviderHost(ServiceWorkerProviderHost::Create(
        render_process_id_, std::move(info), GetContext()->AsWeakPtr(), this));
  }
}

template <typename SourceInfo>
void ServiceWorkerDispatcherHost::DispatchExtendableMessageEventInternal(
    scoped_refptr<ServiceWorkerVersion> worker,
    const base::string16& message,
    const url::Origin& source_origin,
    const std::vector<MessagePort>& sent_message_ports,
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
      base::Bind(&ServiceWorkerDispatcherHost::
                     DispatchExtendableMessageEventAfterStartWorker,
                 this, worker, message, source_origin, sent_message_ports,
                 ExtendableMessageEventSource(source_info), timeout, callback),
      base::Bind(
          &ServiceWorkerDispatcherHost::DidFailToDispatchExtendableMessageEvent<
              SourceInfo>,
          this, sent_message_ports, source_info, callback));
}

void ServiceWorkerDispatcherHost::
    DispatchExtendableMessageEventAfterStartWorker(
        scoped_refptr<ServiceWorkerVersion> worker,
        const base::string16& message,
        const url::Origin& source_origin,
        const std::vector<MessagePort>& sent_message_ports,
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
  event->message_ports = MessagePort::ReleaseHandles(sent_message_ports);
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
    const std::vector<MessagePort>& sent_message_ports,
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
    ServiceWorkerRegistrationObjectInfo* out_info,
    ServiceWorkerVersionAttributes* out_attrs) {
  ServiceWorkerRegistrationHandle* handle =
      GetOrCreateRegistrationHandle(provider_host, registration);
  *out_info = handle->GetObjectInfo();

  out_attrs->installing = provider_host->GetOrCreateServiceWorkerHandle(
      registration->installing_version());
  out_attrs->waiting = provider_host->GetOrCreateServiceWorkerHandle(
      registration->waiting_version());
  out_attrs->active = provider_host->GetOrCreateServiceWorkerHandle(
      registration->active_version());
}

void ServiceWorkerDispatcherHost::RegistrationComplete(
    int thread_id,
    int provider_id,
    int request_id,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerDispatcherHost::RegisterServiceWorker",
      request_id, "Status", status, "Registration ID", registration_id);
  if (!GetContext())
    return;

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;  // The provider has already been destroyed.

  if (status != SERVICE_WORKER_OK) {
    base::string16 error_message;
    blink::WebServiceWorkerError::ErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, status_message,
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerRegisterErrorPrefix) + error_message));
    return;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  DCHECK(registration);

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  GetRegistrationObjectInfoAndVersionAttributes(
      provider_host->AsWeakPtr(), registration, &info, &attrs);

  Send(new ServiceWorkerMsg_ServiceWorkerRegistered(
      thread_id, request_id, info, attrs));
}

void ServiceWorkerDispatcherHost::UpdateComplete(
    int thread_id,
    int provider_id,
    int request_id,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerDispatcherHost::UpdateServiceWorker",
      request_id, "Status", status, "Registration ID", registration_id);
  if (!GetContext())
    return;

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;  // The provider has already been destroyed.

  if (status != SERVICE_WORKER_OK) {
    base::string16 error_message;
    blink::WebServiceWorkerError::ErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, status_message,
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) + error_message));
    return;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  DCHECK(registration);

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  GetRegistrationObjectInfoAndVersionAttributes(provider_host->AsWeakPtr(),
                                                registration, &info, &attrs);

  Send(new ServiceWorkerMsg_ServiceWorkerUpdated(thread_id, request_id));
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

void ServiceWorkerDispatcherHost::OnIncrementRegistrationRefCount(
    int registration_handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnIncrementRegistrationRefCount");
  ServiceWorkerRegistrationHandle* handle =
      registration_handles_.Lookup(registration_handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_INCREMENT_REGISTRATION_BAD_HANDLE);
    return;
  }
  handle->IncrementRefCount();
}

void ServiceWorkerDispatcherHost::OnDecrementRegistrationRefCount(
    int registration_handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnDecrementRegistrationRefCount");
  ServiceWorkerRegistrationHandle* handle =
      registration_handles_.Lookup(registration_handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_DECREMENT_REGISTRATION_BAD_HANDLE);
    return;
  }
  handle->DecrementRefCount();
  if (handle->HasNoRefCount())
    registration_handles_.Remove(registration_handle_id);
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
    blink::WebServiceWorkerError::ErrorType error_type;
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

void ServiceWorkerDispatcherHost::GetRegistrationComplete(
    int thread_id,
    int provider_id,
    int request_id,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerDispatcherHost::GetRegistration",
      request_id, "Status", status, "Registration ID",
      registration ? registration->id() : kInvalidServiceWorkerRegistrationId);
  if (!GetContext())
    return;

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;  // The provider has already been destroyed.

  if (status != SERVICE_WORKER_OK && status != SERVICE_WORKER_ERROR_NOT_FOUND) {
    base::string16 error_message;
    blink::WebServiceWorkerError::ErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, std::string(),
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
            error_message));

    return;
  }

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  if (status == SERVICE_WORKER_OK) {
    DCHECK(registration.get());
    if (!registration->is_uninstalling()) {
      GetRegistrationObjectInfoAndVersionAttributes(
          provider_host->AsWeakPtr(), registration.get(), &info, &attrs);
    }
  }

  Send(new ServiceWorkerMsg_DidGetRegistration(
      thread_id, request_id, info, attrs));
}

void ServiceWorkerDispatcherHost::GetRegistrationsComplete(
    int thread_id,
    int provider_id,
    int request_id,
    ServiceWorkerStatusCode status,
    const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
        registrations) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerDispatcherHost::GetRegistrations",
                         request_id, "Status", status);
  if (!GetContext())
    return;

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;  // The provider has already been destroyed.

  if (status != SERVICE_WORKER_OK) {
    base::string16 error_message;
    blink::WebServiceWorkerError::ErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, std::string(),
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationsError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
            error_message));
    return;
  }

  std::vector<ServiceWorkerRegistrationObjectInfo> object_infos;
  std::vector<ServiceWorkerVersionAttributes> version_attrs;

  for (const auto& registration : registrations) {
    DCHECK(registration.get());
    if (!registration->is_uninstalling()) {
      ServiceWorkerRegistrationObjectInfo object_info;
      ServiceWorkerVersionAttributes version_attr;
      GetRegistrationObjectInfoAndVersionAttributes(
          provider_host->AsWeakPtr(), registration.get(), &object_info,
          &version_attr);
      object_infos.push_back(object_info);
      version_attrs.push_back(version_attr);
    }
  }

  Send(new ServiceWorkerMsg_DidGetRegistrations(thread_id, request_id,
                                                object_infos, version_attrs));
}

void ServiceWorkerDispatcherHost::GetRegistrationForReadyComplete(
    int thread_id,
    int request_id,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration) {
  DCHECK(registration);
  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerDispatcherHost::GetRegistrationForReady",
      request_id, "Registration ID",
      registration ? registration->id() : kInvalidServiceWorkerRegistrationId);
  if (!GetContext())
    return;

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  GetRegistrationObjectInfoAndVersionAttributes(
      provider_host, registration, &info, &attrs);
  Send(new ServiceWorkerMsg_DidGetRegistrationForReady(
        thread_id, request_id, info, attrs));
}

ServiceWorkerContextCore* ServiceWorkerDispatcherHost::GetContext() {
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
        thread_id, request_id, WebServiceWorkerError::kErrorTypeUnknown,
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
        thread_id, request_id, WebServiceWorkerError::kErrorTypeUnknown,
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
  handle->version()->StopWorker(
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

}  // namespace content
