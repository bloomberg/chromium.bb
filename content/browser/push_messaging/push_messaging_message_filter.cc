// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging/push_messaging_message_filter.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/push_messaging_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"
#include "third_party/WebKit/public/platform/WebPushPermissionStatus.h"

namespace content {
namespace {

void RecordRegistrationStatus(PushRegistrationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.RegistrationStatus",
                            status,
                            PUSH_REGISTRATION_STATUS_LAST + 1);
}

}  // namespace

PushMessagingMessageFilter::PushMessagingMessageFilter(
    int render_process_id,
    ServiceWorkerContextWrapper* service_worker_context)
    : BrowserMessageFilter(PushMessagingMsgStart),
      render_process_id_(render_process_id),
      service_worker_context_(service_worker_context),
      service_(NULL),
      weak_factory_ui_to_ui_(this) {
}

PushMessagingMessageFilter::~PushMessagingMessageFilter() {}

bool PushMessagingMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushMessagingMessageFilter, message)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_RegisterFromDocument,
                        OnRegisterFromDocument)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_RegisterFromWorker,
                        OnRegisterFromWorker)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_PermissionStatus,
                        OnPermissionStatusRequest)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_GetPermissionStatus,
                        OnGetPermissionStatus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushMessagingMessageFilter::OnRegisterFromDocument(
    int render_frame_id,
    int request_id,
    const std::string& sender_id,
    bool user_gesture,
    int service_worker_provider_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mvanouwerkerk): Validate arguments?
  ServiceWorkerProviderHost* service_worker_host =
      service_worker_context_->context()->GetProviderHost(
          render_process_id_, service_worker_provider_id);
  if (!service_worker_host || !service_worker_host->active_version()) {
    PushRegistrationStatus status =
        PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER;
    Send(new PushMessagingMsg_RegisterFromDocumentError(render_frame_id,
                                                        request_id, status));
    RecordRegistrationStatus(status);
    return;
  }

  // TODO(mvanouwerkerk): Persist sender id in Service Worker storage.
  // https://crbug.com/437298

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PushMessagingMessageFilter::RegisterFromDocumentOnUI,
                 this, render_frame_id, request_id, sender_id, user_gesture,
                 service_worker_host->active_version()->scope().GetOrigin(),
                 service_worker_host->active_version()->registration_id()));
}

void PushMessagingMessageFilter::OnRegisterFromWorker(
    int request_id,
    int64 service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->context()->GetLiveRegistration(
          service_worker_registration_id);
  DCHECK(service_worker_registration);
  if (!service_worker_registration)
    return;

  // TODO(mvanouwerkerk): Get sender id from Service Worker storage.
  // https://crbug.com/437298
  std::string sender_id = "";

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PushMessagingMessageFilter::RegisterFromWorkerOnUI,
                 this, request_id, sender_id,
                 service_worker_registration->pattern().GetOrigin(),
                 service_worker_registration_id));
}

void PushMessagingMessageFilter::OnPermissionStatusRequest(
    int render_frame_id,
    int service_worker_provider_id,
    int permission_callback_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerProviderHost* service_worker_host =
      service_worker_context_->context()->GetProviderHost(
          render_process_id_, service_worker_provider_id);

  if (service_worker_host && service_worker_host->active_version()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&PushMessagingMessageFilter::DoPermissionStatusRequest,
                   this,
                   service_worker_host->active_version()->scope().GetOrigin(),
                   render_frame_id,
                   permission_callback_id));
  } else {
    Send(new PushMessagingMsg_PermissionStatusFailure(
          render_frame_id, permission_callback_id));
  }
}

void PushMessagingMessageFilter::OnGetPermissionStatus(
    int request_id,
    int64 service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->context()->GetLiveRegistration(
          service_worker_registration_id);
  DCHECK(service_worker_registration);
  if (!service_worker_registration)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PushMessagingMessageFilter::GetPermissionStatusOnUI,
                 this,
                 service_worker_registration->pattern().GetOrigin(),
                 request_id));
}

