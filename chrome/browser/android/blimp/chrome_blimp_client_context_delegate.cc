// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/blimp/chrome_blimp_client_context_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
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
#include "net/base/net_errors.h"

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
    const blimp::client::Assignment& assignment) {
  if (result == blimp::client::ASSIGNMENT_REQUEST_RESULT_OK)
    return;

  // TODO(xingliu): All strings shown in the UI should be accessed through grd
  // files. https://crbug.com/630687
  std::stringstream ss;
  ss << "Blimp: Assignment failed, reason: " << result << ".";
  ShowMessage(base::UTF8ToUTF16(ss.str()), false);
}

std::unique_ptr<IdentityProvider>
ChromeBlimpClientContextDelegate::CreateIdentityProvider() {
  return base::WrapUnique<IdentityProvider>(new ProfileIdentityProvider(
      SigninManagerFactory::GetForProfile(profile_),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
      base::Closure()));
}

void ChromeBlimpClientContextDelegate::OnAuthenticationError(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "GoogleAuth error : " << error.ToString();
  ShowMessage(base::UTF8ToUTF16("Blimp: Failed to get OAuth2 token."), false);
}

void ChromeBlimpClientContextDelegate::OnConnected() {
  ShowMessage(base::UTF8ToUTF16("Blimp: Connected."), true);
}

void ChromeBlimpClientContextDelegate::OnEngineDisconnected(int result) {
  OnDisconnected(base::UTF8ToUTF16(base::IntToString(result)));
}

void ChromeBlimpClientContextDelegate::OnNetworkDisconnected(int result) {
  OnDisconnected(base::UTF8ToUTF16(net::ErrorToShortString(result)));
}

void ChromeBlimpClientContextDelegate::ShowMessage(
    const base::string16& message,
    bool short_message) {}

void ChromeBlimpClientContextDelegate::OnDisconnected(
    const base::string16& reason) {
  std::stringstream ss;
  ss << "Blimp: Disconnected(" << reason << ").";
  ShowMessage(base::UTF8ToUTF16(ss.str()), false);
}
