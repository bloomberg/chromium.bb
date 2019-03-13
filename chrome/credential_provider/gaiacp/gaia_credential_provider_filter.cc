// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_filter.h"

#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

CGaiaCredentialProviderFilter::CGaiaCredentialProviderFilter() = default;

CGaiaCredentialProviderFilter::~CGaiaCredentialProviderFilter() = default;

HRESULT CGaiaCredentialProviderFilter::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CGaiaCredentialProviderFilter::FinalRelease() {
  LOGFN(INFO);
}

HRESULT CGaiaCredentialProviderFilter::Filter(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD flags,
    GUID* providers_clsids,
    BOOL* providers_allow,
    DWORD providers_count) {
  // Re-enable all users in case internet has been lost or the computer
  // crashed while users were locked out.
  AssociatedUserValidator::Get()->AllowSigninForAllAssociatedUsers(cpus);
  // Check to see if any users need to have their access to this system
  // using the normal credential providers revoked.
  AssociatedUserValidator::Get()->DenySigninForUsersWithInvalidTokenHandles(
      cpus);
  return S_OK;
}

HRESULT CGaiaCredentialProviderFilter::UpdateRemoteCredential(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_out) {
  return E_NOTIMPL;
}

}  // namespace credential_provider