void PushMessagingMessageFilter::RegisterFromDocumentOnUI(
    int render_frame_id,
    int request_id,
    const std::string& sender_id,
    bool user_gesture,
    const GURL& requesting_origin,
    int64 service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service()) {
    PushRegistrationStatus status =
        PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE;
    Send(new PushMessagingMsg_RegisterFromDocumentError(render_frame_id,
                                                        request_id, status));
    RecordRegistrationStatus(status);
    return;
  }
  service()->RegisterFromDocument(
      requesting_origin, service_worker_registration_id, sender_id,
      render_process_id_, render_frame_id, user_gesture,
      base::Bind(&PushMessagingMessageFilter::DidRegisterFromDocument,
                 weak_factory_ui_to_ui_.GetWeakPtr(),
                 render_frame_id, request_id));
}

void PushMessagingMessageFilter::RegisterFromWorkerOnUI(
    int request_id,
    const std::string& sender_id,
    const GURL& requesting_origin,
    int64 service_worker_registration_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!service()) {
    PushRegistrationStatus status =
        PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE;
    Send(new PushMessagingMsg_RegisterFromWorkerError(request_id, status));
    RecordRegistrationStatus(status);
    return;
  }
  service()->RegisterFromWorker(
      requesting_origin, service_worker_registration_id, sender_id,
      base::Bind(&PushMessagingMessageFilter::DidRegisterFromWorker,
                 weak_factory_ui_to_ui_.GetWeakPtr(), request_id));
}

void PushMessagingMessageFilter::DoPermissionStatusRequest(
    const GURL& requesting_origin,
    int render_frame_id,
    int callback_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::WebPushPermissionStatus permission_value =
      service()->GetPermissionStatus(
          requesting_origin, render_process_id_, render_frame_id);

  Send(new PushMessagingMsg_PermissionStatusResult(
      render_frame_id, callback_id, permission_value));
}

void PushMessagingMessageFilter::GetPermissionStatusOnUI(
    const GURL& requesting_origin,
    int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL embedding_origin = requesting_origin;
  blink::WebPushPermissionStatus permission_status =
      service()->GetPermissionStatus(requesting_origin, embedding_origin);
  Send(new PushMessagingMsg_GetPermissionStatusSuccess(request_id,
                                                       permission_status));
}

void PushMessagingMessageFilter::DidRegisterFromDocument(
    int render_frame_id,
    int request_id,
    const std::string& push_registration_id,
    PushRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status == PUSH_REGISTRATION_STATUS_SUCCESS) {
    GURL push_endpoint(service()->PushEndpoint());
    Send(new PushMessagingMsg_RegisterFromDocumentSuccess(
        render_frame_id, request_id, push_endpoint, push_registration_id));
  } else {
    Send(new PushMessagingMsg_RegisterFromDocumentError(render_frame_id,
                                                        request_id, status));
  }
  RecordRegistrationStatus(status);
}

void PushMessagingMessageFilter::DidRegisterFromWorker(
    int request_id,
    const std::string& push_registration_id,
    PushRegistrationStatus status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (status == PUSH_REGISTRATION_STATUS_SUCCESS) {
    GURL push_endpoint(service()->PushEndpoint());
    Send(new PushMessagingMsg_RegisterFromWorkerSuccess(
        request_id, push_endpoint, push_registration_id));
  } else {
    Send(new PushMessagingMsg_RegisterFromWorkerError(request_id, status));
  }
  RecordRegistrationStatus(status);
}

PushMessagingService* PushMessagingMessageFilter::service() {
  if (!service_) {
    RenderProcessHost* process_host =
        RenderProcessHost::FromID(render_process_id_);
    if (!process_host)
      return NULL;
    service_ = process_host->GetBrowserContext()->GetPushMessagingService();
  }
  return service_;
}

}  // namespace content
