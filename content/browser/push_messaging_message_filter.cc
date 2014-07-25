// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging_message_filter.h"

#include <string>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/push_messaging_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"

namespace content {

PushMessagingMessageFilter::PushMessagingMessageFilter(
    int render_process_id,
    ServiceWorkerContextWrapper* service_worker_context)
    : BrowserMessageFilter(PushMessagingMsgStart),
      render_process_id_(render_process_id),
      service_worker_context_(service_worker_context),
      service_(NULL),
      weak_factory_(this) {
}

PushMessagingMessageFilter::~PushMessagingMessageFilter() {}

bool PushMessagingMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushMessagingMessageFilter, message)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_Register, OnRegister)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushMessagingMessageFilter::OnRegister(int render_frame_id,
                                            int callbacks_id,
                                            const std::string& sender_id,
                                            bool user_gesture,
                                            int service_worker_provider_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // TODO(mvanouwerkerk): Validate arguments?
  ServiceWorkerProviderHost* service_worker_host =
      service_worker_context_->context()->GetProviderHost(
          render_process_id_, service_worker_provider_id);
  if (!service_worker_host || !service_worker_host->active_version()) {
    Send(new PushMessagingMsg_RegisterError(
        render_frame_id,
        callbacks_id,
        PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_NO_SERVICE_WORKER));
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&PushMessagingMessageFilter::DoRegister,
                 weak_factory_.GetWeakPtr(),
                 render_frame_id,
                 callbacks_id,
                 sender_id,
                 user_gesture,
                 service_worker_host->active_version()->scope().GetOrigin(),
                 service_worker_host->active_version()->registration_id()));
}

void PushMessagingMessageFilter::DoRegister(
    int render_frame_id,
    int callbacks_id,
    const std::string& sender_id,
    bool user_gesture,
    const GURL& origin,
    int64 service_worker_registration_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!service()) {
    Send(new PushMessagingMsg_RegisterError(
        render_frame_id,
        callbacks_id,
        PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_SERVICE_NOT_AVAILABLE));
    return;
  }
  service()->Register(origin,
                      service_worker_registration_id,
                      sender_id,
                      render_process_id_,
                      render_frame_id,
                      user_gesture,
                      base::Bind(&PushMessagingMessageFilter::DidRegister,
                                 weak_factory_.GetWeakPtr(),
                                 render_frame_id,
                                 callbacks_id));
}

void PushMessagingMessageFilter::DidRegister(
    int render_frame_id,
    int callbacks_id,
    const GURL& push_endpoint,
    const std::string& push_registration_id,
    PushMessagingStatus status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (status == PUSH_MESSAGING_STATUS_OK) {
    Send(new PushMessagingMsg_RegisterSuccess(
        render_frame_id, callbacks_id, push_endpoint, push_registration_id));
  } else {
    Send(new PushMessagingMsg_RegisterError(
        render_frame_id, callbacks_id, status));
  }
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
