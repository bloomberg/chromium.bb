// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"

#include <netlistmgr.h>

#include <iomanip>
#include <map>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reauth_credential.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

#define W2CW(p) const_cast<wchar_t*>(p)

static const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR g_field_desc[] = {
    {FID_DESCRIPTION, CPFT_LARGE_TEXT, W2CW(L"Description"), GUID_NULL},
    {FID_CURRENT_PASSWORD_FIELD, CPFT_PASSWORD_TEXT, W2CW(L"Windows Password"),
     GUID_NULL},
    {FID_SUBMIT, CPFT_SUBMIT_BUTTON, W2CW(L"Submit button"), GUID_NULL},
    {FID_PROVIDER_LOGO, CPFT_TILE_IMAGE, W2CW(L"Provider logo"),
     CPFG_CREDENTIAL_PROVIDER_LOGO},
    {FID_PROVIDER_LABEL, CPFT_LARGE_TEXT, W2CW(L"Provider label"),
     CPFG_CREDENTIAL_PROVIDER_LABEL},
};

static_assert(base::size(g_field_desc) == FIELD_COUNT,
              "g_field_desc does not match FIELDID enum");

CGaiaCredentialProvider::CGaiaCredentialProvider() {}

CGaiaCredentialProvider::~CGaiaCredentialProvider() {}

HRESULT CGaiaCredentialProvider::FinalConstruct() {
  LOGFN(INFO);
  CleanupStaleTokenHandles();
  CleanupOlderVersions();
  return S_OK;
}

void CGaiaCredentialProvider::FinalRelease() {
  LOGFN(INFO);
  ClearTransient();
}

HRESULT CGaiaCredentialProvider::CreateGaiaCredential() {
  if (users_.size() > 0) {
    LOG(ERROR) << "Users should be empty";
    return E_UNEXPECTED;
  }

  CComPtr<IGaiaCredential> cred;
  HRESULT hr = CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
      nullptr, IID_IGaiaCredential, (void**)&cred);
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not create credential hr=" << putHR(hr);
    return hr;
  }

  hr = cred->Initialize(this);
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not initialize credential hr=" << putHR(hr);
    return hr;
  }

  users_.emplace_back(cred);
  return S_OK;
}

HRESULT CGaiaCredentialProvider::DestroyCredentials() {
  LOGFN(INFO);
  for (auto it = users_.begin(); it != users_.end(); ++it)
    (*it)->Terminate();

  users_.clear();
  return S_OK;
}

void CGaiaCredentialProvider::ClearTransient() {
  LOGFN(INFO);
  // Reset event support.
  advise_context_ = 0;
  events_.Release();
  new_user_sid_.Empty();
  index_ = std::numeric_limits<size_t>::max();
}

void CGaiaCredentialProvider::CleanupStaleTokenHandles() {
  LOGFN(INFO);
  std::map<base::string16, base::string16> handles;

  HRESULT hr = GetUserTokenHandles(&handles);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetUserTokenHandles hr=" << putHR(hr);
    return;
  }

  OSUserManager* manager = OSUserManager::Get();
  for (auto it = handles.cbegin(); it != handles.cend(); ++it) {
    HRESULT hr = manager->FindUserBySID(it->first.c_str(), nullptr, 0);
    if (hr == HRESULT_FROM_WIN32(ERROR_NONE_MAPPED)) {
      RemoveAllUserProperties(it->first.c_str());
    } else if (FAILED(hr)) {
      LOGFN(ERROR) << "manager->FindUserBySID hr=" << putHR(hr);
    }
  }
}

void CGaiaCredentialProvider::CleanupOlderVersions() {
  base::FilePath versions_directory = GetInstallDirectory();
  if (!versions_directory.empty())
    DeleteVersionsExcept(versions_directory, TEXT(CHROME_VERSION_STRING));
}

// Static.
bool CGaiaCredentialProvider::IsUsageScenarioSupported(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
  return cpus == CPUS_LOGON || cpus == CPUS_UNLOCK_WORKSTATION;
}

