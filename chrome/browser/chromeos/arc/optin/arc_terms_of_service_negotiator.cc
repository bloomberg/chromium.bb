// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_negotiator.h"

#include "base/callback_helpers.h"

namespace arc {

ArcTermsOfServiceNegotiator::ArcTermsOfServiceNegotiator() = default;

ArcTermsOfServiceNegotiator::~ArcTermsOfServiceNegotiator() = default;

void ArcTermsOfServiceNegotiator::StartNegotiation(
    const NegotiationCallback& callback) {
  DCHECK(pending_callback_.is_null());
  pending_callback_ = callback;
  StartNegotiationImpl();
}

void ArcTermsOfServiceNegotiator::ReportResult(bool accepted) {
  DCHECK(!pending_callback_.is_null());
  base::ResetAndReturn(&pending_callback_).Run(accepted);
}

}  // namespace arc
