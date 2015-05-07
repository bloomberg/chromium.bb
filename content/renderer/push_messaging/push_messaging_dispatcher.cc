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
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscription.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"
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

void PushMessagingDispatcher::subscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    blink::WebPushSubscriptionCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  RenderFrameImpl::FromRoutingID(routing_id())
      ->manifest_manager()
      ->GetManifest(base::Bind(&PushMessagingDispatcher::DoSubscribe,
                               base::Unretained(this),
                               service_worker_registration,
                               options,
                               callbacks));
}

void PushMessagingDispatcher::DoSubscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    blink::WebPushSubscriptionCallbacks* callbacks,
    const Manifest& manifest) {
  int request_id = subscription_callbacks_.Add(callbacks);
  int64_t service_worker_registration_id =
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

  // TODO(peter): Display a deprecation warning if gcm_user_visible_only is
  // set to true. See https://crbug.com/471534
  const bool user_visible = manifest.gcm_user_visible_only ||
                            options.userVisibleOnly;

  Send(new PushMessagingHostMsg_RegisterFromDocument(
      routing_id(), request_id,
      manifest.gcm_sender_id.is_null()
          ? std::string()
          : base::UTF16ToUTF8(manifest.gcm_sender_id.string()),
      user_visible, service_worker_registration_id));
}

void PushMessagingDispatcher::OnRegisterFromDocumentSuccess(
    int32_t request_id,
    const GURL& endpoint,
    const std::string& registration_id) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  DCHECK(callbacks);

  scoped_ptr<blink::WebPushSubscription> subscription(
      new blink::WebPushSubscription(
          WebString::fromUTF8(endpoint.spec()),
          WebString::fromUTF8(registration_id)));
  callbacks->onSuccess(subscription.release());

  subscription_callbacks_.Remove(request_id);
}

void PushMessagingDispatcher::OnRegisterFromDocumentError(
    int32_t request_id,
    PushRegistrationStatus status) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  DCHECK(callbacks);

  scoped_ptr<blink::WebPushError> error(new blink::WebPushError(
      blink::WebPushError::ErrorTypeAbort,
      WebString::fromUTF8(PushRegistrationStatusToString(status))));
  callbacks->onError(error.release());

  subscription_callbacks_.Remove(request_id);
}

}  // namespace content
