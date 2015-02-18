// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging/push_messaging_message_filter.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/push_messaging_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/common/child_process_host.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"

namespace content {

const char kPushRegistrationIdServiceWorkerKey[] = "push_registration_id";

namespace {

void RecordRegistrationStatus(PushRegistrationStatus status) {
  // Only called from IO thread, but would be acceptable (even though slightly
  // racy) to call from UI thread as well, see
  // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/FNzZRJtN2aw/Aw0CWAXJJ1kJ
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.RegistrationStatus",
                            status,
                            PUSH_REGISTRATION_STATUS_LAST + 1);
}

const char kSenderIdServiceWorkerKey[] = "push_sender_id";

}  // namespace

struct PushMessagingMessageFilter::RegisterData {
  RegisterData();
  RegisterData(const RegisterData& other) = default;
  bool FromDocument() const;
  int request_id;
  GURL requesting_origin;
  int64 service_worker_registration_id;
  // The following two members should only be read if FromDocument() is true.
  int render_frame_id;
  bool user_visible_only;
};


// Inner core of this message filter which lives on the UI thread.
class PushMessagingMessageFilter::Core {
 public:
  Core(const base::WeakPtr<PushMessagingMessageFilter>& io_parent,
       int render_process_id);

  // Public Register methods on UI thread --------------------------------------

  // Called via PostTask from IO thread.
  void RegisterOnUI(const RegisterData& data, const std::string& sender_id);

  // Public Unregister methods on UI thread ------------------------------------

  // Called via PostTask from IO thread.
  void UnregisterFromService(int request_id,
                             int64 service_worker_registration_id,
                             const GURL& requesting_origin);

  // Public GetPermission methods on UI thread ---------------------------------

  // Called via PostTask from IO thread.
  void GetPermissionStatusOnUI(const GURL& requesting_origin, int request_id);

  // Public helper methods on UI thread ----------------------------------------

  // Returns a push messaging service. May return null.
  PushMessagingService* service();

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<Core>;

  ~Core();

  // Private Register methods on UI thread -------------------------------------

  void DidRegister(const RegisterData& data,
                   const std::string& push_registration_id,
                   PushRegistrationStatus status);

  // Private Unregister methods on UI thread -----------------------------------

  void DidUnregisterFromService(int request_id,
                                int64 service_worker_registration_id,
                                PushUnregistrationStatus unregistration_status);

  // Private helper methods on UI thread ---------------------------------------

  void Send(IPC::Message* message);

  // Outer part of this message filter which lives on the IO thread.
  base::WeakPtr<PushMessagingMessageFilter> io_parent_;

  int render_process_id_;

  base::WeakPtrFactory<Core> weak_factory_ui_to_ui_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};


PushMessagingMessageFilter::RegisterData::RegisterData()
    : request_id(0),
      service_worker_registration_id(0),
      render_frame_id(ChildProcessHost::kInvalidUniqueID),
      user_visible_only(false) {
}

bool PushMessagingMessageFilter::RegisterData::FromDocument() const {
  return render_frame_id != ChildProcessHost::kInvalidUniqueID;
}

PushMessagingMessageFilter::Core::Core(
    const base::WeakPtr<PushMessagingMessageFilter>& io_parent,
    int render_process_id)
    : io_parent_(io_parent),
      render_process_id_(render_process_id),
      weak_factory_ui_to_ui_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PushMessagingMessageFilter::Core::~Core() {}

PushMessagingMessageFilter::PushMessagingMessageFilter(
    int render_process_id,
    ServiceWorkerContextWrapper* service_worker_context)
    : BrowserMessageFilter(PushMessagingMsgStart),
      service_worker_context_(service_worker_context),
      weak_factory_io_to_io_(this) {
  // Although this class is used only on the IO thread, it is constructed on UI.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Normally, it would be unsafe to obtain a weak pointer from the UI thread,
  // but it's ok in the constructor since we can't be destroyed before our
  // constructor finishes.
  ui_core_.reset(new Core(weak_factory_io_to_io_.GetWeakPtr(),
                          render_process_id));
  PushMessagingService* push_service = ui_core_->service();
  if (push_service)
    push_endpoint_ = push_service->GetPushEndpoint();
}

