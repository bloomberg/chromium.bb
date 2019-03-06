// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_ANONYMOUS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_ANONYMOUS_H_

#include "chrome/credential_provider/gaiacp/reauth_credential_base.h"

namespace credential_provider {

// A credential for a user that exists on the system and is associated with a
// Gaia account but is not a ICredentialProviderCredential2 so that it can
// be displayed even if the user is not passed into SetUserArray. This
// credential is created in certain cases where the user's normal access was
// revoked because their token handle is no longer valid.
class ATL_NO_VTABLE CReauthCredentialAnonymous
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CReauthCredentialBase {
 public:
  DECLARE_NO_REGISTRY()

  CReauthCredentialAnonymous();
  ~CReauthCredentialAnonymous();

  HRESULT FinalConstruct();
  void FinalRelease();

 private:
  // This class is shown when no user tile is available for the user so it does
  // not say it implements ICredentialProviderCredential2. It only implements
  // ICredentialProviderCredential.
  BEGIN_COM_MAP(CReauthCredentialAnonymous)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(IReauthCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT GetStringValueImpl(DWORD field_id, wchar_t** value) override;
  HRESULT GetBitmapValueImpl(DWORD field_id, HBITMAP* phbmp) override;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_ANONYMOUS_H_
