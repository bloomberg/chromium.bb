// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reauth_credential.h"

#include "base/stl_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

CReauthCredential::CReauthCredential() {}

CReauthCredential::~CReauthCredential() {}

HRESULT CReauthCredential::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CReauthCredential::FinalRelease() {
  LOGFN(INFO);
}

HRESULT CReauthCredential::GetEmailForReauth(wchar_t* email, size_t length) {
  if (email == nullptr)
    return E_POINTER;

  if (length < 1)
    return E_INVALIDARG;

  errno_t err = wcsncpy_s(email, length, OLE2CW(email_for_reauth_), _TRUNCATE);
  return err == 0 ? S_OK : E_FAIL;
}

IFACEMETHODIMP CReauthCredential::SetEmailForReauth(BSTR email) {
  DCHECK(email);

  email_for_reauth_ = email;
  return S_OK;
}

HRESULT CReauthCredential::SetOSUserInfo(BSTR sid, BSTR username) {
  DCHECK(sid);
  DCHECK(username);

  set_user_sid(sid);
  set_username(username);
  return S_OK;
}

}  // namespace credential_provider
