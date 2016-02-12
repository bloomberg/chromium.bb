// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/renderer/credential_manager_client.h"

#include <stddef.h>

#include "components/password_manager/content/common/credential_manager_content_utils.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/platform/WebFederatedCredential.h"
#include "third_party/WebKit/public/platform/WebPassOwnPtr.h"
#include "third_party/WebKit/public/platform/WebPasswordCredential.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace password_manager {

namespace {

template <typename T>
void ClearCallbacksMapWithErrors(T* callbacks_map) {
  typename T::iterator iter(callbacks_map);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->onError(blink::WebCredentialManagerUnknownError);
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
  ClearCallbacksMapWithErrors(&store_callbacks_);
  ClearCallbacksMapWithErrors(&require_user_mediation_callbacks_);
  ClearCallbacksMapWithErrors(&get_callbacks_);
}

// -----------------------------------------------------------------------------
// Handle messages from the browser.

bool CredentialManagerClient::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CredentialManagerClient, message)
    IPC_MESSAGE_HANDLER(CredentialManagerMsg_AcknowledgeStore,
                        OnAcknowledgeStore)
    IPC_MESSAGE_HANDLER(CredentialManagerMsg_AcknowledgeRequireUserMediation,
                        OnAcknowledgeRequireUserMediation)
    IPC_MESSAGE_HANDLER(CredentialManagerMsg_SendCredential, OnSendCredential)
    IPC_MESSAGE_HANDLER(CredentialManagerMsg_RejectCredentialRequest,
                        OnRejectCredentialRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CredentialManagerClient::OnAcknowledgeStore(int request_id) {
  RespondToNotificationCallback(request_id, &store_callbacks_);
}

void CredentialManagerClient::OnAcknowledgeRequireUserMediation(
    int request_id) {
  RespondToNotificationCallback(request_id, &require_user_mediation_callbacks_);
}

void CredentialManagerClient::OnSendCredential(int request_id,
                                               const CredentialInfo& info) {
  RequestCallbacks* callbacks = get_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  scoped_ptr<blink::WebCredential> credential = nullptr;
  switch (info.type) {
    case CredentialType::CREDENTIAL_TYPE_FEDERATED:
      credential.reset(new blink::WebFederatedCredential(
          info.id, info.federation, info.name, info.icon));
      break;
    case CredentialType::CREDENTIAL_TYPE_PASSWORD:
      credential.reset(new blink::WebPasswordCredential(
          info.id, info.password, info.name, info.icon));
      break;
    case CredentialType::CREDENTIAL_TYPE_EMPTY:
      // Intentionally empty; we'll send nullptr to the onSuccess call below.
      break;
  }
  callbacks->onSuccess(adoptWebPtr(credential.release()));
  get_callbacks_.Remove(request_id);
}

void CredentialManagerClient::OnRejectCredentialRequest(
    int request_id,
    blink::WebCredentialManagerError error) {
  RequestCallbacks* callbacks = get_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onError(error);
  get_callbacks_.Remove(request_id);
}

// -----------------------------------------------------------------------------
// Dispatch messages from the renderer to the browser.

void CredentialManagerClient::dispatchStore(
    const blink::WebCredential& credential,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  int request_id = store_callbacks_.Add(callbacks);
  CredentialInfo info(WebCredentialToCredentialInfo(credential));
  Send(new CredentialManagerHostMsg_Store(routing_id(), request_id, info));
}

void CredentialManagerClient::dispatchRequireUserMediation(
    NotificationCallbacks* callbacks) {
  int request_id = require_user_mediation_callbacks_.Add(callbacks);
  Send(new CredentialManagerHostMsg_RequireUserMediation(routing_id(),
                                                         request_id));
}

void CredentialManagerClient::dispatchGet(
    bool zero_click_only,
    bool include_passwords,
    const blink::WebVector<blink::WebURL>& federations,
    RequestCallbacks* callbacks) {
  int request_id = get_callbacks_.Add(callbacks);
  std::vector<GURL> federation_vector;
  for (size_t i = 0; i < std::min(federations.size(), kMaxFederations); ++i)
    federation_vector.push_back(federations[i]);
  Send(new CredentialManagerHostMsg_RequestCredential(
      routing_id(), request_id, zero_click_only, include_passwords,
      federation_vector));
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
