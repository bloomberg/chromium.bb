// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/renderer/credential_manager_client.h"

#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace password_manager {

namespace {

template <typename T>
void ClearCallbacksMapWithErrors(T* callbacks_map) {
  typename T::iterator iter(callbacks_map);
  while (!iter.IsAtEnd()) {
    blink::WebCredentialManagerError reason(
        blink::WebCredentialManagerError::ErrorTypeUnknown,
        "An unknown error occurred.");
    iter.GetCurrentValue()->onError(&reason);
    callbacks_map->Remove(iter.GetCurrentKey());
    iter.Advance();
  }
}

}  // namespace

CredentialManagerClient::CredentialManagerClient()
    : routing_id_(MSG_ROUTING_NONE),
      render_thread_(content::RenderThread::Get()) {
  DCHECK(render_thread_);
}

CredentialManagerClient::~CredentialManagerClient() {
  DisconnectFromRenderThread();

  ClearCallbacksMapWithErrors(&failed_sign_in_callbacks_);
  ClearCallbacksMapWithErrors(&signed_in_callbacks_);
  ClearCallbacksMapWithErrors(&signed_out_callbacks_);
  ClearCallbacksMapWithErrors(&request_callbacks_);
}

void CredentialManagerClient::OnRenderViewCreated(
    content::RenderView* render_view) {
  render_view->GetWebView()->setCredentialManagerClient(this);
}

void CredentialManagerClient::OnRenderProcessShutdown() {
  DisconnectFromRenderThread();
}

int CredentialManagerClient::GetRoutingID() {
  if (render_thread_ && routing_id_ == MSG_ROUTING_NONE) {
    routing_id_ = render_thread_->GenerateRoutingID();
    render_thread_->AddRoute(routing_id_, this);
  }
  return routing_id_;
}

void CredentialManagerClient::DisconnectFromRenderThread() {
  if (render_thread_ && routing_id_ != MSG_ROUTING_NONE)
    render_thread_->RemoveRoute(routing_id_);
  render_thread_ = NULL;
  routing_id_ = MSG_ROUTING_NONE;
}

// -----------------------------------------------------------------------------
// Handle messages from the browser.

bool CredentialManagerClient::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CredentialManagerClient, message)
    IPC_MESSAGE_HANDLER(CredentialManagerMsg_AcknowledgeFailedSignIn,
                        OnAcknowledgeFailedSignIn)
    IPC_MESSAGE_HANDLER(CredentialManagerMsg_AcknowledgeSignedIn,
                        OnAcknowledgeSignedIn)
    IPC_MESSAGE_HANDLER(CredentialManagerMsg_AcknowledgeSignedOut,
                        OnAcknowledgeSignedOut)
    IPC_MESSAGE_HANDLER(CredentialManagerMsg_SendCredential, OnSendCredential)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CredentialManagerClient::OnAcknowledgeFailedSignIn(int request_id) {
  RespondToNotificationCallback(request_id, &failed_sign_in_callbacks_);
}

void CredentialManagerClient::OnAcknowledgeSignedIn(int request_id) {
  RespondToNotificationCallback(request_id, &signed_in_callbacks_);
}

void CredentialManagerClient::OnAcknowledgeSignedOut(int request_id) {
  RespondToNotificationCallback(request_id, &signed_out_callbacks_);
}

void CredentialManagerClient::OnSendCredential(int request_id,
                                               const CredentialInfo& info) {
  RequestCallbacks* callbacks = request_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  // TODO(mkwst): Split into local/federated credentials.
  blink::WebCredential credential(info.id, info.name, info.avatar);
  callbacks->onSuccess(&credential);
  request_callbacks_.Remove(request_id);
}

// -----------------------------------------------------------------------------
// Dispatch messages from the renderer to the browser.

void CredentialManagerClient::dispatchFailedSignIn(
    const blink::WebCredential& credential,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  int request_id = failed_sign_in_callbacks_.Add(callbacks);
  CredentialInfo info(
      credential.id(), credential.name(), credential.avatarURL());
  if (render_thread_) {
    render_thread_->Send(new CredentialManagerHostMsg_NotifyFailedSignIn(
        GetRoutingID(), request_id, info));
  }
}

void CredentialManagerClient::dispatchSignedIn(
    const blink::WebCredential& credential,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  int request_id = signed_in_callbacks_.Add(callbacks);
  CredentialInfo info(
      credential.id(), credential.name(), credential.avatarURL());
  if (render_thread_) {
    render_thread_->Send(new CredentialManagerHostMsg_NotifySignedIn(
        GetRoutingID(), request_id, info));
  }
}

void CredentialManagerClient::dispatchSignedOut(
    NotificationCallbacks* callbacks) {
  int request_id = signed_out_callbacks_.Add(callbacks);
  if (render_thread_) {
    render_thread_->Send(new CredentialManagerHostMsg_NotifySignedOut(
        GetRoutingID(), request_id));
  }
}

void CredentialManagerClient::dispatchRequest(
    bool zeroClickOnly,
    const blink::WebVector<blink::WebURL>& federations,
    RequestCallbacks* callbacks) {
  int request_id = request_callbacks_.Add(callbacks);
  std::vector<GURL> federation_vector;
  for (size_t i = 0; i < std::min(federations.size(), kMaxFederations); ++i)
    federation_vector.push_back(federations[i]);
  if (render_thread_) {
    render_thread_->Send(new CredentialManagerHostMsg_RequestCredential(
        GetRoutingID(), request_id, zeroClickOnly, federation_vector));
  }
}

void CredentialManagerClient::RespondToNotificationCallback(
    int request_id,
    CredentialManagerClient::NotificationCallbacksMap* map) {
  blink::WebCredentialManagerClient::NotificationCallbacks* callbacks =
      map->Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onSuccess();
  map->Remove(request_id);
}

}  // namespace password_manager
