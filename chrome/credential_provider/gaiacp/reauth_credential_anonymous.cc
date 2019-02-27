// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reauth_credential_anonymous.h"

#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"

namespace credential_provider {

CReauthCredentialAnonymous::CReauthCredentialAnonymous() = default;

CReauthCredentialAnonymous::~CReauthCredentialAnonymous() = default;

HRESULT CReauthCredentialAnonymous::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CReauthCredentialAnonymous::FinalRelease() {
  LOGFN(INFO);
}

HRESULT CReauthCredentialAnonymous::GetStringValueImpl(DWORD field_id,
                                                       wchar_t** value) {
  if (field_id != FID_PROVIDER_LABEL)
    return CReauthCredentialBase::GetStringValueImpl(field_id, value);

  base::string16 fullname;
  HRESULT hr = OSUserManager::Get()->GetUserFullname(
      get_os_user_domain(), get_os_username(), &fullname);

  if (FAILED(hr))
    fullname = OLE2W(get_os_username());

  return ::SHStrDupW(fullname.c_str(), value);
}

}  // namespace credential_provider
