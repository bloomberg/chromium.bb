// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/identity_internals/identity_internals_token_revoker.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/identity_internals/identity_internals_ui_handler.h"
#include "google_apis/gaia/gaia_constants.h"

IdentityInternalsTokenRevoker::IdentityInternalsTokenRevoker(
    const std::string& extension_id,
    const std::string& access_token,
    const mojo::Callback<void()>& callback,
    Profile* profile,
    IdentityInternalsUIHandler* consumer)
    : fetcher_(this, GaiaConstants::kChromeSource,
               profile->GetRequestContext()),
      extension_id_(extension_id),
      access_token_(access_token),
      callback_(callback),
      consumer_(consumer) {
  DCHECK(consumer_);
  fetcher_.StartRevokeOAuth2Token(access_token);
}

IdentityInternalsTokenRevoker::~IdentityInternalsTokenRevoker() {}

void IdentityInternalsTokenRevoker::OnOAuth2RevokeTokenCompleted() {
  consumer_->OnTokenRevokerDone(this);
}