PushMessagingMessageFilter::~PushMessagingMessageFilter() {}

void PushMessagingMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool PushMessagingMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushMessagingMessageFilter, message)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_RegisterFromDocument,
                        OnRegisterFromDocument)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_RegisterFromWorker,
                        OnRegisterFromWorker)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_Unregister,
                        OnUnregister)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_GetRegistration, OnGetRegistration)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_GetPermissionStatus,
                        OnGetPermissionStatus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Register methods on both IO and UI threads, merged in order of use from
// PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::OnRegisterFromDocument(
    int render_frame_id,
    int request_id,
    const std::string& sender_id,
    bool user_visible_only,
    int64 service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mvanouwerkerk): Validate arguments?
  // TODO(peter): Persist |user_visible_only| in Service Worker storage.
  RegisterData data;
  data.request_id = request_id;
  data.service_worker_registration_id = service_worker_registration_id;
  data.render_frame_id = render_frame_id;
  data.user_visible_only = user_visible_only;

  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->context()->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration ||
      !service_worker_registration->active_version()) {
    SendRegisterError(data, PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER);
    return;
  }
  data.requesting_origin = service_worker_registration->pattern().GetOrigin();

  service_worker_context_->context()->storage()->StoreUserData(
      service_worker_registration_id,
      data.requesting_origin,
      kSenderIdServiceWorkerKey,
      sender_id,
      base::Bind(&PushMessagingMessageFilter::DidPersistSenderId,
                 weak_factory_io_to_io_.GetWeakPtr(),
                 data, sender_id));
}

void PushMessagingMessageFilter::OnRegisterFromWorker(
    int request_id,
    int64 service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RegisterData data;
  data.request_id = request_id;
  data.service_worker_registration_id = service_worker_registration_id;

  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->context()->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration) {
    SendRegisterError(data, PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER);
    return;
  }
  data.requesting_origin = service_worker_registration->pattern().GetOrigin();

  // This sender_id will be ignored; instead it will be fetched from storage.
  CheckForExistingRegistration(data, std::string() /* sender_id */);
}

void PushMessagingMessageFilter::DidPersistSenderId(
    const RegisterData& data,
    const std::string& sender_id,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status != SERVICE_WORKER_OK)
    SendRegisterError(data, PUSH_REGISTRATION_STATUS_STORAGE_ERROR);
  else
    CheckForExistingRegistration(data, sender_id);
}

void PushMessagingMessageFilter::CheckForExistingRegistration(
    const RegisterData& data,
    const std::string& sender_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->context()->storage()->GetUserData(
      data.service_worker_registration_id,
      kPushRegistrationIdServiceWorkerKey,
      base::Bind(&PushMessagingMessageFilter::DidCheckForExistingRegistration,
                 weak_factory_io_to_io_.GetWeakPtr(), data, sender_id));
}

void PushMessagingMessageFilter::DidCheckForExistingRegistration(
    const RegisterData& data,
    const std::string& sender_id,
    const std::string& push_registration_id,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status == SERVICE_WORKER_OK) {
    SendRegisterSuccess(data, PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE,
                        push_registration_id);
    return;
  }
  // TODO(johnme): The spec allows the register algorithm to reject with an
  // AbortError when accessing storage fails. Perhaps we should do that if
  // service_worker_status != SERVICE_WORKER_ERROR_NOT_FOUND instead of
  // attempting to do a fresh registration?
  // https://w3c.github.io/push-api/#widl-PushRegistrationManager-register-Promise-PushRegistration
  if (data.FromDocument()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&Core::RegisterOnUI, base::Unretained(ui_core_.get()),
                   data, sender_id));
  } else {
    service_worker_context_->context()->storage()->GetUserData(
        data.service_worker_registration_id,
        kSenderIdServiceWorkerKey,
        base::Bind(&PushMessagingMessageFilter::DidGetSenderIdFromStorage,
                   weak_factory_io_to_io_.GetWeakPtr(), data));
  }
}

void PushMessagingMessageFilter::DidGetSenderIdFromStorage(
    const RegisterData& data,
    const std::string& sender_id,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status != SERVICE_WORKER_OK) {
    SendRegisterError(data, PUSH_REGISTRATION_STATUS_NO_SENDER_ID);
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::RegisterOnUI, base::Unretained(ui_core_.get()),
                 data, sender_id));
}

