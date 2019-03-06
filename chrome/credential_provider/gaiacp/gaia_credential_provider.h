// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_H_

#include <limits>
#include <memory>
#include <set>
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
  COM_INTERFACE_ENTRY(ICredentialProviderSetUserArray)
  COM_INTERFACE_ENTRY(ICredentialProvider)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct();
  void FinalRelease();

  // Returns true if the given usage scenario is supported by GCPW. Currently
  // only CPUS_LOGON and CPUS_UNLOCK_WORKSTATION are supported.
  static bool IsUsageScenarioSupported(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);

  // Returns true if a new user can be added in the current usage scenario. This
  // function also checks other settings controlled by registry settings to
  // determine the result of this query.
  static bool CanNewUsersBeCreated(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);

 private:
  // Checks whether anonymous reauth credentials should be created given the
  // usage scenario and also whether an "Other User" tile exists on the sign on
  // screen.
  bool ShouldCreateAnonymousReauthCredential(bool other_user_credential_exists);

  HRESULT DestroyCredentials();

  // Functions to create credentials during the processing of SetUserArray.

  // Creates necessary anonymous credentials given the state of the sign in
  // screen (currently only whether |showing_other_user| set influences this
  // behavior.
  HRESULT CreateAnonymousCredentialIfNeeded(bool showing_other_user);

  // Creates all the reauth credentials from the users that is returned from
  // |users|. Fills |reauth_sids| with the list of user sids for which a reauth
  // credential was created.
  HRESULT CreateReauthCredentials(ICredentialProviderUserArray* users,
                                  std::set<base::string16>* reauth_sids);

  // If needed, creates anonymous reauth credentials for any users that have had
  // their sign permissions revoked and thus do not appear in the users list
  // passed into SetUserArray.
  HRESULT CreateAnonymousReauthCredentialsIfNeeded(
      bool showing_other_user,
      const std::set<base::string16>& reauth_sids);

  void ClearTransient();
  void CleanupOlderVersions();

  // IGaiaCredentialProvider
  IFACEMETHODIMP GetUsageScenario(DWORD* cpus) override;
  IFACEMETHODIMP OnUserAuthenticated(IUnknown* credential,
                                     BSTR username,
                                     BSTR password,
                                     BSTR sid,
                                     BOOL fire_credentials_changed) override;

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
};

// OBJECT_ENTRY_AUTO() contains an extra semicolon.
// TODO(thakis): Make -Wextra-semi not warn on semicolons that are from a
// macro in a system header, then remove the pragma, https://llvm.org/PR40874
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#endif

OBJECT_ENTRY_AUTO(__uuidof(GaiaCredentialProvider), CGaiaCredentialProvider)

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_H_