// IGaiaCredentialProvider ////////////////////////////////////////////////////

HRESULT CGaiaCredentialProvider::OnUserAuthenticated(
    IUnknown* credential,
    BSTR /*username*/,
    BSTR /*password*/,
    BSTR sid,
    BOOL fire_credentials_changed) {
  DCHECK(!credential || sid);

  // |credential| should be in the |users_|.  Find its index.
  index_ = std::numeric_limits<size_t>::max();
  for (size_t i = 0; i < users_.size(); ++i) {
    if (users_[i].IsEqualObject(credential)) {
      index_ = i;
      break;
    }
  }
  if (index_ == std::numeric_limits<size_t>::max()) {
    LOGFN(INFO) << "Could not find credential";
    return E_INVALIDARG;
  }

  new_user_sid_ = sid;

  // Tell winlogon.exe that credential info has changed.  This provider will
  // make the newly created user the default login credential with auto
  // logon enabled.  See GetCredentialCount() for more details.
  HRESULT hr = S_OK;
  if (events_ && fire_credentials_changed)
    hr = events_->CredentialsChanged(advise_context_);

  LOGFN(INFO) << "hr=" << putHR(hr) << " sid=" << new_user_sid_.m_str
              << " index=" << index_;
  return hr;
}

HRESULT CGaiaCredentialProvider::HasInternetConnection() {
  if (has_internet_connection_ != kHicCheckAlways)
    return has_internet_connection_ == kHicForceYes ? S_OK : S_FALSE;

  // If any errors occur, return that internet connection is available.  At
  // worst the credential provider will try to connect and fail.

  CComPtr<INetworkListManager> manager;
  HRESULT hr = manager.CoCreateInstance(CLSID_NetworkListManager);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CoCreateInstance(NetworkListManager) hr=" << putHR(hr);
    return S_OK;
  }

  VARIANT_BOOL is_connected;
  hr = manager->get_IsConnectedToInternet(&is_connected);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "manager->get_IsConnectedToInternet hr=" << putHR(hr);
    return S_OK;
  }

  // Normally VARIANT_TRUE/VARIANT_FALSE are used with the type VARIANT_BOOL
  // but in this case the docs explicitly say to use FALSE.
  // https://docs.microsoft.com/en-us/windows/desktop/api/Netlistmgr/
  //     nf-netlistmgr-inetworklistmanager-get_isconnectedtointernet
  return is_connected != FALSE ? S_OK : S_FALSE;
}

// IGaiaCredentialProviderForTesting //////////////////////////////////////////

HRESULT CGaiaCredentialProvider::SetHasInternetConnection(
    HasInternetConnectionCheckType has_internet_connection) {
  has_internet_connection_ = has_internet_connection;
  return S_OK;
}

// ICredentialProvider ////////////////////////////////////////////////////////

