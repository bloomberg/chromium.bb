// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_RENDERER_CREDENTIAL_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_RENDERER_CREDENTIAL_MANAGER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerClient.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/platform/WebCredentialMediationRequirement.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/credentialmanager/credential_manager.mojom.h"

namespace blink {
class WebCredential;
class WebURL;
}

namespace content {
class RenderView;
}

namespace password_manager {

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

  // blink::WebCredentialManagerClient:
  void DispatchStore(
      const blink::WebCredential& credential,
      WebCredentialManagerClient::NotificationCallbacks* callbacks) override;
  void DispatchPreventSilentAccess(NotificationCallbacks* callbacks) override;
  void DispatchGet(blink::WebCredentialMediationRequirement mediation,
                   bool include_passwords,
                   const blink::WebVector<blink::WebURL>& federations,
                   RequestCallbacks* callbacks) override;

 private:
  // RenderViewObserver implementation.
  void OnDestruct() override;

  void ConnectToMojoCMIfNeeded();
  void OnMojoConnectionError();

  mojom::CredentialManagerAssociatedPtr mojo_cm_service_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_RENDERER_CREDENTIAL_MANAGER_CLIENT_H_
