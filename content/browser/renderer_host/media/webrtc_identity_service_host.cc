// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/webrtc_identity_service_host.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/browser/media/webrtc_identity_store.h"
#include "content/common/media/webrtc_identity_messages.h"
#include "net/base/net_errors.h"

namespace content {

WebRTCIdentityServiceHost::WebRTCIdentityServiceHost(
    WebRTCIdentityStore* identity_store)
    : identity_store_(identity_store) {}

WebRTCIdentityServiceHost::~WebRTCIdentityServiceHost() {
  if (cancel_callback_.is_null())
    return;
  cancel_callback_.Run();
}

bool WebRTCIdentityServiceHost::OnMessageReceived(const IPC::Message& message,
                                                  bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WebRTCIdentityServiceHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(WebRTCIdentityMsg_RequestIdentity, OnRequestIdentity)
    IPC_MESSAGE_HANDLER(WebRTCIdentityMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void WebRTCIdentityServiceHost::OnRequestIdentity(
    int request_id,
    const GURL& origin,
    const std::string& identity_name,
    const std::string& common_name) {
  if (!cancel_callback_.is_null()) {
    DLOG(WARNING)
        << "The request is rejected because there is already a pending request";
    SendErrorMessage(request_id, net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }
  cancel_callback_ = identity_store_->RequestIdentity(
      origin,
      identity_name,
      common_name,
      base::Bind(&WebRTCIdentityServiceHost::OnComplete,
                 base::Unretained(this),
                 request_id));
  if (cancel_callback_.is_null()) {
    SendErrorMessage(request_id, net::ERR_UNEXPECTED);
  }
}

void WebRTCIdentityServiceHost::OnCancelRequest(int request_id) {
  base::ResetAndReturn(&cancel_callback_);
}

void WebRTCIdentityServiceHost::OnComplete(int request_id,
                                           int error,
                                           const std::string& certificate,
                                           const std::string& private_key) {
  cancel_callback_.Reset();
  if (error == net::OK) {
    Send(new WebRTCIdentityHostMsg_IdentityReady(
        request_id, certificate, private_key));
  } else {
    SendErrorMessage(request_id, error);
  }
}

void WebRTCIdentityServiceHost::SendErrorMessage(int request_id, int error) {
  Send(new WebRTCIdentityHostMsg_RequestFailed(request_id, error));
}

}  // namespace content
