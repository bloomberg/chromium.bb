// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_signin_flow.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_context.h"
#include "services/identity/public/cpp/account_state.h"
#include "services/identity/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace extensions {

IdentitySigninFlow::IdentitySigninFlow(Delegate* delegate, Profile* profile)
    : delegate_(delegate), profile_(profile) {}

IdentitySigninFlow::~IdentitySigninFlow() {}

void IdentitySigninFlow::Start() {
  DCHECK(delegate_);

#if defined(OS_CHROMEOS)
  // In normal mode (i.e. non-kiosk mode), the user has to log out to
  // re-establish credentials. Let the global error popup handle everything.
  // In kiosk mode, interactive sign-in is not supported.
  delegate_->SigninFailed();
  return;
#endif

  content::BrowserContext::GetConnectorFor(profile_)->BindInterface(
      ::identity::mojom::kServiceName, mojo::MakeRequest(&identity_manager_));
  identity_manager_->GetPrimaryAccountWhenAvailable(base::Bind(
      &IdentitySigninFlow::OnPrimaryAccountAvailable, base::Unretained(this)));

  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(profile_);
  login_ui_service->ShowLoginPopup();
}

void IdentitySigninFlow::OnPrimaryAccountAvailable(
    const AccountInfo& account_info,
    const identity::AccountState& account_state) {
  delegate_->SigninSuccess();
}

}  // namespace extensions
