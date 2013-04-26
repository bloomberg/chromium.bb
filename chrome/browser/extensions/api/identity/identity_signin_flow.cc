// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_signin_flow.h"

#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "google_apis/gaia/gaia_constants.h"

namespace extensions {

IdentitySigninFlow::IdentitySigninFlow(Delegate* delegate, Profile* profile)
    : delegate_(delegate),
      profile_(profile) {
}

IdentitySigninFlow::~IdentitySigninFlow() {
}

void IdentitySigninFlow::Start() {
  DCHECK(delegate_);

#if defined(OS_CHROMEOS)
  // In normal mode (i.e. non-forced app mode), the user has to log out to
  // re-establish credentials. Let the global error popup handle everything.
  if (!chrome::IsRunningInForcedAppMode()) {
    delegate_->SigninFailed();
    return;
  }
#endif

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(token_service));

  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(profile_);
  login_ui_service->ShowLoginPopup();
}

void IdentitySigninFlow::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  CHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE);
  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();
  if (token_details->service() == GaiaConstants::kGaiaOAuth2LoginRefreshToken)
    delegate_->SigninSuccess(token_details->token());
}

}  // namespace extensions
