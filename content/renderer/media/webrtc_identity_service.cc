// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_identity_service.h"

#include "base/atomic_sequence_num.h"
#include "content/common/media/webrtc_identity_messages.h"
#include "content/public/renderer/render_thread.h"

namespace content {

WebRTCIdentityService::WebRTCIdentityService(const GURL& origin)
    : origin_(origin), pending_observer_(NULL), pending_request_id_(0) {
  RenderThread::Get()->AddObserver(this);
}

WebRTCIdentityService::~WebRTCIdentityService() {
  RenderThread::Get()->RemoveObserver(this);
  if (pending_observer_) {
    RenderThread::Get()->Send(
        new WebRTCIdentityMsg_CancelRequest(pending_request_id_));
  }
}

bool WebRTCIdentityService::RequestIdentity(
    const std::string& identity_name,
    const std::string& common_name,
    webrtc::DTLSIdentityRequestObserver* observer) {
  // Static because the request id needs to be unique per renderer process
  // across WebRTCIdentityService instances.
  static base::AtomicSequenceNumber s_next_request_id;

  DCHECK(observer);
  if (pending_observer_)
    return false;

  pending_observer_ = observer;
  pending_request_id_ = s_next_request_id.GetNext();
  RenderThread::Get()->Send(new WebRTCIdentityMsg_RequestIdentity(
      pending_request_id_, origin_, identity_name, common_name));
  return true;
}

bool WebRTCIdentityService::OnControlMessageReceived(
    const IPC::Message& message) {
  if (!pending_observer_)
    return false;

  int old_pending_request_id = pending_request_id_;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebRTCIdentityService, message)
    IPC_MESSAGE_HANDLER(WebRTCIdentityHostMsg_IdentityReady, OnIdentityReady)
    IPC_MESSAGE_HANDLER(WebRTCIdentityHostMsg_RequestFailed, OnRequestFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (pending_request_id_ == old_pending_request_id)
    handled = false;

  return handled;
}

void WebRTCIdentityService::OnIdentityReady(int request_id,
                                            const std::string& certificate,
                                            const std::string& private_key) {
  if (request_id != pending_request_id_)
    return;
  pending_observer_->OnSuccess(certificate, private_key);
  pending_observer_ = NULL;
  pending_request_id_ = 0;
}

void WebRTCIdentityService::OnRequestFailed(int request_id, int error) {
  if (request_id != pending_request_id_)
    return;
  pending_observer_->OnFailure(error);
  pending_observer_ = NULL;
  pending_request_id_ = 0;
}

}  // namespace content
