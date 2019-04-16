// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"

#include <iomanip>
#include <map>
#include <utility>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/auth_utils.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_other_user.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
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
    {FID_FORGOT_PASSWORD_LINK, CPFT_COMMAND_LINK, W2CW(L"Forgot Password"),
     GUID_NULL},
    {FID_PROVIDER_LOGO, CPFT_TILE_IMAGE, W2CW(L"Provider logo"),
     CPFG_CREDENTIAL_PROVIDER_LOGO},
    {FID_PROVIDER_LABEL, CPFT_LARGE_TEXT, W2CW(L"Provider label"),
     CPFG_CREDENTIAL_PROVIDER_LABEL},
};

static_assert(base::size(g_field_desc) == FIELD_COUNT,
              "g_field_desc does not match FIELDID enum");

namespace {

// Initializes an object that implements IReauthCredential.
HRESULT InitializeReauthCredential(CGaiaCredentialProvider* provider,
                                   const base::string16& sid,
                                   const base::string16& domain,
                                   const base::string16& username,
                                   const CComPtr<IGaiaCredential>& gaia_cred) {
  CComPtr<IReauthCredential> reauth;
  HRESULT hr = gaia_cred.QueryInterface(&reauth);
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not get reauth credential interface hr=" << putHR(hr);
    return hr;
  }

  hr = gaia_cred->Initialize(provider);
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not initialize credential hr=" << putHR(hr);
    return hr;
  }

  // Get the user's email address.  If not found, proceed anyway.  The net
  // effect is that the user will need to enter their email address manually
  // instead of it being pre-filled.
  wchar_t email[64];
  ULONG email_length = base::size(email);
  hr = GetUserProperty(sid.c_str(), kUserEmail, email, &email_length);
  if (FAILED(hr))
    email[0] = 0;

  hr = reauth->SetOSUserInfo(CComBSTR(W2COLE(sid.c_str())),
                             CComBSTR(W2COLE(domain.c_str())),
                             CComBSTR(W2COLE(username.c_str())));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "cred->SetOSUserInfo hr=" << putHR(hr);
    return hr;
  }

  if (email[0]) {
    hr = reauth->SetEmailForReauth(CComBSTR(email));
    if (FAILED(hr))
      LOGFN(ERROR) << "reauth->SetEmailForReauth hr=" << putHR(hr);
  }

  return S_OK;
}

}  // namespace

CGaiaCredentialProvider::CGaiaCredentialProvider() {}

CGaiaCredentialProvider::~CGaiaCredentialProvider() {}

HRESULT CGaiaCredentialProvider::FinalConstruct() {
  LOGFN(INFO);
  CleanupOlderVersions();
  return S_OK;
}

void CGaiaCredentialProvider::FinalRelease() {
  LOGFN(INFO);
  ClearTransient();
  // Unlock all the users that had their access locked due to invalid token
  // handles.
  AssociatedUserValidator::Get()->AllowSigninForUsersWithInvalidTokenHandles();
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

void CGaiaCredentialProvider::CleanupOlderVersions() {
  base::FilePath versions_directory = GetInstallDirectory();
  if (!versions_directory.empty())
    DeleteVersionsExcept(versions_directory, TEXT(CHROME_VERSION_STRING));
}

HRESULT CGaiaCredentialProvider::CreateAnonymousCredentialIfNeeded(
    bool showing_other_user) {
  CComPtr<IGaiaCredential> cred;
  HRESULT hr = E_FAIL;
  if (showing_other_user) {
    hr = CComCreator<CComObject<COtherUserGaiaCredential>>::CreateInstance(
        nullptr, IID_IGaiaCredential, reinterpret_cast<void**>(&cred));
  } else if (CanNewUsersBeCreated(cpus_)) {
    hr = CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
        nullptr, IID_IGaiaCredential, reinterpret_cast<void**>(&cred));
  } else {
    return S_OK;
  }

  if (SUCCEEDED(hr)) {
    hr = cred->Initialize(this);
    if (SUCCEEDED(hr)) {
      AddCredentialAndCheckAutoLogon(cred, base::string16());
    } else {
      LOG(ERROR) << "Could not create credential hr=" << putHR(hr);
    }
  }

  return hr;
}