void PushMessagingMessageFilter::Core::RegisterOnUI(
    const PushMessagingMessageFilter::RegisterData& data,
    const std::string& sender_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service = service();
  if (!push_service) {
    // TODO(johnme): Prevent websites from detecting incognito mode.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PushMessagingMessageFilter::SendRegisterError, io_parent_,
                   data, PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE));
    return;
  }

  if (data.FromDocument()) {
    push_service->RegisterFromDocument(
        data.requesting_origin, data.service_worker_registration_id, sender_id,
        render_process_id_, data.render_frame_id, data.user_visible_only,
        base::Bind(&Core::DidRegister, weak_factory_ui_to_ui_.GetWeakPtr(),
                   data));
  } else {
    push_service->RegisterFromWorker(
        data.requesting_origin, data.service_worker_registration_id, sender_id,
        base::Bind(&Core::DidRegister, weak_factory_ui_to_ui_.GetWeakPtr(),
                   data));
  }
}

void PushMessagingMessageFilter::Core::DidRegister(
    const RegisterData& data,
    const std::string& push_registration_id,
    PushRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status == PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PushMessagingMessageFilter::PersistRegistrationOnIO,
                   io_parent_, data, push_registration_id));
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PushMessagingMessageFilter::SendRegisterError, io_parent_,
                   data, status));
  }
}

void PushMessagingMessageFilter::PersistRegistrationOnIO(
    const RegisterData& data,
    const std::string& push_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->context()->storage()->StoreUserData(
      data.service_worker_registration_id,
      data.requesting_origin,
      kPushRegistrationIdServiceWorkerKey,
      push_registration_id,
      base::Bind(&PushMessagingMessageFilter::DidPersistRegistrationOnIO,
                 weak_factory_io_to_io_.GetWeakPtr(),
                 data, push_registration_id));
}

void PushMessagingMessageFilter::DidPersistRegistrationOnIO(
    const RegisterData& data,
    const std::string& push_registration_id,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status == SERVICE_WORKER_OK) {
    SendRegisterSuccess(data,
                        PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE,
                        push_registration_id);
  } else {
    // TODO(johnme): Unregister, so PushMessagingServiceImpl can decrease count.
    SendRegisterError(data, PUSH_REGISTRATION_STATUS_STORAGE_ERROR);
  }
}

void PushMessagingMessageFilter::SendRegisterError(
    const RegisterData& data, PushRegistrationStatus status) {
  // Only called from IO thread, but would be safe to call from UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (data.FromDocument()) {
    Send(new PushMessagingMsg_RegisterFromDocumentError(
        data.render_frame_id, data.request_id, status));
  } else {
    Send(new PushMessagingMsg_RegisterFromWorkerError(
        data.request_id, status));
  }
  RecordRegistrationStatus(status);
}

void PushMessagingMessageFilter::SendRegisterSuccess(
    const RegisterData& data,
    PushRegistrationStatus status,
    const std::string& push_registration_id) {
  // Only called from IO thread, but would be safe to call from UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (push_endpoint_.is_empty()) {
    // TODO(johnme): Prevent websites from detecting incognito mode.
    SendRegisterError(data, PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE);
    return;
  }
  if (data.FromDocument()) {
    Send(new PushMessagingMsg_RegisterFromDocumentSuccess(
        data.render_frame_id,
        data.request_id, push_endpoint_, push_registration_id));
  } else {
    Send(new PushMessagingMsg_RegisterFromWorkerSuccess(
        data.request_id, push_endpoint_, push_registration_id));
  }
  RecordRegistrationStatus(status);
}

// Unregister methods on both IO and UI threads, merged in order of use from
// PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::OnUnregister(
    int request_id, int64 service_worker_registration_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->context()->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration) {
    DidUnregister(request_id, PUSH_UNREGISTRATION_STATUS_UNKNOWN_ERROR);
    return;
  }

  service_worker_context_->context()->storage()->GetUserData(
      service_worker_registration_id,
      kPushRegistrationIdServiceWorkerKey,
      base::Bind(&PushMessagingMessageFilter::DoUnregister,
                 weak_factory_io_to_io_.GetWeakPtr(),
                 request_id,
                 service_worker_registration_id,
                 service_worker_registration->pattern().GetOrigin()));
}

