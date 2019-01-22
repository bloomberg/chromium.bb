// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_H_

#include <limits>
#include <memory>
#include <vector>

#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"

namespace credential_provider {

// Implementation of ICredentialProvider backed by Gaia.
class ATL_NO_VTABLE CGaiaCredentialProvider
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CComCoClass<CGaiaCredentialProvider,
                         &CLSID_GaiaCredentialProvider>,
      public IGaiaCredentialProvider,
      public IGaiaCredentialProviderForTesting,
      public ICredentialProviderSetUserArray,
      public ICredentialProvider {
 public:
  // This COM object is registered with the rgs file.  The rgs file is used by
  // CGaiaCredentialProviderModule class, see latter for details.
  DECLARE_NO_REGISTRY()

  CGaiaCredentialProvider();
  ~CGaiaCredentialProvider();

  BEGIN_COM_MAP(CGaiaCredentialProvider)
  COM_INTERFACE_ENTRY(IGaiaCredentialProvider)
  COM_INTERFACE_ENTRY(IGaiaCredentialProviderForTesting)
  COM_INTERFACE_ENTRY(ICredentialProviderSetUserArray)
  COM_INTERFACE_ENTRY(ICredentialProvider)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct();
  void FinalRelease();

  static bool IsUsageScenarioSupported(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);

 private:
  HRESULT CreateGaiaCredential();
  HRESULT DestroyCredentials();
  void ClearTransient();
  void CleanupStaleTokenHandles();
  void CleanupOlderVersions();

  // Checks of any of the Google account users need to re-auth.
  static unsigned __stdcall CheckReauthStatus(void* param);

  // IGaiaCredentialProvider
  IFACEMETHODIMP OnUserAuthenticated(IUnknown* credential,
                                     BSTR username,
                                     BSTR password,
                                     BSTR sid,
                                     BOOL fire_credentials_changed) override;
  IFACEMETHODIMP HasInternetConnection() override;

  // IGaiaCredentialProviderForTesting
  IFACEMETHODIMP SetHasInternetConnection(
      HasInternetConnectionCheckType has_internet_connection) override;

  // ICredentialProviderSetUserArray
  IFACEMETHODIMP SetUserArray(ICredentialProviderUserArray* users) override;

  // ICredentialProvider
  IFACEMETHODIMP SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                                  DWORD dwFlags) override;
  IFACEMETHODIMP SetSerialization(
      const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs) override;
  IFACEMETHODIMP Advise(ICredentialProviderEvents* pcpe,
                        UINT_PTR upAdviseContext) override;
  IFACEMETHODIMP UnAdvise() override;
  IFACEMETHODIMP GetFieldDescriptorCount(DWORD* pdwCount) override;
  IFACEMETHODIMP GetFieldDescriptorAt(
      DWORD dwIndex,
      CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd) override;
  IFACEMETHODIMP GetCredentialCount(DWORD* pdwCount,
                                    DWORD* pdwDefault,
                                    BOOL* pbAutoLogonWithDefault) override;
  IFACEMETHODIMP GetCredentialAt(
      DWORD dwIndex,
      ICredentialProviderCredential** ppcpc) override;

  CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus_ = CPUS_INVALID;
  DWORD cpus_flags_ = 0;
  UINT_PTR advise_context_;
  CComPtr<ICredentialProviderEvents> events_;

  // List of credentials exposed by this provider.  The first is always the
  // Gaia credential for creating new users.  The rest are reauth credentials.
  std::vector<CComPtr<IGaiaCredential>> users_;

  // SID of the user that was authenticated.
  CComBSTR new_user_sid_;

  // Index in the |users_| array of the credential that performed the
  // authentication.
  size_t index_ = std::numeric_limits<size_t>::max();

  // Used during tests to force the credential provider to believe if an
  // internet connection is possible or not.  In production the value is
  // always set to HIC_CHECK_ALWAYS to perform a real check at runtime.
  HasInternetConnectionCheckType has_internet_connection_ = kHicCheckAlways;
};

OBJECT_ENTRY_AUTO(__uuidof(GaiaCredentialProvider), CGaiaCredentialProvider)

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_H_
