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

HRESULT CGaiaCredential::FinishAuthentication(BSTR username,
                                              BSTR password,
                                              BSTR fullname,
                                              BSTR* sid,
                                              BSTR* error_text) {
  LOGFN(INFO);
  DCHECK(error_text);

  OSUserManager* manager = OSUserManager::Get();
  base::string16 comment(GetStringResource(IDS_USER_ACCOUNT_COMMENT));
  HRESULT hr = CreateNewUser(manager, OLE2CW(username), OLE2CW(password),
                     OLE2CW(fullname), comment.c_str(), true, sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateNewUser hr=" << putHR(hr)
                 << " account=" << OLE2CW(username);
    *error_text = AllocErrorString(IDS_CANT_CREATE_USER);
    return hr;
  }

  return S_OK;
}

}  // namespace credential_provider