void PushMessagingMessageFilter::DoUnregister(
    int request_id,
    int64 service_worker_registration_id,
    const GURL& requesting_origin,
    const std::string& push_registration_id,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  switch (service_worker_status) {
    case SERVICE_WORKER_OK:
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&Core::UnregisterFromService,
                     base::Unretained(ui_core_.get()), request_id,
                     service_worker_registration_id, requesting_origin));
      return;
    case SERVICE_WORKER_ERROR_NOT_FOUND:
      // We did not find a registration, stop here and notify the renderer that
      // it was a success even though we did not unregister.
      DidUnregister(request_id,
                    PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED);
      return;
    case SERVICE_WORKER_ERROR_FAILED:
      DidUnregister(request_id,
                    PUSH_UNREGISTRATION_STATUS_UNKNOWN_ERROR);
      return;
    case SERVICE_WORKER_ERROR_ABORT:
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case SERVICE_WORKER_ERROR_STATE:
      NOTREACHED() << "Got unexpected error code: " << service_worker_status
                   << " " << ServiceWorkerStatusToString(service_worker_status);
      DidUnregister(request_id, PUSH_UNREGISTRATION_STATUS_UNKNOWN_ERROR);
      return;
  }
}

void PushMessagingMessageFilter::Core::UnregisterFromService(
    int request_id,
    int64 service_worker_registration_id,
    const GURL& requesting_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service = service();
  if (!push_service) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PushMessagingMessageFilter::DidUnregister, io_parent_,
                   request_id, PUSH_UNREGISTRATION_STATUS_UNKNOWN_ERROR));
    return;
  }

  push_service->Unregister(
      requesting_origin, service_worker_registration_id,
      false /* retry_on_failure */,
      base::Bind(&Core::DidUnregisterFromService,
                 weak_factory_ui_to_ui_.GetWeakPtr(),
                 request_id, service_worker_registration_id));
}

void PushMessagingMessageFilter::Core::DidUnregisterFromService(
    int request_id,
    int64 service_worker_registration_id,
    PushUnregistrationStatus unregistration_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  switch (unregistration_status) {
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTER:
      break;
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_WILL_RETRY_NETWORK_ERROR:
      NOTREACHED();
      break;
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED:
    case PUSH_UNREGISTRATION_STATUS_NETWORK_ERROR:
    case PUSH_UNREGISTRATION_STATUS_UNKNOWN_ERROR:
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&PushMessagingMessageFilter::DidUnregister, io_parent_,
                     request_id, unregistration_status));
      return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PushMessagingMessageFilter::ClearRegistrationData, io_parent_,
                 request_id, service_worker_registration_id,
                 unregistration_status));
}

void PushMessagingMessageFilter::ClearRegistrationData(
    int request_id,
    int64 service_worker_registration_id,
    PushUnregistrationStatus unregistration_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->context()->storage()->ClearUserData(
      service_worker_registration_id,
      kPushRegistrationIdServiceWorkerKey,
      base::Bind(&PushMessagingMessageFilter::DidClearRegistrationData,
                 weak_factory_io_to_io_.GetWeakPtr(),
                 request_id, unregistration_status));
}

void PushMessagingMessageFilter::DidClearRegistrationData(
    int request_id,
    PushUnregistrationStatus unregistration_status,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(service_worker_status == SERVICE_WORKER_OK)
      << "Got unexpected error code: " << service_worker_status
      << " " << ServiceWorkerStatusToString(service_worker_status);

  DidUnregister(request_id, unregistration_status);
}

void PushMessagingMessageFilter::DidUnregister(
    int request_id,
    PushUnregistrationStatus unregistration_status) {
  // Only called from IO thread, but would be safe to call from UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (unregistration_status) {
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTER:
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_WILL_RETRY_NETWORK_ERROR:
      Send(new PushMessagingMsg_UnregisterSuccess(request_id, true));
      return;
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED:
      Send(new PushMessagingMsg_UnregisterSuccess(request_id, false));
      return;
    case PUSH_UNREGISTRATION_STATUS_NETWORK_ERROR:
      Send(new PushMessagingMsg_UnregisterError(
          request_id, blink::WebPushError::ErrorTypeNetwork,
          "Failed to connect to the push server."));
      return;
    case PUSH_UNREGISTRATION_STATUS_UNKNOWN_ERROR:
      Send(new PushMessagingMsg_UnregisterError(
          request_id, blink::WebPushError::ErrorTypeUnknown,
          "Unexpected error while trying to unregister from the push server."));
      return;
  }
}

