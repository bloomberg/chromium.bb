// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "device/fido/fido_request_handler_base.h"

class Profile;

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace device {
class FidoAuthenticator;
}

class ChromeAuthenticatorRequestDelegate
    : public content::AuthenticatorRequestClientDelegate,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#if defined(OS_MACOSX)
  static TouchIdAuthenticatorConfig TouchIdAuthenticatorConfigForProfile(
      Profile* profile);
#endif  // defined(OS_MACOSX)

  // The |render_frame_host| must outlive this instance.
  explicit ChromeAuthenticatorRequestDelegate(
      content::RenderFrameHost* render_frame_host);
  ~ChromeAuthenticatorRequestDelegate() override;

#if defined(OS_MACOSX)
  base::Optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig()
      const override;
#endif  // defined(OS_MACOSX)

  base::WeakPtr<ChromeAuthenticatorRequestDelegate> AsWeakPtr();

 private:
  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }
  content::BrowserContext* browser_context() const;

  // content::AuthenticatorRequestClientDelegate:
  void DidStartRequest() override;
  bool ShouldPermitIndividualAttestation(
      const std::string& relying_party_id) override;
  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      base::OnceCallback<void(bool)> callback) override;
  bool IsFocused() override;
  void BluetoothAdapterIsAvailable() override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(base::StringPiece device_id) override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed() override;

  content::RenderFrameHost* const render_frame_host_;
  AuthenticatorRequestDialogModel* weak_dialog_model_ = nullptr;

  base::WeakPtrFactory<ChromeAuthenticatorRequestDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAuthenticatorRequestDelegate);
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
