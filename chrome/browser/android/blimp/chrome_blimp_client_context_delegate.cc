// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/blimp/chrome_blimp_client_context_delegate.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "chrome/browser/android/blimp/blimp_client_context_factory.h"
#include "chrome/browser/android/blimp/blimp_contents_profile_attachment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "components/signin/core/browser/signin_manager.h"

ChromeBlimpClientContextDelegate::ChromeBlimpClientContextDelegate(
    Profile* profile)
    : profile_(profile),
      blimp_client_context_(
          BlimpClientContextFactory::GetForBrowserContext(profile)) {
  blimp_client_context_->SetDelegate(this);
}

ChromeBlimpClientContextDelegate::~ChromeBlimpClientContextDelegate() {
  blimp_client_context_->SetDelegate(nullptr);
}

void ChromeBlimpClientContextDelegate::AttachBlimpContentsHelpers(
    blimp::client::BlimpContents* blimp_contents) {
  AttachProfileToBlimpContents(blimp_contents, profile_);
}

void ChromeBlimpClientContextDelegate::OnAssignmentConnectionAttempted(
    blimp::client::AssignmentRequestResult result,
    const blimp::client::Assignment& assignment) {}

std::unique_ptr<IdentityProvider>
ChromeBlimpClientContextDelegate::CreateIdentityProvider() {
  return base::WrapUnique<IdentityProvider>(new ProfileIdentityProvider(
      SigninManagerFactory::GetForProfile(profile_),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
      base::Closure()));
}

void ChromeBlimpClientContextDelegate::OnAuthenticationError(
    BlimpClientContextDelegate::AuthError error) {
  switch (error) {
    case AuthError::NOT_SIGNED_IN:
      // User need to signed in first, on Android it's handled in Java layer
      // before the connection.
      VLOG(1) << "User is not signed in before connection to Blimp.";
      break;
    case AuthError::OAUTH_TOKEN_FAIL:
      // TODO(xingliu): Alert the user in UI.
      // There is an known issue that ongoing request can be cancelled when
      // Android layer updates the refresh token.
      break;
    default:
      LOG(WARNING) << "Unknown Blimp error.";
  }
}