HRESULT CGaiaCredentialProvider::SetUserArray(
    ICredentialProviderUserArray* users) {
  LOGFN(INFO);
  std::map<base::string16, base::string16> sid_to_username;

  // Get the SIDs of all users being shown in the logon UI.
  {
    if (!users) {
      LOGFN(ERROR) << "hr=" << putHR(E_INVALIDARG);
      return E_INVALIDARG;
    }

    HRESULT hr = users->SetProviderFilter(Identity_LocalUserProvider);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "users->SetProviderFilter hr=" << putHR(hr);
      return hr;
    }

    DWORD count;
    hr = users->GetCount(&count);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "users->GetCount hr=" << putHR(hr);
      return hr;
    }

    LOGFN(INFO) << "count=" << count;

    for (DWORD i = 0; i < count; ++i) {
      CComPtr<ICredentialProviderUser> user;
      hr = users->GetAt(i, &user);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "users->GetAt hr=" << putHR(hr);
        return hr;
      }

      wchar_t* sid = nullptr;
      wchar_t* username = nullptr;

      hr = user->GetSid(&sid);
      if (SUCCEEDED(hr))
        hr = user->GetStringValue(PKEY_Identity_UserName, &username);

      if (SUCCEEDED(hr)) {
        sid_to_username.emplace(sid, username);
      } else {
        LOGFN(ERROR) << "Can't get sid or username hr=" << putHR(hr);
      }

      ::CoTaskMemFree(username);
      ::CoTaskMemFree(sid);
    }
  }

  // For each SID, check to see if this user requires reauth.
  for (const auto& kv : sid_to_username) {
    // Get the user's email address.  If not found, proceed anyway.  The net
    // effect is that the user will need to enter their email address
    // manually instead of it being pre-filled.  Need to see if it would be
    // better to just fail.
    wchar_t email[64];
    ULONG length = base::size(email);
    HRESULT hr = GetUserProperty(kv.first.c_str(), kUserEmail, email, &length);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "GetUserProperty(" << kv.first << ", email)"
                   << " hr=" << putHR(hr);
      email[0] = 0;
      continue;
    }

    LOGFN(INFO) << "Existing gaia user: sid=" << kv.first
                << " user=" << kv.second << " email=" << email;
    CComPtr<IGaiaCredential> cred;
    hr = CComCreator<CComObject<CReauthCredential>>::CreateInstance(
        nullptr, IID_IGaiaCredential, (void**)&cred);

    hr = cred->Initialize(this);
    if (FAILED(hr)) {
      LOG(ERROR) << "Could not initialize credential hr=" << putHR(hr);
      return hr;
    }

    CComPtr<IReauthCredential> reauth;
    reauth = cred;
    hr = reauth->SetOSUserInfo(CComBSTR(W2COLE(kv.first.c_str())),
                               CComBSTR(W2COLE(kv.second.c_str())));
    if (FAILED(hr)) {
      LOG(ERROR) << "reauth->SetOSUserInfo hr=" << putHR(hr);
      return hr;
    }

    hr = reauth->SetEmailForReauth(CComBSTR(email));
    if (FAILED(hr)) {
      LOG(ERROR) << "reauth->SetEmailForReauth hr=" << putHR(hr);
      return hr;
    }

    users_.emplace_back(cred);
  }

  return S_OK;
}

// ICredentialProvider ////////////////////////////////////////////////////////

HRESULT CGaiaCredentialProvider::SetUsageScenario(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD flags) {
  ClearTransient();

  cpus_ = cpus;
  cpus_flags_ = flags;

  // This credential provider only supports signing in and unlocking the screen.
  HRESULT hr =
      IsUsageScenarioSupported(cpus) ? CreateGaiaCredential() : E_NOTIMPL;

  LOGFN(INFO) << "hr=" << putHR(hr) << " cpu=" << cpus
              << " flags=" << std::setbase(16) << flags;
  return hr;
}

HRESULT CGaiaCredentialProvider::SetSerialization(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs) {
  DCHECK(pcpcs);

  // NOTE: we only need to support this method if we want to support the
  // CredUI or support remote login with gaia creds.  We are likely to want
  // to support both, but not for prototype.
  if (pcpcs->clsidCredentialProvider == CLSID_GaiaCredentialProvider) {
    // Unmarshall the serialized buffer and fill in partial fields as needed.
    // Examine cpusflags for things like admin-only, cred-in-only, etc.
    LOGFN(INFO) << " authpkg=" << pcpcs->ulAuthenticationPackage
                << " setsize=" << pcpcs->cbSerialization;
  }

  return S_OK;
}

HRESULT CGaiaCredentialProvider::Advise(ICredentialProviderEvents* pcpe,
                                        UINT_PTR context) {
  DCHECK(pcpe);

  bool had_previous = events_.p != nullptr;
  events_ = pcpe;
  advise_context_ = context;
  LOGFN(INFO) << " had=" << had_previous;
  return S_OK;
}

