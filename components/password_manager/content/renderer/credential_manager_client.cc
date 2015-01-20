// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/renderer/credential_manager_client.h"

#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/platform/WebFederatedCredential.h"
#include "third_party/WebKit/public/platform/WebLocalCredential.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace password_manager {

namespace {

template <typename T>
void ClearCallbacksMapWithErrors(T* callbacks_map) {
  typename T::iterator iter(callbacks_map);
  while (!iter.IsAtEnd()) {
    blink::WebCredentialManagerError reason(
        blink::WebCredentialManagerError::ErrorTypeUnknown);
    iter.GetCurrentValue()->onError(&reason);
    callbacks_map->Remove(iter.GetCurrentKey());
    iter.Advance();
  }
}

}  // namespace

CredentialManagerClient::CredentialManagerClient(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
  render_view->GetWebView()->setCredentialManagerClient(this);
}

CredentialManagerClient::~CredentialManagerClient() {
  ClearCallbacksMapWithErrors(&failed_sign_in_callbacks_);
  ClearCallbacksMapWithErrors(&signed_in_callbacks_);
  ClearCallbacksMapWithErrors(&signed_out_callbacks_);
  ClearCallbacksMapWithErrors(&request_callbacks_);
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
    IPC_MESSAGE_HANDLER(CredentialManagerMsg_RejectCredentialRequest,
                        OnRejectCredentialRequest)
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
  scoped_ptr<blink::WebCredential> credential = nullptr;
  switch (info.type) {
    case CredentialType::CREDENTIAL_TYPE_FEDERATED:
      credential.reset(new blink::WebFederatedCredential(
          info.id, info.name, info.avatar, info.federation));
      break;
    case CredentialType::CREDENTIAL_TYPE_LOCAL:
      credential.reset(new blink::WebLocalCredential(
          info.id, info.name, info.avatar, info.password));
      break;
    case CredentialType::CREDENTIAL_TYPE_EMPTY:
      // Intentionally empty; we'll send nullptr to the onSuccess call below.
      break;
  }
  callbacks->onSuccess(credential.get());
  request_callbacks_.Remove(request_id);
}

void CredentialManagerClient::OnRejectCredentialRequest(
    int request_id,
    blink::WebCredentialManagerError::ErrorType error_type) {
  RequestCallbacks* callbacks = request_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  scoped_ptr<blink::WebCredentialManagerError> error(
      new blink::WebCredentialManagerError(error_type));
  callbacks->onError(error.get());
  request_callbacks_.Remove(request_id);
}

// -----------------------------------------------------------------------------
// Dispatch messages from the renderer to the browser.

void CredentialManagerClient::dispatchFailedSignIn(
    const blink::WebCredential& credential,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  int request_id = failed_sign_in_callbacks_.Add(callbacks);
  CredentialInfo info(credential);
  Send(new CredentialManagerHostMsg_NotifyFailedSignIn(
      routing_id(), request_id, info));
}

void CredentialManagerClient::dispatchSignedIn(
    const blink::WebCredential& credential,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  int request_id = signed_in_callbacks_.Add(callbacks);
  CredentialInfo info(credential);
  Send(new CredentialManagerHostMsg_NotifySignedIn(
      routing_id(), request_id, info));
}

void CredentialManagerClient::dispatchSignedOut(
    NotificationCallbacks* callbacks) {
  int request_id = signed_out_callbacks_.Add(callbacks);
  Send(new CredentialManagerHostMsg_NotifySignedOut(routing_id(), request_id));
}

void CredentialManagerClient::dispatchRequest(
    bool zeroClickOnly,
    const blink::WebVector<blink::WebURL>& federations,
    RequestCallbacks* callbacks) {
  int request_id = request_callbacks_.Add(callbacks);
  std::vector<GURL> federation_vector;
  for (size_t i = 0; i < std::min(federations.size(), kMaxFederations); ++i)
    federation_vector.push_back(federations[i]);
  Send(new CredentialManagerHostMsg_RequestCredential(
      routing_id(), request_id, zeroClickOnly, federation_vector));
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
