// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential.h"

#include <algorithm>
#include <memory>

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"

namespace credential_provider {

CGaiaCredential::CGaiaCredential() {}

CGaiaCredential::~CGaiaCredential() {}

HRESULT CGaiaCredential::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CGaiaCredential::FinalRelease() {
  LOGFN(INFO);
}

void CGaiaCredential::ResetInternalState() {
  // Reset sid and username so that the credential can be signed in with
  // another user in case it was cancelled in the middle of a password change
  // sign in.
  set_user_sid(CComBSTR());
  set_username(CComBSTR());

  CGaiaCredentialBase::ResetInternalState();
}

}  // namespace credential_provider