HRESULT CGaiaCredentialProvider::UnAdvise() {
  ClearTransient();
  HRESULT hr = DestroyCredentials();
  LOGFN(INFO) << "hr=" << putHR(hr);

  // Delete the startup sentinel file if any still exists. It can still exist
  // in 2 cases:

  // 1. The UnAdvise should only occur after the user has logged in, so if
  // they never selected any gaia credential and just used normal credentials
  // this function will be called in that situation and it is guaranteed that
  // the user has at least been able provide some input to winlogon.
  // 2. When no usage scenario is supported, none of the credentials will be
  // selected and thus the gcpw startup sentinel file will not be deleted.
  // So in the case where the user is asked for CPUS_CRED_UI enough times,
  // the sentinel file size will keep growing without being deleted and
  // eventually GCPW will be disabled completely. In the unsupported usage
  // scenario, FinalRelease will be called shortly after SetUsageScenario
  // if the function returns E_NOTIMPL so try to catch potential crashes
  // of the destruction of the provider when it is not used because
  // crashes in this case will prevent the cred ui from coming up and not
  // allow the user to access their desired resource.
  DeleteStartupSentinel();

  return S_OK;
}

HRESULT CGaiaCredentialProvider::GetFieldDescriptorCount(DWORD* count) {
  *count = FIELD_COUNT;
  LOGFN(INFO) << " count=" << *count;
  return S_OK;
}

HRESULT CGaiaCredentialProvider::GetFieldDescriptorAt(
    DWORD index,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd) {
  *ppcpfd = nullptr;
  HRESULT hr = E_INVALIDARG;
  if (index < FIELD_COUNT) {
    // Always return a CoTask copy of the structure as well as any strings
    // pointed to by that structure.
    *ppcpfd = reinterpret_cast<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*>(
        ::CoTaskMemAlloc(sizeof(**ppcpfd)));
    if (*ppcpfd) {
      **ppcpfd = g_field_desc[index];
      if ((*ppcpfd)->pszLabel) {
        hr = ::SHStrDupW((*ppcpfd)->pszLabel, &(*ppcpfd)->pszLabel);
      } else {
        (*ppcpfd)->pszLabel = nullptr;
        hr = S_OK;
      }
    }

    if (FAILED(hr)) {
      ::CoTaskMemFree(*ppcpfd);
      *ppcpfd = nullptr;
    }
  }

  LOGFN(INFO) << "hr=" << putHR(hr) << " index=" << index;
  return hr;
}

HRESULT CGaiaCredentialProvider::GetCredentialCount(
    DWORD* count,
    DWORD* default_index,
    BOOL* autologin_with_default) {
  // NOTE: assumes SetUserArray() is called before this.
  *count = users_.size();
  *default_index = CREDENTIAL_PROVIDER_NO_DEFAULT;
  *autologin_with_default = false;

  // If a user was authenticated, winlogon was notified of credentials changes
  // and is re-enumerating the credentials.  Make sure autologin is enabled.
  if (index_ < users_.size() && new_user_sid_.Length() > 0) {
    *default_index = index_;
    *autologin_with_default = true;
  }

  LOGFN(INFO) << " count=" << *count << " default=" << *default_index
              << " auto=" << *autologin_with_default;
  return S_OK;
}

HRESULT CGaiaCredentialProvider::GetCredentialAt(
    DWORD index,
    ICredentialProviderCredential** ppcpc) {
  HRESULT hr = E_INVALIDARG;
  if (!ppcpc || index >= users_.size()) {
    LOG(ERROR) << "hr=" << putHR(hr) << " index=" << index;
    return hr;
  }

  *ppcpc = nullptr;
  hr = users_[index]->QueryInterface(IID_ICredentialProviderCredential,
                                     (void**)ppcpc);

  LOGFN(INFO) << "hr=" << putHR(hr) << " index=" << index;
  return hr;
}

}  // namespace credential_provider