HRESULT CGaiaCredentialProvider::CreateReauthCredentials(
    ICredentialProviderUserArray* users) {
  std::map<base::string16, std::pair<base::string16, base::string16>>
      sid_to_username;

  OSUserManager* manager = OSUserManager::Get();

  // Get the SIDs of all users being shown in the logon UI.
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

    wchar_t* sid_buffer = nullptr;

    hr = user->GetSid(&sid_buffer);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "user->GetSid hr=" << putHR(hr);
      continue;
    }

    base::string16 sid = sid_buffer;
    ::CoTaskMemFree(sid_buffer);

    wchar_t username[kWindowsUsernameBufferLength];
    wchar_t domain[kWindowsDomainBufferLength];

    hr = manager->FindUserBySID(sid.c_str(), username, base::size(username),
                                domain, base::size(domain));
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Can't get sid or username hr=" << putHR(hr);
      continue;
    }

    // If the token handle is valid, no need to create a reauth credential.
    // The user can just sign in using their password.
    if (AssociatedUserValidator::Get()->IsTokenHandleValidForUser(sid))
      continue;

    CComPtr<IGaiaCredential> cred;
    HRESULT hr = CComCreator<CComObject<CReauthCredential>>::CreateInstance(
        nullptr, IID_IGaiaCredential, reinterpret_cast<void**>(&cred));
    if (FAILED(hr)) {
      LOG(ERROR) << "Could not create credential hr=" << putHR(hr);
      return hr;
    }

    hr = InitializeReauthCredential(this, sid, domain, username, cred);
    if (FAILED(hr)) {
      LOG(ERROR) << "InitializeReauthCredential hr=" << putHR(hr);
      return hr;
    }

    AddCredentialAndCheckAutoLogon(cred, sid);
  }

  return S_OK;
}

void CGaiaCredentialProvider::AddCredentialAndCheckAutoLogon(
    const CComPtr<IGaiaCredential>& cred,
    const base::string16& sid) {
  USES_CONVERSION;
  users_.emplace_back(cred);

  if (sid.empty())
    return;

  if (set_serialization_sid_.empty())
    return;

  // If serialization sid is set, then try to see if this credential is a reauth
  // credential that needs to be auto signed in.
  CComPtr<IReauthCredential> associated_user;
  if (FAILED(cred.QueryInterface(&associated_user)))
    return;

  if (set_serialization_sid_ != sid)
    return;

  index_ = users_.size() - 1;
  new_user_sid_ = W2COLE(sid.c_str());
}

// Static.
bool CGaiaCredentialProvider::IsUsageScenarioSupported(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
  return cpus == CPUS_LOGON || cpus == CPUS_UNLOCK_WORKSTATION;
}

// Static.
bool CGaiaCredentialProvider::CanNewUsersBeCreated(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
  // When in an unlock usage, only the user that locked the computer will be
  // allowed to sign in, so no new users can be added.
  if (cpus == CPUS_UNLOCK_WORKSTATION)
    return false;

  // If MDM enrollment is required and multiple users is not supported, only
  // allow a new associated user to be created if there does not yet exist an
  // OS user created from a Google account.
  if (MdmEnrollmentEnabled()) {
    DWORD multi_user_supported = 0;
    HRESULT hr = GetGlobalFlag(kRegMdmSupportsMultiUser, &multi_user_supported);
    if (FAILED(hr) || multi_user_supported == 0) {
      if (AssociatedUserValidator::Get()->GetAssociatedUsersCount() > 0)
        return false;
    }
  }

  return true;
}

// IGaiaCredentialProvider ////////////////////////////////////////////////////

