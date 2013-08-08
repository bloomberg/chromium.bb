// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/webrtc_identity_service_host.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/media/webrtc_identity_store.h"
#include "content/common/media/webrtc_identity_messages.h"
#include "net/base/net_errors.h"

namespace content {

WebRTCIdentityServiceHost::WebRTCIdentityServiceHost(
    int renderer_process_id,
    WebRTCIdentityStore* identity_store)
    : renderer_process_id_(renderer_process_id),
      identity_store_(identity_store) {}

WebRTCIdentityServiceHost::~WebRTCIdentityServiceHost() {
  if (!cancel_callback_.is_null())
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
    const GURL& origin,
    const std::string& identity_name,
    const std::string& common_name) {
  if (!cancel_callback_.is_null()) {
    DLOG(WARNING)
        << "Request rejected because the previous request has not finished.";
    SendErrorMessage(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(renderer_process_id_, origin)) {
    DLOG(WARNING) << "Request rejected because origin access is denied.";
    SendErrorMessage(net::ERR_ACCESS_DENIED);
    return;
  }

  cancel_callback_ = identity_store_->RequestIdentity(
      origin,
      identity_name,
      common_name,
      base::Bind(&WebRTCIdentityServiceHost::OnComplete,
                 base::Unretained(this)));
  if (cancel_callback_.is_null()) {
    SendErrorMessage(net::ERR_UNEXPECTED);
  }
}

void WebRTCIdentityServiceHost::OnCancelRequest() {
  base::ResetAndReturn(&cancel_callback_).Run();
}

void WebRTCIdentityServiceHost::OnComplete(int status,
                                         const std::string& certificate,
                                         const std::string& private_key) {
  cancel_callback_.Reset();
  if (status == net::OK) {
    Send(new WebRTCIdentityHostMsg_IdentityReady(certificate, private_key));
  } else {
    SendErrorMessage(status);
  }
}

void WebRTCIdentityServiceHost::SendErrorMessage(int error) {
  Send(new WebRTCIdentityHostMsg_RequestFailed(error));
}

}  // namespace content
