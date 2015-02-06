// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/push_messaging/push_messaging_dispatcher.h"

#include "base/strings/utf_string_conversions.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/common/push_messaging_messages.h"
#include "content/renderer/manifest/manifest_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushError.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushRegistration.h"
#include "url/gurl.h"

using blink::WebString;

namespace content {

PushMessagingDispatcher::PushMessagingDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

PushMessagingDispatcher::~PushMessagingDispatcher() {}

bool PushMessagingDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushMessagingDispatcher, message)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_RegisterFromDocumentSuccess,
                        OnRegisterFromDocumentSuccess)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_RegisterFromDocumentError,
                        OnRegisterFromDocumentError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushMessagingDispatcher::registerPushMessaging(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebPushRegistrationCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  RenderFrameImpl::FromRoutingID(routing_id())
      ->manifest_manager()
      ->GetManifest(base::Bind(&PushMessagingDispatcher::DoRegister,
                               base::Unretained(this),
                               service_worker_registration, callbacks));
}

void PushMessagingDispatcher::DoRegister(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebPushRegistrationCallbacks* callbacks,
    const Manifest& manifest) {
  int request_id = registration_callbacks_.Add(callbacks);
  int64 service_worker_registration_id =
      static_cast<WebServiceWorkerRegistrationImpl*>(
          service_worker_registration)->registration_id();

  std::string sender_id =
      manifest.gcm_sender_id.is_null()
          ? std::string()
          : base::UTF16ToUTF8(manifest.gcm_sender_id.string());
  if (sender_id.empty()) {
    OnRegisterFromDocumentError(request_id,
                                PUSH_REGISTRATION_STATUS_NO_SENDER_ID);
    return;
  }

  Send(new PushMessagingHostMsg_RegisterFromDocument(
      routing_id(), request_id,
      manifest.gcm_sender_id.is_null()
          ? std::string()
          : base::UTF16ToUTF8(manifest.gcm_sender_id.string()),
      manifest.gcm_user_visible_only, service_worker_registration_id));
}

void PushMessagingDispatcher::OnRegisterFromDocumentSuccess(
    int32 request_id,
    const GURL& endpoint,
    const std::string& registration_id) {
  blink::WebPushRegistrationCallbacks* callbacks =
      registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);

  scoped_ptr<blink::WebPushRegistration> registration(
      new blink::WebPushRegistration(
          WebString::fromUTF8(endpoint.spec()),
          WebString::fromUTF8(registration_id)));
  callbacks->onSuccess(registration.release());
  registration_callbacks_.Remove(request_id);
}

void PushMessagingDispatcher::OnRegisterFromDocumentError(
    int32 request_id,
    PushRegistrationStatus status) {
  blink::WebPushRegistrationCallbacks* callbacks =
      registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);

  scoped_ptr<blink::WebPushError> error(new blink::WebPushError(
      blink::WebPushError::ErrorTypeAbort,
      WebString::fromUTF8(PushRegistrationStatusToString(status))));
  callbacks->onError(error.release());
  registration_callbacks_.Remove(request_id);
}

}  // namespace content