// GetRegistration methods on both IO and UI threads, merged in order of use
// from PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::OnGetRegistration(
    int request_id,
    int64 service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(johnme): Validate arguments?
  service_worker_context_->context()->storage()->GetUserData(
      service_worker_registration_id,
      kPushRegistrationIdServiceWorkerKey,
      base::Bind(&PushMessagingMessageFilter::DidGetRegistration,
                 weak_factory_io_to_io_.GetWeakPtr(), request_id));
}

void PushMessagingMessageFilter::DidGetRegistration(
    int request_id,
    const std::string& push_registration_id,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PushGetRegistrationStatus get_status =
      PUSH_GETREGISTRATION_STATUS_SERVICE_WORKER_ERROR;
  switch (service_worker_status) {
    case SERVICE_WORKER_OK:
      if (push_endpoint_.is_empty()) {
        // TODO(johnme): Prevent websites from detecting incognito mode.
        Send(new PushMessagingMsg_GetRegistrationError(
            request_id, PUSH_GETREGISTRATION_STATUS_SERVICE_WORKER_ERROR));
        return;
      }
      Send(new PushMessagingMsg_GetRegistrationSuccess(request_id,
                                                       push_endpoint_,
                                                       push_registration_id));
      return;
    case SERVICE_WORKER_ERROR_NOT_FOUND:
      get_status = PUSH_GETREGISTRATION_STATUS_REGISTRATION_NOT_FOUND;
      break;
    case SERVICE_WORKER_ERROR_FAILED:
      get_status = PUSH_GETREGISTRATION_STATUS_SERVICE_WORKER_ERROR;
      break;
    case SERVICE_WORKER_ERROR_ABORT:
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case SERVICE_WORKER_ERROR_STATE:
      NOTREACHED() << "Got unexpected error code: " << service_worker_status
                   << " " << ServiceWorkerStatusToString(service_worker_status);
      get_status = PUSH_GETREGISTRATION_STATUS_SERVICE_WORKER_ERROR;
      break;
  }
  Send(new PushMessagingMsg_GetRegistrationError(request_id, get_status));
  // TODO(johnme): RecordGetRegistrationStatus(status); ?
}

// GetPermission methods on both IO and UI threads, merged in order of use from
// PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::OnGetPermissionStatus(
    int request_id,
    int64 service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->context()->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration) {
    Send(new PushMessagingMsg_GetPermissionStatusError(request_id));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::GetPermissionStatusOnUI,
                 base::Unretained(ui_core_.get()),
                 service_worker_registration->pattern().GetOrigin(),
                 request_id));
}

void PushMessagingMessageFilter::Core::GetPermissionStatusOnUI(
    const GURL& requesting_origin,
    int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service = service();
  if (!push_service) {
    // TODO(johnme): Prevent websites from detecting incognito mode.
    Send(new PushMessagingMsg_GetPermissionStatusError(request_id));
    return;
  }
  GURL embedding_origin = requesting_origin;
  blink::WebPushPermissionStatus permission_status =
      push_service->GetPermissionStatus(requesting_origin, embedding_origin);
  Send(new PushMessagingMsg_GetPermissionStatusSuccess(request_id,
                                                       permission_status));
}

// Helper methods on both IO and UI threads, merged from
// PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::Core::Send(IPC::Message* message) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PushMessagingMessageFilter::SendIPC, io_parent_,
                 base::Passed(make_scoped_ptr(message))));
}

void PushMessagingMessageFilter::SendIPC(scoped_ptr<IPC::Message> message) {
  Send(message.release());
}

PushMessagingService* PushMessagingMessageFilter::Core::service() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* process_host =
      RenderProcessHost::FromID(render_process_id_);
  return process_host
         ? process_host->GetBrowserContext()->GetPushMessagingService()
         : nullptr;
}

}  // namespace content
