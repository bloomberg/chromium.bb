// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_client_context_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/public/resources/blimp_strings.h"
#include "blimp/client/support/resources/blimp_strings.h"
#include "blimp/client/support/session/blimp_default_identity_provider.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"

namespace blimp {
namespace client {

BlimpClientContextDelegateAndroid::BlimpClientContextDelegateAndroid(
    BlimpClientContext* blimp_client_context)
    : blimp_client_context_(blimp_client_context) {
  blimp_client_context_->SetDelegate(this);
}

BlimpClientContextDelegateAndroid::~BlimpClientContextDelegateAndroid() {
  blimp_client_context_->SetDelegate(nullptr);
}

void BlimpClientContextDelegateAndroid::AttachBlimpContentsHelpers(
    BlimpContents* blimp_contents) {}

void BlimpClientContextDelegateAndroid::OnAssignmentConnectionAttempted(
    AssignmentRequestResult result,
    const Assignment& assignment) {
  if (result == blimp::client::ASSIGNMENT_REQUEST_RESULT_OK)
    return;
  base::string16 message = blimp::string::BlimpPrefix(
      blimp::string::AssignmentResultErrorToString(result));
  ShowMessage(message);
}

std::unique_ptr<IdentityProvider>
BlimpClientContextDelegateAndroid::CreateIdentityProvider() {
  return base::MakeUnique<BlimpDefaultIdentityProvider>();
}

void BlimpClientContextDelegateAndroid::OnAuthenticationError(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "GoogleAuth error : " << error.ToString();
  base::string16 message = blimp::string::BlimpPrefix(
      l10n_util::GetStringUTF16(IDS_BLIMP_SIGNIN_GET_TOKEN_FAILED));
  ShowMessage(message);
}

void BlimpClientContextDelegateAndroid::OnConnected() {
  base::string16 message = blimp::string::BlimpPrefix(
      l10n_util::GetStringUTF16(IDS_BLIMP_NETWORK_CONNECTED));
  ShowMessage(message);
}

void BlimpClientContextDelegateAndroid::OnEngineDisconnected(int result) {
  OnDisconnected(blimp::string::EndConnectionMessageToString(result));
}

void BlimpClientContextDelegateAndroid::OnNetworkDisconnected(int result) {
  OnDisconnected(base::UTF8ToUTF16(net::ErrorToShortString(result)));
}

void BlimpClientContextDelegateAndroid::OnDisconnected(
    const base::string16& reason) {
  ShowMessage(blimp::string::BlimpPrefix(
      l10n_util::GetStringFUTF16(IDS_BLIMP_NETWORK_DISCONNECTED, reason)));
}

void BlimpClientContextDelegateAndroid::ShowMessage(
    const base::string16& message) {
  VLOG(1) << "ShowMessage: " << message;
}

}  // namespace client
}  // namespace blimp
