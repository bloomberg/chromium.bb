// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_RENDERER_CREDENTIAL_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_RENDERER_CREDENTIAL_MANAGER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "content/public/renderer/render_view_observer.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerClient.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace blink {
class WebCredential;
class WebURL;
}

namespace content {
class RenderView;
}

namespace password_manager {

struct CredentialInfo;

// The CredentialManagerClient implements the Blink platform interface
// WebCredentialManagerClient, and acts as an intermediary between Blink-side
// calls to 'navigator.credential.*' and the password manager internals which
// live in the browser process.
//
// One instance of CredentialManagerClient is created per RenderView,
// acts as RenderViewObserver so it can send messages to the browser process,
// and route responses to itself.
// Once RenderView is gone away, the instance will be deleted.
//
// Note that each RenderView's WebView holds a pointer to the
// CredentialManagerClient (set in 'OnRenderViewCreated()') but does not own it.
class CredentialManagerClient : public blink::WebCredentialManagerClient,
                                public content::RenderViewObserver {
 public:
  explicit CredentialManagerClient(content::RenderView* render_view);
  ~CredentialManagerClient() override;

  // RenderViewObserver:
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers for messages from the browser process:
  virtual void OnAcknowledgeStore(int request_id);
  virtual void OnAcknowledgeRequireUserMediation(int request_id);
  virtual void OnSendCredential(int request_id,
                                const CredentialInfo& credential_info);
  virtual void OnRejectCredentialRequest(
      int request_id,
      blink::WebCredentialManagerError error);

  // blink::WebCredentialManager:
  void dispatchStore(
      const blink::WebCredential& credential,
      WebCredentialManagerClient::NotificationCallbacks* callbacks) override;
  void dispatchRequireUserMediation(NotificationCallbacks* callbacks) override;
  void dispatchGet(bool zero_click_only,
                   bool include_passwords,
                   const blink::WebVector<blink::WebURL>& federations,
                   RequestCallbacks* callbacks) override;

 private:
  typedef IDMap<blink::WebCredentialManagerClient::RequestCallbacks,
                IDMapOwnPointer> RequestCallbacksMap;
  typedef IDMap<blink::WebCredentialManagerClient::NotificationCallbacks,
                IDMapOwnPointer> NotificationCallbacksMap;

  void RespondToNotificationCallback(int request_id,
                                     NotificationCallbacksMap* map);

  // Track the various blink::WebCredentialManagerClient::*Callbacks objects
  // generated from Blink. This class takes ownership of these objects.
  NotificationCallbacksMap store_callbacks_;
  NotificationCallbacksMap require_user_mediation_callbacks_;
  RequestCallbacksMap get_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_RENDERER_CREDENTIAL_MANAGER_CLIENT_H_
