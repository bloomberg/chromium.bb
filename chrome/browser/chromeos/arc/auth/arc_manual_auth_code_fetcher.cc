// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/auth/arc_manual_auth_code_fetcher.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/arc/arc_auth_context.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"

namespace arc {

ArcManualAuthCodeFetcher::ArcManualAuthCodeFetcher(ArcAuthContext* context,
                                                   ArcSupportHost* support_host)
    : context_(context), support_host_(support_host), weak_ptr_factory_(this) {
  DCHECK(context_);
  DCHECK(support_host_);
  support_host_->AddObserver(this);
}

ArcManualAuthCodeFetcher::~ArcManualAuthCodeFetcher() {
  support_host_->RemoveObserver(this);
}

void ArcManualAuthCodeFetcher::Fetch(const FetchCallback& callback) {
  DCHECK(pending_callback_.is_null());
  pending_callback_ = callback;

  FetchInternal();
}

void ArcManualAuthCodeFetcher::FetchInternal() {
  DCHECK(!pending_callback_.is_null());
  context_->Prepare(base::Bind(&ArcManualAuthCodeFetcher::OnContextPrepared,
                               weak_ptr_factory_.GetWeakPtr()));
}

void ArcManualAuthCodeFetcher::OnContextPrepared(
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK(!pending_callback_.is_null());
  if (!request_context_getter) {
    UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
    support_host_->ShowError(ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR,
                             false);
    return;
  }

  support_host_->ShowLso();
}

void ArcManualAuthCodeFetcher::OnAuthSucceeded(const std::string& auth_code) {
  DCHECK(!pending_callback_.is_null());
  base::ResetAndReturn(&pending_callback_).Run(auth_code);
}

void ArcManualAuthCodeFetcher::OnAuthFailed() {
  // Don't report via callback. Extension is already showing more detailed
  // information. Update only UMA here.
  UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
}

void ArcManualAuthCodeFetcher::OnRetryClicked() {
  DCHECK(!pending_callback_.is_null());
  FetchInternal();
}

}  // namespace arc
