// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager_android.h"

#include "base/logging.h"

namespace signin {

ConsistencyCookieManagerAndroid::ConsistencyCookieManagerAndroid(
    SigninClient* signin_client,
    AccountReconcilor* reconcilor)
    : account_reconcilor_state_(reconcilor->GetState()),
      signin_client_(signin_client),
      account_reconcilor_observer_(this) {
  DCHECK(signin_client_);
  DCHECK(reconcilor);
  account_reconcilor_observer_.Add(reconcilor);
  UpdateCookie();
}

ConsistencyCookieManagerAndroid::~ConsistencyCookieManagerAndroid() = default;

void ConsistencyCookieManagerAndroid::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  if (state == account_reconcilor_state_)
    return;
  account_reconcilor_state_ = state;
  UpdateCookie();
}

void ConsistencyCookieManagerAndroid::UpdateCookie() {
  // TODO(droger): update the cookie.
}

}  // namespace signin
