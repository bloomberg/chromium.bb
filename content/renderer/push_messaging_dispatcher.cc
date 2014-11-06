// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/push_messaging_dispatcher.h"

#include "base/strings/utf_string_conversions.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/common/push_messaging_messages.h"
#include "content/renderer/manifest/manifest_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebPushError.h"
#include "third_party/WebKit/public/platform/WebPushRegistration.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
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
    IPC_MESSAGE_HANDLER(PushMessagingMsg_RegisterSuccess, OnRegisterSuccess)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_RegisterError, OnRegisterError)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_PermissionStatusResult,
                        OnPermissionStatus)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_PermissionStatusFailure,
                        OnPermissionStatusFailure)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushMessagingDispatcher::registerPushMessaging(
    blink::WebPushRegistrationCallbacks* callbacks,
    blink::WebServiceWorkerProvider* service_worker_provider) {
  RenderFrameImpl::FromRoutingID(routing_id())->manifest_manager()->GetManifest(
      base::Bind(&PushMessagingDispatcher::DoRegister,
                 base::Unretained(this),
                 callbacks,
                 service_worker_provider));
}

void PushMessagingDispatcher::DoRegister(
    blink::WebPushRegistrationCallbacks* callbacks,
    blink::WebServiceWorkerProvider* service_worker_provider,
    const Manifest& manifest) {
  DCHECK(callbacks);
  int callbacks_id = registration_callbacks_.Add(callbacks);
  int service_worker_provider_id = static_cast<WebServiceWorkerProviderImpl*>(
                                       service_worker_provider)->provider_id();

  std::string sender_id = manifest.gcm_sender_id.is_null()
      ? std::string() : base::UTF16ToUTF8(manifest.gcm_sender_id.string());
  if (sender_id.empty()) {
    OnRegisterError(callbacks_id, PUSH_REGISTRATION_STATUS_NO_SENDER_ID);
    return;
  }

  Send(new PushMessagingHostMsg_Register(
      routing_id(),
      callbacks_id,
      manifest.gcm_sender_id.is_null()
          ? std::string()
          : base::UTF16ToUTF8(manifest.gcm_sender_id.string()),
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      service_worker_provider_id));
}

void PushMessagingDispatcher::getPermissionStatus(
    blink::WebPushPermissionStatusCallback* callback,
    blink::WebServiceWorkerProvider* service_worker_provider) {
  int permission_callback_id = permission_check_callbacks_.Add(callback);
  int service_worker_provider_id = static_cast<WebServiceWorkerProviderImpl*>(
                                       service_worker_provider)->provider_id();
  Send(new PushMessagingHostMsg_PermissionStatus(
      routing_id(), service_worker_provider_id, permission_callback_id));
}

void PushMessagingDispatcher::OnRegisterSuccess(
    int32 callbacks_id,
    const GURL& endpoint,
    const std::string& registration_id) {
  blink::WebPushRegistrationCallbacks* callbacks =
      registration_callbacks_.Lookup(callbacks_id);
  CHECK(callbacks);

  scoped_ptr<blink::WebPushRegistration> registration(
      new blink::WebPushRegistration(
          WebString::fromUTF8(endpoint.spec()),
          WebString::fromUTF8(registration_id)));
  callbacks->onSuccess(registration.release());
  registration_callbacks_.Remove(callbacks_id);
}

void PushMessagingDispatcher::OnRegisterError(int32 callbacks_id,
                                              PushRegistrationStatus status) {
  blink::WebPushRegistrationCallbacks* callbacks =
      registration_callbacks_.Lookup(callbacks_id);
  CHECK(callbacks);

  scoped_ptr<blink::WebPushError> error(new blink::WebPushError(
      blink::WebPushError::ErrorTypeAbort,
      WebString::fromUTF8(PushRegistrationStatusToString(status))));
  callbacks->onError(error.release());
  registration_callbacks_.Remove(callbacks_id);
}

void PushMessagingDispatcher::OnPermissionStatus(
    int32 callback_id,
    blink::WebPushPermissionStatus status) {
  blink::WebPushPermissionStatusCallback* callback =
      permission_check_callbacks_.Lookup(callback_id);
  callback->onSuccess(&status);
  permission_check_callbacks_.Remove(callback_id);
}

void PushMessagingDispatcher::OnPermissionStatusFailure(int32 callback_id) {
  blink::WebPushPermissionStatusCallback* callback =
      permission_check_callbacks_.Lookup(callback_id);
  callback->onError();
  permission_check_callbacks_.Remove(callback_id);
}

}  // namespace content