HRESULT CGaiaCredentialProvider::GetUsageScenario(DWORD* cpus) {
  DCHECK(cpus);
  *cpus = static_cast<DWORD>(cpus_);
  return S_OK;
}

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
  // make the newly created user the default login credential with auto logon
  // enabled.  See GetCredentialCount() for more details.
  HRESULT hr = S_OK;
  if (events_ && fire_credentials_changed)
    hr = events_->CredentialsChanged(advise_context_);

  LOGFN(INFO) << "hr=" << putHR(hr) << " sid=" << new_user_sid_.m_str
              << " index=" << index_;
  return hr;
}

// ICredentialProviderSetUserArray ////////////////////////////////////////////

HRESULT CGaiaCredentialProvider::SetUserArray(
    ICredentialProviderUserArray* users) {
  LOGFN(INFO);

  if (!IsUsageScenarioSupported(cpus_))
    return S_OK;

  if (users_.size() > 0) {
    LOG(ERROR) << "Users should be empty";
    return E_UNEXPECTED;
  }

  CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS options;
  HRESULT hr = users->GetAccountOptions(&options);
  bool showing_other_user =
      SUCCEEDED(hr) && options != CPAO_EMPTY_CONNECTED && options != CPAO_NONE;
  hr = CreateAnonymousCredentialIfNeeded(showing_other_user);
  if (FAILED(hr))
    LOG(ERROR) << "Could not create anonymous credential hr=" << putHR(hr);

  hr = CreateReauthCredentials(users);
  if (FAILED(hr))
    LOG(ERROR) << "CreateReauthCredentials hr=" << putHR(hr);

  return hr;
}

// ICredentialProvider ////////////////////////////////////////////////////////

HRESULT CGaiaCredentialProvider::SetUsageScenario(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD flags) {
  ClearTransient();

  cpus_ = cpus;
  cpus_flags_ = flags;

  LOGFN(INFO) << " cpu=" << cpus << " flags=" << std::setbase(16) << flags;
  return IsUsageScenarioSupported(cpus_) ? S_OK : E_NOTIMPL;
}

HRESULT CGaiaCredentialProvider::SetSerialization(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs) {
  DCHECK(pcpcs);

  if (pcpcs->clsidCredentialProvider != CLSID_GaiaCredentialProvider)
    return E_NOTIMPL;

  ULONG auth_package_id;
  HRESULT hr = GetAuthenticationPackageId(&auth_package_id);
  if (FAILED(hr))
    return E_NOTIMPL;

  if (pcpcs->ulAuthenticationPackage != auth_package_id)
    return E_NOTIMPL;

  if (pcpcs->cbSerialization == 0)
    return E_NOTIMPL;

  // If serialziation data is set, try to extract the sid for the user
  // referenced in the serialization data.
  hr = DetermineUserSidFromAuthenticationBuffer(pcpcs, &set_serialization_sid_);
  if (FAILED(hr))
    LOGFN(ERROR) << "DetermineUserSidFromAuthenticationBuffer hr=" << putHR(hr);

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

  // Delete the startup sentinel file if any still exists. It can still exist in
  // 2 cases:

  // 1. The UnAdvise should only occur after the user has logged in, so if they
  // never selected any gaia credential and just used normal credentials this
  // function will be called in that situation and it is guaranteed that the
  // user has at least been able provide some input to winlogon. 2. When no
  // usage scenario is supported, none of the credentials will be selected and
  // thus the gcpw startup sentinel file will not be deleted. So in the case
  // where the user is asked for CPUS_CRED_UI enough times, the sentinel file
  // size will keep growing without being deleted and eventually GCPW will be
  // disabled completely. In the unsupported usage scenario, FinalRelease will
  // be called shortly after SetUsageScenario if the function returns E_NOTIMPL
  // so try to catch potential crashes of the destruction of the provider when
  // it is not used because crashes in this case will prevent the cred ui from
  // coming up and not allow the user to access their desired resource.
  DeleteStartupSentinel();

  return S_OK;
}

HRESULT CGaiaCredentialProvider::GetFieldDescriptorCount(DWORD* count) {
  *count = FIELD_COUNT;
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
