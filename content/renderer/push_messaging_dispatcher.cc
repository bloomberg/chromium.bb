// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/push_messaging_dispatcher.h"

#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/common/push_messaging_messages.h"
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushMessagingDispatcher::registerPushMessaging(
    const WebString& sender_id,
    blink::WebPushRegistrationCallbacks* callbacks) {
  DCHECK(callbacks);
  scoped_ptr<blink::WebPushError> error(new blink::WebPushError(
      blink::WebPushError::ErrorTypeAbort,
      WebString::fromUTF8(PushMessagingStatusToString(
          PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_NO_SERVICE_WORKER))));
  callbacks->onError(error.release());
  delete callbacks;
}

void PushMessagingDispatcher::registerPushMessaging(
    const WebString& sender_id,
    blink::WebPushRegistrationCallbacks* callbacks,
    blink::WebServiceWorkerProvider* service_worker_provider) {
  DCHECK(callbacks);
  int callbacks_id = registration_callbacks_.Add(callbacks);
  int service_worker_provider_id = static_cast<WebServiceWorkerProviderImpl*>(
                                       service_worker_provider)->provider_id();
  Send(new PushMessagingHostMsg_Register(
      routing_id(),
      callbacks_id,
      sender_id.utf8(),
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      service_worker_provider_id));
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
                                              PushMessagingStatus status) {
  blink::WebPushRegistrationCallbacks* callbacks =
      registration_callbacks_.Lookup(callbacks_id);
  CHECK(callbacks);

  scoped_ptr<blink::WebPushError> error(new blink::WebPushError(
      blink::WebPushError::ErrorTypeAbort,
      WebString::fromUTF8(PushMessagingStatusToString(status))));
  callbacks->onError(error.release());
  registration_callbacks_.Remove(callbacks_id);
}

}  // namespace content
