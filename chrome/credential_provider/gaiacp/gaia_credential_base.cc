// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of CGaiaCredentialBase class, used as the base for all
// credentials that need to show the gaia sign in page.

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"

#include <ntstatus.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/current_module.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/auth_utils.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/grit/gaia_static_resources.h"
#include "chrome/credential_provider/gaiacp/internet_availability_checker.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/os_process_manager.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace credential_provider {

namespace {

constexpr wchar_t kEmailDomainsKey[] = L"ed";

base::string16 GetEmailDomains() {
  std::vector<wchar_t> email_domains(16);
  ULONG length = email_domains.size();
  HRESULT hr = GetGlobalFlag(kEmailDomainsKey, &email_domains[0], &length);
  if (FAILED(hr)) {
    if (hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
      email_domains.resize(length + 1);
      length = email_domains.size();
      hr = GetGlobalFlag(kEmailDomainsKey, &email_domains[0], &length);
      if (FAILED(hr))
        email_domains[0] = 0;
    }
  }
  return base::string16(&email_domains[0]);
}

// Tries to find a user associated to the gaia_id stored in |result| under the
// key |kKeyId|. If one exists, then this function will fill out |gaia_id|,
// |username|, |domain| and |sid| with the user's information. If not
// this function will try to generate a new username derived from the email
// and fill out only |gaia_id| and |username|. |domain| will always be empty
// since only local users can be created and |sid| will also be empty until
// the user is created later on.
void MakeUsernameForAccount(const base::DictionaryValue* result,
                            base::string16* gaia_id,
                            wchar_t* username,
                            DWORD username_length,
                            wchar_t* domain,
                            DWORD domain_length,
                            wchar_t* sid,
                            DWORD sid_length) {
  DCHECK(gaia_id);
  DCHECK(username);
  DCHECK(domain);
  DCHECK(sid);

  *gaia_id = GetDictString(result, kKeyId);
  // First try to detect if this gaia account has been used to create an OS
  // user already.  If so, return the OS username of that user.
  HRESULT hr = GetSidFromId(*gaia_id, sid, sid_length);
  if (SUCCEEDED(hr)) {
    hr = OSUserManager::Get()->FindUserBySID(sid, username, username_length,
                                             domain, domain_length);
    if (SUCCEEDED(hr))
      return;
  }
  LOGFN(INFO) << "No existing user found associated to gaia id:" << *gaia_id;
  wcscpy_s(domain, domain_length, OSUserManager::GetLocalDomain().c_str());
  username[0] = 0;
  sid[0] = 0;

  // Create a username based on the email address.  Usernames are more
  // restrictive than emails, so some transformations are needed.  This tries
  // to preserve the email as much as possible in the username while respecting
  // Windows username rules.  See remarks in
  // https://docs.microsoft.com/en-us/windows/desktop/api/lmaccess/ns-lmaccess-_user_info_0
  base::string16 os_username = GetDictString(result, kKeyEmail);
  std::transform(os_username.begin(), os_username.end(), os_username.begin(),
                 ::tolower);

  // If the email ends with @gmail.com or @googlemail.com, strip it.
  base::string16::size_type at = os_username.find(L"@gmail.com");
  if (at == base::string16::npos)
    at = os_username.find(L"@googlemail.com");
  if (at != base::string16::npos) {
    os_username.resize(at);
  } else {
    // Strip off well known TLDs.
    std::string username_utf8 =
        gaia::SanitizeEmail(base::UTF16ToUTF8(os_username));

    size_t tld_length =
        net::registry_controlled_domains::GetCanonicalHostRegistryLength(
            gaia::ExtractDomainName(username_utf8),
            net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    // If an TLD is found strip it off, plus 1 to remove the separating dot too.
    if (tld_length > 0) {
      username_utf8.resize(username_utf8.length() - tld_length - 1);
      os_username = base::UTF8ToUTF16(username_utf8);
    }
  }

  // If the username is longer than 20 characters, truncate.
  if (os_username.size() > kWindowsUsernameBufferLength - 1)
    os_username.resize(kWindowsUsernameBufferLength - 1);

  // Replace invalid characters.  While @ is not strictly invalid according to
  // MSDN docs, it causes trouble.
  for (auto& c : os_username) {
    if (wcschr(L"@\\[]:|<>+=;?*", c) != nullptr || c < 32)
      c = L'_';
  }

  wcscpy_s(username, username_length, os_username.c_str());
}

// Waits for the login UI to completes and returns the result of the operation.
// This function returns S_OK on success, E_UNEXPECTED on failure, and E_ABORT
// if the user aborted or timed out (or was killed during cleanup).
HRESULT WaitForLoginUIAndGetResult(
    CGaiaCredentialBase::UIProcessInfo* uiprocinfo,
    std::string* json_result,
    DWORD* exit_code,
    BSTR* status_text) {
  LOGFN(INFO);
  DCHECK(uiprocinfo);
  DCHECK(json_result);
  DCHECK(exit_code);
  DCHECK(status_text);

  // Buffer used to accumulate output from UI.
  const int kBufferSize = 4096;
  std::vector<char> output_buffer(kBufferSize, '\0');
  base::ScopedClosureRunner zero_buffer_on_exit(
      base::BindOnce(base::IgnoreResult(&RtlSecureZeroMemory),
                     &output_buffer[0], kBufferSize));

  HRESULT hr = WaitForProcess(uiprocinfo->procinfo.process_handle(),
                              uiprocinfo->parent_handles, exit_code,
                              &output_buffer[0], kBufferSize);
  // output_buffer contains sensitive information like the password. Don't log
  // it.
  LOGFN(INFO) << "exit_code=" << exit_code;

  if (*exit_code == kUiecAbort) {
    LOGFN(ERROR) << "Aborted hr=" << putHR(hr);
    return E_ABORT;
  } else if (*exit_code != kUiecSuccess) {
    LOGFN(ERROR) << "Error hr=" << putHR(hr);
    *status_text =
        CGaiaCredentialBase::AllocErrorString(IDS_INVALID_UI_RESPONSE_BASE);
    return E_FAIL;
  }

  *json_result = std::string(&output_buffer[0]);
  return S_OK;
}

// This function validates the response from GLS and makes sure it contained
// all the fields required to proceed with logon.  This does not necessarily
// guarantee that the logon will succeed, only that GLS response seems correct.
HRESULT ValidateResult(const base::DictionaryValue* result, BSTR* status_text) {
  DCHECK(result);
  DCHECK(status_text);

  // Check the exit_code to see if any errors were detected by the GLS.
  const base::Value* exit_code_value =
      result->FindKeyOfType(kKeyExitCode, base::Value::Type::INTEGER);
  int exit_code = exit_code_value->GetInt();
  if (exit_code != kUiecSuccess) {
    switch (exit_code) {
      case kUiecAbort:
        // This case represents a user abort and no error message is shown.
        return E_ABORT;
      case kUiecTimeout:
      case kUiecKilled:
        NOTREACHED() << "Internal codes, not returned by GLS";
        break;
      case kUiecEMailMissmatch:
        *status_text =
            CGaiaCredentialBase::AllocErrorString(IDS_EMAIL_MISMATCH_BASE);
        break;
      case kUiecInvalidEmailDomain:
        *status_text = CGaiaCredentialBase::AllocErrorString(
            IDS_INVALID_EMAIL_DOMAIN_BASE);
        break;
      case kUiecMissingSigninData:
        *status_text =
            CGaiaCredentialBase::AllocErrorString(IDS_INVALID_UI_RESPONSE_BASE);
        break;
    }
    return E_FAIL;
  }

  // Check that the webui returned all expected values.

  bool has_error = false;
  std::string email = GetDictStringUTF8(result, kKeyEmail);
  if (email.empty()) {
    LOGFN(ERROR) << "Email is empty";
    has_error = true;
  }

  std::string fullname = GetDictStringUTF8(result, kKeyFullname);
  if (fullname.empty()) {
    LOGFN(ERROR) << "Full name is empty";
    has_error = true;
  }

  std::string id = GetDictStringUTF8(result, kKeyId);
  if (id.empty()) {
    LOGFN(ERROR) << "Id is empty";
    has_error = true;
  }

  std::string mdm_id_token = GetDictStringUTF8(result, kKeyMdmIdToken);
  if (mdm_id_token.empty()) {
    LOGFN(ERROR) << "mdm id token is empty";
    has_error = true;
  }

  std::string password = GetDictStringUTF8(result, kKeyPassword);
  if (password.empty()) {
    LOGFN(ERROR) << "Password is empty";
    has_error = true;
  }

  std::string refresh_token = GetDictStringUTF8(result, kKeyRefreshToken);
  if (refresh_token.empty()) {
    LOGFN(ERROR) << "refresh token is empty";
    has_error = true;
  }

  std::string token_handle = GetDictStringUTF8(result, kKeyTokenHandle);
  if (token_handle.empty()) {
    LOGFN(ERROR) << "Token handle is empty";
    has_error = true;
  }

  if (has_error) {
    *status_text =
        CGaiaCredentialBase::AllocErrorString(IDS_INVALID_UI_RESPONSE_BASE);
    return E_UNEXPECTED;
  }

  return S_OK;
}

// Creates a new windows OS user with the given |base_username|, |fullname| and
// |password| on the local machine.  Returns the SID of the new user.
// If a user with |base_username| already exists, the function will try to
// generate a new indexed username up to |max_attempts| before failing.
// The actual username used for the new user will be filled in |final_username|
// if successful.
HRESULT CreateNewUser(OSUserManager* manager,
                      const wchar_t* base_username,
                      const wchar_t* password,
                      const wchar_t* fullname,
                      const wchar_t* comment,
                      bool add_to_users_group,
                      int max_attempts,
                      BSTR* final_username,
                      BSTR* sid) {
  DCHECK(manager);
  DCHECK(base_username);
  DCHECK(password);
  DCHECK(fullname);
  DCHECK(comment);
  DCHECK(final_username);
  DCHECK(sid);
  wchar_t new_username[kWindowsUsernameBufferLength];
  errno_t err = wcscpy_s(new_username, base::size(new_username), base_username);
  if (err != 0) {
    LOGFN(ERROR) << "wcscpy_s errno=" << err;
    return E_FAIL;
  }

  // Keep trying to create the user account until an unused username can be
  // found or |max_attempts| has been reached.
  for (int i = 0; i < max_attempts; ++i) {
    CComBSTR new_sid;
    DWORD error;
    HRESULT hr = manager->AddUser(new_username, password, fullname, comment,
                                  add_to_users_group, &new_sid, &error);
    if (hr == HRESULT_FROM_WIN32(NERR_UserExists)) {
      base::string16 next_username = base_username;
      base::string16 next_username_suffix =
          base::NumberToString16(i + kInitialDuplicateUsernameIndex);
      // Create a new user name that fits in |kWindowsUsernameBufferLength|
      if (next_username.size() + next_username_suffix.size() >
          (kWindowsUsernameBufferLength - 1)) {
        next_username =
            next_username.substr(0, (kWindowsUsernameBufferLength - 1) -
                                        next_username_suffix.size()) +
            next_username_suffix;
      } else {
        next_username += next_username_suffix;
      }
      LOGFN(INFO) << "Username '" << new_username
                  << "' already exists. Trying '" << next_username << "'";

      errno_t err = wcscpy_s(new_username, base::size(new_username),
                             next_username.c_str());
      if (err != 0) {
        LOGFN(ERROR) << "wcscpy_s errno=" << err;
        return E_FAIL;
      }

      continue;
    } else if (FAILED(hr)) {
      LOGFN(ERROR) << "manager->AddUser hr=" << putHR(hr);
      return hr;
    }

    *sid = ::SysAllocString(new_sid);
    *final_username = ::SysAllocString(new_username);
    return S_OK;
  }

  return HRESULT_FROM_WIN32(NERR_UserExists);
}

}  // namespace

CGaiaCredentialBase::UIProcessInfo::UIProcessInfo() {}

CGaiaCredentialBase::UIProcessInfo::~UIProcessInfo() {}

// static
HRESULT CGaiaCredentialBase::OnDllRegisterServer() {
  OSUserManager* manager = OSUserManager::Get();

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  PSID sid = nullptr;

  // Try to get existing username and password and then log on the user, if any
  // step fails, assume that a new user needs to be created.
  wchar_t gaia_username[kWindowsUsernameBufferLength];
  HRESULT hr = policy->RetrievePrivateData(kLsaKeyGaiaUsername, gaia_username,
                                           base::size(gaia_username));

  if (SUCCEEDED(hr)) {
    LOGFN(INFO) << "Expecting gaia user '" << gaia_username << "' to exist.";
    wchar_t password[32];
    HRESULT hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, password,
                                             base::size(password));
    if (SUCCEEDED(hr)) {
      base::string16 local_domain = OSUserManager::GetLocalDomain();
      base::win::ScopedHandle token;
      hr = OSUserManager::Get()->CreateLogonToken(
          local_domain.c_str(), gaia_username, password,
          /*interactive=*/false, &token);
      if (SUCCEEDED(hr)) {
        hr = OSUserManager::Get()->GetUserSID(local_domain.c_str(),
                                              gaia_username, &sid);
        if (FAILED(hr)) {
          LOGFN(ERROR) << "GetUserSID(sid from existing user '" << gaia_username
                       << "') hr=" << putHR(hr);
          sid = nullptr;
        }
      }
    }
  }

  if (sid == nullptr) {
    // No valid existing user found, reset to default name and start generating
    // from there.
    errno_t err = wcscpy_s(gaia_username, base::size(gaia_username),
                           kDefaultGaiaAccountName);
    if (err != 0) {
      LOGFN(ERROR) << "wcscpy_s errno=" << err;
      return E_FAIL;
    }

    // Generate a random password for the new gaia account.
    wchar_t password[32];
    HRESULT hr =
        manager->GenerateRandomPassword(password, base::size(password));
    if (FAILED(hr)) {
      LOGFN(ERROR) << "GenerateRandomPassword hr=" << putHR(hr);
      return hr;
    }

    CComBSTR sid_string;
    CComBSTR gaia_username;
    // Keep trying to create the special Gaia account used to run the UI until
    // an unused username can be found or kMaxUsernameAttempts has been reached.
    hr =
        CreateNewUser(manager, kDefaultGaiaAccountName, password,
                      GetStringResource(IDS_GAIA_ACCOUNT_FULLNAME_BASE).c_str(),
                      GetStringResource(IDS_GAIA_ACCOUNT_COMMENT_BASE).c_str(),
                      /*add_to_users_group=*/false, kMaxUsernameAttempts,
                      &gaia_username, &sid_string);

    if (FAILED(hr)) {
      LOGFN(ERROR) << "CreateNewUser hr=" << putHR(hr);
      return hr;
    }

    if (!::ConvertStringSidToSid(sid_string, &sid)) {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "ConvertStringSidToSid hr=" << putHR(hr);
      return hr;
    }

    // Save the password in a machine secret area.
    hr = policy->StorePrivateData(kLsaKeyGaiaPassword, password);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Failed to store gaia user password in LSA hr="
                   << putHR(hr);
      return hr;
    }

    // Save the gaia username in a machine secret area.
    hr = policy->StorePrivateData(kLsaKeyGaiaUsername, gaia_username);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Failed to store gaia user name in LSA hr=" << putHR(hr);
      return hr;
    }
  }

  if (!sid) {
    LOGFN(ERROR) << "No valid username could be found for the gaia user.";
    return HRESULT_FROM_WIN32(NERR_UserExists);
  }

  // Add "logon as batch" right.
  hr = policy->AddAccountRights(sid, SE_BATCH_LOGON_NAME);
  ::LocalFree(sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "policy.AddAccountRights hr=" << putHR(hr);
    return hr;
  }
  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::OnDllUnregisterServer() {
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (policy) {
    wchar_t password[32];

    HRESULT hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, password,
                                             base::size(password));
    if (FAILED(hr))
      LOGFN(ERROR) << "policy.RetrievePrivateData hr=" << putHR(hr);

    hr = policy->RemovePrivateData(kLsaKeyGaiaPassword);
    if (FAILED(hr))
      LOGFN(ERROR) << "policy.RemovePrivateData hr=" << putHR(hr);

    OSUserManager* manager = OSUserManager::Get();
    PSID sid;

    wchar_t gaia_username[kWindowsUsernameBufferLength];
    hr = policy->RetrievePrivateData(kLsaKeyGaiaUsername, gaia_username,
                                     base::size(gaia_username));

    if (SUCCEEDED(hr)) {
      hr = policy->RemovePrivateData(kLsaKeyGaiaUsername);
      base::string16 local_domain = OSUserManager::GetLocalDomain();

      hr = manager->GetUserSID(local_domain.c_str(), gaia_username, &sid);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "manager.GetUserSID hr=" << putHR(hr);
        sid = nullptr;
      }

      hr = manager->RemoveUser(gaia_username, password);
      if (FAILED(hr))
        LOGFN(ERROR) << "manager->RemoveUser hr=" << putHR(hr);

      // Remove the account from LSA after the OS account is deleted.
      if (sid != nullptr) {
        hr = policy->RemoveAccount(sid);
        ::LocalFree(sid);
        if (FAILED(hr))
          LOGFN(ERROR) << "policy.RemoveAccount hr=" << putHR(hr);
      }
    } else {
      LOGFN(ERROR) << "Get gaia username failed hr=" << putHR(hr);
    }

  } else {
    LOGFN(ERROR) << "ScopedLsaPolicy::Create failed";
  }

  return S_OK;
}

CGaiaCredentialBase::CGaiaCredentialBase() {}

CGaiaCredentialBase::~CGaiaCredentialBase() {}

bool CGaiaCredentialBase::AreCredentialsValid() const {
  return CanAttemptWindowsLogon() &&
         IsWindowsPasswordValidForStoredUser(password_) == S_OK;
}

bool CGaiaCredentialBase::CanAttemptWindowsLogon() const {
  return username_.Length() > 0 && password_.Length() > 0;
}

HRESULT CGaiaCredentialBase::IsWindowsPasswordValidForStoredUser(
    BSTR password) const {
  if (username_.Length() == 0 || user_sid_.Length() == 0)
    return S_FALSE;

  if (::SysStringLen(password) == 0)
    return S_FALSE;
  OSUserManager* manager = OSUserManager::Get();
  return manager->IsWindowsPasswordValid(domain_, username_, password);
}

HRESULT CGaiaCredentialBase::GetStringValueImpl(DWORD field_id,
                                                wchar_t** value) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_DESCRIPTION: {
      base::string16 description(
          GetStringResource(IDS_AUTH_FID_DESCRIPTION_BASE));
      hr = ::SHStrDupW(description.c_str(), value);
      break;
    }
    case FID_PROVIDER_LABEL: {
      base::string16 label(GetStringResource(IDS_AUTH_FID_PROVIDER_LABEL_BASE));
      hr = ::SHStrDupW(label.c_str(), value);
      break;
    }
    case FID_CURRENT_PASSWORD_FIELD: {
      hr = ::SHStrDupW(current_windows_password_.Length() > 0
                           ? current_windows_password_
                           : L"",
                       value);
      break;
    }
    default:
      break;
  }

  return hr;
}

HRESULT CGaiaCredentialBase::GetBitmapValueImpl(DWORD field_id,
                                                HBITMAP* phbmp) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_PROVIDER_LOGO:
      *phbmp = ::LoadBitmap(CURRENT_MODULE(),
                            MAKEINTRESOURCE(IDB_GOOGLE_LOGO_SMALL));
      if (*phbmp)
        hr = S_OK;
      break;
    default:
      break;
  }

  return hr;
}

void CGaiaCredentialBase::ResetInternalState() {
  LOGFN(INFO);
  username_.Empty();
  domain_.Empty();
  password_.Empty();
  current_windows_password_.Empty();
  authentication_results_.reset();
  needs_windows_password_ = false;
  result_status_ = STATUS_SUCCESS;

  TerminateLogonProcess();

  if (events_) {
    wchar_t* default_status_text = nullptr;
    GetStringValue(FID_DESCRIPTION, &default_status_text);
    events_->SetFieldString(this, FID_DESCRIPTION, default_status_text);
    events_->SetFieldState(this, FID_CURRENT_PASSWORD_FIELD, CPFS_HIDDEN);
    events_->SetFieldString(this, FID_CURRENT_PASSWORD_FIELD,
                            current_windows_password_);
    events_->SetFieldSubmitButton(this, FID_SUBMIT, FID_DESCRIPTION);
    UpdateSubmitButtonInteractiveState();
  }
}

HRESULT CGaiaCredentialBase::GetBaseGlsCommandline(
    base::CommandLine* command_line) {
  DCHECK(command_line);

  base::FilePath gls_path =
      chrome_launcher_support::GetChromePathForInstallationLevel(
          chrome_launcher_support::SYSTEM_LEVEL_INSTALLATION, false);

  constexpr wchar_t kGlsPath[] = L"gls_path";

  wchar_t custom_gls_path_value[MAX_PATH];
  ULONG path_len = base::size(custom_gls_path_value) * sizeof(wchar_t);
  HRESULT hr = GetGlobalFlag(kGlsPath, custom_gls_path_value, &path_len);
  if (SUCCEEDED(hr)) {
    base::FilePath custom_gls_path(custom_gls_path_value);
    if (base::PathExists(custom_gls_path)) {
      gls_path = custom_gls_path;
    } else {
      LOGFN(ERROR) << "Specified gls path ('" << custom_gls_path.value()
                   << "') does not exist, using default gls path.";
    }
  }

  if (gls_path.empty()) {
    LOGFN(ERROR) << "No path to chrome.exe could be found.";
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  command_line->SetProgram(gls_path);

  LOGFN(INFO) << "App exe: " << command_line->GetProgram().value();

  command_line->AppendSwitch(kGcpwSigninSwitch);

  // Temporary endpoint while waiting for the real GCPW endpoint to be
  // finished.
  GURL endpoint_url = GaiaUrls::GetInstance()->embedded_signin_url();
  std::string endpoint_path = endpoint_url.path().substr(1);

  // Registry specified endpoint.
  wchar_t endpoint_url_setting[256];
  ULONG endpoint_url_length = base::size(endpoint_url_setting);
  if (SUCCEEDED(GetGlobalFlag(L"ep_url", endpoint_url_setting,
                              &endpoint_url_length)) &&
      endpoint_url_setting[0]) {
    endpoint_url = GURL(endpoint_url_setting);
    if (endpoint_url.is_valid()) {
      command_line->AppendSwitchASCII(switches::kGaiaUrl,
                                      endpoint_url.GetWithEmptyPath().spec());
      endpoint_path = endpoint_url.path().substr(1);
    }
  }

  command_line->AppendSwitchASCII(kGcpwEndpointPathSwitch, endpoint_path);

  // Get the language selected by the LanguageSelector and pass it onto Chrome.
  // The language will depend on if it is currently a SYSTEM logon (initial
  // logon) or a lock screen logon (from a user). If the user who locked the
  // screen has a specific language, that will be the one used for the UI
  // language.
  command_line->AppendSwitchNative("lang", GetSelectedLanguage());

  return S_OK;
}

HRESULT CGaiaCredentialBase::GetUserGlsCommandline(
    base::CommandLine* command_line) {
  return S_OK;
}

HRESULT CGaiaCredentialBase::GetGlsCommandline(
    base::CommandLine* command_line) {
  DCHECK(command_line);
  HRESULT hr = GetBaseGlsCommandline(command_line);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetBaseGlsCommandline hr=" << putHR(hr);
    return hr;
  }

  // If email domains are specified, pass them to the GLS.
  base::string16 email_domains = GetEmailDomains();
  if (email_domains[0])
    command_line->AppendSwitchNative(kEmailDomainsSwitch, email_domains);

  hr = GetUserGlsCommandline(command_line);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetBaseGlsCommandline hr=" << putHR(hr);
    return hr;
  }

  LOGFN(INFO) << "Command line: " << command_line->GetCommandLineString();
  return S_OK;
}

void CGaiaCredentialBase::DisplayErrorInUI(LONG status,
                                           LONG substatus,
                                           BSTR status_text) {
  if (status != STATUS_SUCCESS) {
    if (events_)
      events_->SetFieldString(this, FID_DESCRIPTION, status_text);
  }
}

HRESULT CGaiaCredentialBase::HandleAutologon(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* cpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs) {
  USES_CONVERSION;
  LOGFN(INFO) << "user-sid=" << get_sid().m_str;
  DCHECK(cpgsr);
  DCHECK(cpcs);

  if (!CanAttemptWindowsLogon())
    return S_FALSE;

  // If a password update is needed, check if the user entered their old
  // Windows password and it is valid. If it is, try to change the password
  // using the old password. If it isn't, return S_FALSE to state that the
  // login is not complete.
  if (needs_windows_password_) {
    HRESULT hr = IsWindowsPasswordValidForStoredUser(current_windows_password_);
    if (hr == S_OK) {
      OSUserManager* manager = OSUserManager::Get();
      hr = manager->ChangeUserPassword(domain_, username_,
                                       current_windows_password_, password_);
      if (FAILED(hr)) {
        if (hr != HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) {
          LOGFN(ERROR) << "ChangeUserPassword hr=" << putHR(hr);
          return hr;
        }
        LOGFN(ERROR) << "Access was denied to ChangeUserPassword.";
        password_ = current_windows_password_;
      }
    } else {
      if (current_windows_password_.Length() && events_) {
        UINT pasword_message_id = IDS_INVALID_PASSWORD_BASE;
        if (hr == HRESULT_FROM_WIN32(ERROR_ACCOUNT_LOCKED_OUT)) {
          pasword_message_id = IDS_ACCOUNT_LOCKED_BASE;
          LOGFN(ERROR) << "Account is locked.";
        }

        events_->SetFieldString(this, FID_DESCRIPTION,
                                GetStringResource(pasword_message_id).c_str());
        events_->SetFieldInteractiveState(this, FID_CURRENT_PASSWORD_FIELD,
                                          CPFIS_FOCUSED);
      }
      return S_FALSE;
    }
  }

  // Restore user's access so that they can sign in.
  HRESULT hr =
      AssociatedUserValidator::Get()->RestoreUserAccess(OLE2W(get_sid()));
  if (FAILED(hr) && hr != HRESULT_FROM_NT(STATUS_OBJECT_NAME_NOT_FOUND)) {
    LOGFN(ERROR) << "RestoreUserAccess hr=" << putHR(hr);
    return hr;
  }

  // The OS user has already been created, so return all the information
  // needed to log them in.
  DWORD cpus = 0;
  provider()->GetUsageScenario(&cpus);
  hr = BuildCredPackAuthenticationBuffer(
      domain_, get_username(), get_password(),
      static_cast<CREDENTIAL_PROVIDER_USAGE_SCENARIO>(cpus), cpcs);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildCredPackAuthenticationBuffer hr=" << putHR(hr);
    return hr;
  }

  cpcs->clsidCredentialProvider = CLSID_GaiaCredentialProvider;
  *cpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;

  return S_OK;
}

// static
void CGaiaCredentialBase::TellOmahaDidRun() {
#if defined(GOOGLE_CHROME_BUILD)
  // Tell omaha that product was used.  Best effort only.
  //
  // This code always runs as LocalSystem, which means that HKCU maps to
  // HKU\.Default.  This is OK because omaha reads the "dr" value from subkeys
  // of HKEY_USERS.
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_CURRENT_USER, kRegUpdaterClientStateAppPath,
                        KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (sts != ERROR_SUCCESS) {
    LOGFN(INFO) << "Unable to open omaha key sts=" << sts;
  } else {
    sts = key.WriteValue(L"dr", L"1");
    if (sts != ERROR_SUCCESS)
      LOGFN(INFO) << "Unable to write omaha dr value sts=" << sts;
  }
#endif  // defined(GOOGLE_CHROME_BUILD)
}

// static
BSTR CGaiaCredentialBase::AllocErrorString(UINT id) {
  CComBSTR str(GetStringResource(id).c_str());
  return str.Detach();
}

// static
HRESULT CGaiaCredentialBase::GetInstallDirectory(base::FilePath* path) {
  DCHECK(path);

  if (!base::PathService::Get(base::FILE_MODULE, path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Get(FILE_MODULE) hr=" << putHR(hr);
    return hr;
  }

  *path = path->DirName();
  return S_OK;
}

// ICredentialProviderCredential //////////////////////////////////////////////

HRESULT CGaiaCredentialBase::Advise(ICredentialProviderCredentialEvents* cpce) {
  LOGFN(INFO);
  events_ = cpce;
  return S_OK;
}

HRESULT CGaiaCredentialBase::UnAdvise(void) {
  LOGFN(INFO);
  events_.Release();

  return S_OK;
}

HRESULT CGaiaCredentialBase::SetSelected(BOOL* auto_login) {
  *auto_login = CanAttemptWindowsLogon();
  LOGFN(INFO) << "auto-login=" << *auto_login;

  // After this point the user is able to interact with the winlogon and thus
  // can avoid potential crash loops so the startup sentinel can be deleted.
  DeleteStartupSentinel();
  return S_OK;
}

HRESULT CGaiaCredentialBase::SetDeselected(void) {
  LOGFN(INFO);

  // Cancel logon so that the next time this credential is clicked everything
  // has to be re-entered by the user. This prevents a Windows password
  // entered into the password field by the user from being persisted too
  // long. The behaviour is similar to that of the normal windows password
  // text box. Whenever a different user is selected and then the original
  // credential is selected again, the password is cleared.
  ResetInternalState();

  return S_OK;
}

HRESULT CGaiaCredentialBase::GetFieldState(
    DWORD field_id,
    CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_DESCRIPTION:
    case FID_SUBMIT:
      *pcpfs = CPFS_DISPLAY_IN_SELECTED_TILE;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    case FID_PROVIDER_LOGO:
      *pcpfs = ::IsWindows8OrGreater() ? CPFS_HIDDEN : CPFS_DISPLAY_IN_BOTH;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    case FID_PROVIDER_LABEL:
      *pcpfs = ::IsWindows8OrGreater() ? CPFS_HIDDEN
                                       : CPFS_DISPLAY_IN_DESELECTED_TILE;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    case FID_CURRENT_PASSWORD_FIELD:
      *pcpfs = CPFS_HIDDEN;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    default:
      break;
  }
  LOGFN(INFO) << "hr=" << putHR(hr) << " field=" << field_id
              << " state=" << *pcpfs << " inter-state=" << *pcpfis;
  return hr;
}

HRESULT CGaiaCredentialBase::GetStringValue(DWORD field_id, wchar_t** value) {
  return GetStringValueImpl(field_id, value);
}

HRESULT CGaiaCredentialBase::GetBitmapValue(DWORD field_id, HBITMAP* phbmp) {
  return GetBitmapValueImpl(field_id, phbmp);
}

HRESULT CGaiaCredentialBase::GetCheckboxValue(DWORD field_id,
                                              BOOL* pbChecked,
                                              wchar_t** ppszLabel) {
  // No checkboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::GetSubmitButtonValue(DWORD field_id,
                                                  DWORD* adjacent_to) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_SUBMIT:
      *adjacent_to = FID_DESCRIPTION;
      hr = S_OK;
      break;
    default:
      break;
  }

  return hr;
}

HRESULT CGaiaCredentialBase::GetComboBoxValueCount(DWORD field_id,
                                                   DWORD* pcItems,
                                                   DWORD* pdwSelectedItem) {
  // No comboboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::GetComboBoxValueAt(DWORD field_id,
                                                DWORD dwItem,
                                                wchar_t** ppszItem) {
  // No comboboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::SetStringValue(DWORD field_id,
                                            const wchar_t* psz) {
  USES_CONVERSION;

  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_CURRENT_PASSWORD_FIELD:
      if (needs_windows_password_) {
        current_windows_password_ = W2COLE(psz);
        UpdateSubmitButtonInteractiveState();
      }
      hr = S_OK;
      break;
  }
  return hr;
}

HRESULT CGaiaCredentialBase::SetCheckboxValue(DWORD field_id, BOOL bChecked) {
  // No checkboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::SetComboBoxSelectedValue(DWORD field_id,
                                                      DWORD dwSelectedItem) {
  // No comboboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::CommandLinkClicked(DWORD dwFieldID) {
  // No links.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::GetSerialization(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* cpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs,
    wchar_t** status_text,
    CREDENTIAL_PROVIDER_STATUS_ICON* status_icon) {
  USES_CONVERSION;
  LOGFN(INFO);
  DCHECK(status_text);
  DCHECK(status_icon);

  *status_text = nullptr;
  *status_icon = CPSI_NONE;
  memset(cpcs, 0, sizeof(*cpcs));

  // This may be a long running function so disable user input while processing.
  if (events_) {
    events_->SetFieldInteractiveState(this, FID_SUBMIT, CPFIS_DISABLED);
    events_->SetFieldInteractiveState(this, FID_CURRENT_PASSWORD_FIELD,
                                      CPFIS_DISABLED);
  }

  HRESULT hr = HandleAutologon(cpgsr, cpcs);

  // Don't clear the state of the credential on error. The error can occur
  // because the user is locked out or entered an incorrect old password when
  // trying to update their password. In these situations it may still be
  // possible to sign in with the information that is currently available if
  // the problem can be fixed externally so keep all the information for now.
  if (FAILED(hr)) {
    LOGFN(ERROR) << "HandleAutologon hr=" << putHR(hr);
    *status_icon = CPSI_ERROR;
    *cpgsr = CPGSR_RETURN_NO_CREDENTIAL_FINISHED;
  } else if (hr == S_FALSE) {
    // If HandleAutologon returns S_FALSE, then there was not enough information
    // to log the user on or they need to update their password and gave an
    // invalid old password.  Display the Gaia sign in page if there is not
    // sufficient Gaia credentials or just return
    // CPGSR_NO_CREDENTIAL_NOT_FINISHED to wait for the user to try a new
    // password.

    // Logon process is still running or windows password needs to be entered,
    // return that serialization is not finished so that a second logon stub
    // isn't started.
    if (logon_ui_process_ != INVALID_HANDLE_VALUE || needs_windows_password_) {
      *cpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;

      // Warn that password needs update.
      if (needs_windows_password_)
        *status_icon = CPSI_WARNING;

      hr = S_OK;
    } else {
      LOGFN(INFO) << "HandleAutologon hr=" << putHR(hr);
      TellOmahaDidRun();

      // If there is no internet connection, just abort right away.
      if (!InternetAvailabilityChecker::Get()->HasInternetConnection()) {
        BSTR error_message = AllocErrorString(IDS_NO_NETWORK_BASE);
        ::SHStrDupW(OLE2CW(error_message), status_text);
        ::SysFreeString(error_message);

        *status_icon = CPSI_NONE;
        *cpgsr = CPGSR_NO_CREDENTIAL_FINISHED;
        LOGFN(INFO) << "No internet connection";
        UpdateSubmitButtonInteractiveState();

        hr = S_OK;
      } else {
        // The account creation is async so we are not done yet.
        *cpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;

        // The expectation is that the UI will eventually return the username,
        // password, and auth to this CGaiaCredentialBase object, so that
        // OnUserAuthenticated() can be called, followed by
        // provider_->OnUserAuthenticated().
        hr = CreateAndRunLogonStub();
      }
    }
  } else {
    *status_icon = CPSI_SUCCESS;
  }

  // Logon is not complete, re-enable UI as needed.
  if (*cpgsr != CPGSR_NO_CREDENTIAL_FINISHED &&
      *cpgsr != CPGSR_RETURN_CREDENTIAL_FINISHED &&
      *cpgsr != CPGSR_RETURN_NO_CREDENTIAL_FINISHED) {
    if (events_) {
      events_->SetFieldInteractiveState(
          this, FID_CURRENT_PASSWORD_FIELD,
          needs_windows_password_ ? CPFIS_FOCUSED : CPFIS_NONE);
    }
    UpdateSubmitButtonInteractiveState();
  }
  // Otherwise, keep the ui disable forever now. ReportResult will eventually
  // be called on success or failure and the reset of the state of the
  // credential will be done there.
  return hr;
}

HRESULT CGaiaCredentialBase::CreateAndRunLogonStub() {
  LOGFN(INFO);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  HRESULT hr = GetGlsCommandline(&command_line);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetGlsCommandline hr=" << putHR(hr);
    return hr;
  }

  // The process should start on the interactive window station (since it
  // needs to show a UI) but on its own desktop so that it cannot interact
  // with winlogon on user windows.
  std::unique_ptr<UIProcessInfo> uiprocinfo(new UIProcessInfo);
  PSID logon_sid;
  hr = CreateGaiaLogonToken(&uiprocinfo->logon_token, &logon_sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateGaiaLogonToken hr=" << putHR(hr);
    return hr;
  }

  OSProcessManager* process_manager = OSProcessManager::Get();
  hr = process_manager->SetupPermissionsForLogonSid(logon_sid);
  LocalFree(logon_sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetupPermissionsForLogonSid hr=" << putHR(hr);
    return hr;
  }

  hr = ForkGaiaLogonStub(process_manager, command_line, uiprocinfo.get());
  if (FAILED(hr)) {
    LOGFN(ERROR) << "ForkGaiaLogonStub hr=" << putHR(hr);
    return hr;
  }

  // Save the handle to the logon UI process so that it can be killed should
  // the credential be Unadvise()d.
  DCHECK_EQ(logon_ui_process_, INVALID_HANDLE_VALUE);
  logon_ui_process_ = uiprocinfo->procinfo.process_handle();

  uiprocinfo->credential = this;

  // Background thread takes ownership of |uiprocinfo|.
  unsigned int wait_thread_id;
  UIProcessInfo* puiprocinfo = uiprocinfo.release();
  uintptr_t wait_thread = _beginthreadex(nullptr, 0, WaitForLoginUI,
                                         puiprocinfo, 0, &wait_thread_id);
  if (wait_thread != 0) {
    LOGFN(INFO) << "Started wait thread id=" << wait_thread_id;

    // CreateAndRunLogonStub() is called from GetSerialization().  Winlogon
    // will block the UI and show a spinner until the latter returns.  In most
    // cases GetSerialization() should return immediately so that users don't
    // get blocked out of the winlogon UX.  For example users could decide to
    // sign in with another user or another credential provider.  However, in
    // case where enrollment to Google MDM is required, the UI should be
    // blocked until the enrollment either succeeds or fails.  To perform this
    // CreateAndRunLogonStub() waits for WaitForLoginUI() to complete.
    wchar_t mdm_url[256];
    ULONG mdm_length = base::size(mdm_url);
    hr = credential_provider::GetGlobalFlag(credential_provider::kRegMdmUrl,
                                            mdm_url, &mdm_length);
    if (SUCCEEDED(hr) && mdm_length > 0)
      ::WaitForSingleObject(reinterpret_cast<HANDLE>(wait_thread), INFINITE);

    ::CloseHandle(reinterpret_cast<HANDLE>(wait_thread));
  } else {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Unable to start wait thread hr=" << putHR(hr);
    ::TerminateProcess(puiprocinfo->procinfo.process_handle(), kUiecKilled);
    delete puiprocinfo;
    return hr;
  }

  // This function returns success, which means that GetSerialization() will
  // return success.  CGaiaCredentialBase is now committed to telling
  // CGaiaCredentialProvider whether the serialization eventually succeeds or
  // fails, so that CGaiaCredentialProvider can in turn inform winlogon about
  // what happened.
  LOGFN(INFO) << "cleaning up";
  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::CreateGaiaLogonToken(
    base::win::ScopedHandle* token,
    PSID* sid) {
  DCHECK(token);
  DCHECK(sid);

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (!policy) {
    LOGFN(ERROR) << "LsaOpenPolicy failed";
    return E_UNEXPECTED;
  }

  wchar_t gaia_username[kWindowsUsernameBufferLength];
  HRESULT hr = policy->RetrievePrivateData(kLsaKeyGaiaUsername, gaia_username,
                                           base::size(gaia_username));

  if (FAILED(hr)) {
    LOGFN(ERROR) << "Retrieve gaia username hr=" << putHR(hr);
    return hr;
  }
  wchar_t password[32];
  hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, password,
                                   base::size(password));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "Retrieve password for gaia user '" << gaia_username
                 << "' hr=" << putHR(hr);
    return hr;
  }

  base::string16 local_domain = OSUserManager::GetLocalDomain();
  hr = OSUserManager::Get()->CreateLogonToken(local_domain.c_str(),
                                              gaia_username, password,
                                              /*interactive=*/false, token);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateLogonToken hr=" << putHR(hr);
    return hr;
  }

  hr = OSProcessManager::Get()->GetTokenLogonSID(*token, sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetTokenLogonSID hr=" << putHR(hr);
    token->Close();
    return hr;
  }

  wchar_t* sid_string;
  if (::ConvertSidToStringSid(*sid, &sid_string)) {
    LOGFN(INFO) << "logon-sid=" << sid_string;
    LocalFree(sid_string);
  } else {
    LOGFN(ERROR) << "logon-sid=<can't get string>";
  }

  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::ForkGaiaLogonStub(
    OSProcessManager* process_manager,
    const base::CommandLine& command_line,
    UIProcessInfo* uiprocinfo) {
  LOGFN(INFO);
  DCHECK(process_manager);
  DCHECK(uiprocinfo);

  ScopedStartupInfo startupinfo(kDesktopFullName);

  // Only create a stdout pipe for the logon stub process. On some machines
  // Chrome will not startup properly when also given a stderror pipe due
  // to access restrictions. For the purposes of the credential provider
  // only the output of stdout matters.
  HRESULT hr =
      InitializeStdHandles(CommDirection::kChildToParentOnly, kStdOutput,
                           &startupinfo, &uiprocinfo->parent_handles);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "InitializeStdHandles hr=" << putHR(hr);
    return hr;
  }

  // The process is created suspended so that we can adjust its environment
  // before it starts.  Also, it must not run before it is added to the job
  // object.
  hr = process_manager->CreateProcessWithToken(
      uiprocinfo->logon_token, command_line, startupinfo.GetInfo(),
      &uiprocinfo->procinfo);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "process_manager->CreateProcessWithToken hr=" << putHR(hr);
    return hr;
  }

  LOGFN(INFO) << "pid=" << uiprocinfo->procinfo.process_id()
              << " tid=" << uiprocinfo->procinfo.thread_id();

  // Don't create a job here with UI restrictions, since win10 does not allow
  // nested jobs unless all jobs don't specify UI restrictions.  Since chrome
  // will set a job with UI restrictions for renderer/gpu/etc processes, setting
  // one here causes chrome to fail.

  // Environment is fully set up for UI, so let it go.
  if (::ResumeThread(uiprocinfo->procinfo.thread_handle()) ==
      static_cast<DWORD>(-1)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ResumeThread hr=" << putHR(hr);
    ::TerminateProcess(uiprocinfo->procinfo.process_handle(), kUiecKilled);
    return hr;
  }

  // Don't close the desktop until after the process has started and acquired
  // a handle to it.  Otherwise, the desktop will be destroyed and the process
  // will fail to start.
  //
  // WaitForInputIdle() return immediately with an error if the process
  // created is a console app.  In production this will not be the case,
  // however in tests this may happen.  However, tests are not concerned with
  // the destruction of the desktop since one is not created.
  DWORD ret = ::WaitForInputIdle(uiprocinfo->procinfo.process_handle(), 10000);
  if (ret != 0)
    LOGFN(INFO) << "WaitForInputIdle, ret=" << ret;

  return S_OK;
}

HRESULT CGaiaCredentialBase::ForkSaveAccountInfoStub(
    const std::unique_ptr<base::DictionaryValue>& dict,
    BSTR* status_text) {
  LOGFN(INFO);
  DCHECK(status_text);

  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;
  HRESULT hr =
      InitializeStdHandles(CommDirection::kParentToChildOnly, kAllStdHandles,
                           &startupinfo, &parent_handles);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "InitializeStdHandles hr=" << putHR(hr);
    *status_text = AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  }

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  hr = GetCommandLineForEntrypoint(CURRENT_MODULE(), L"SaveAccountInfo",
                                   &command_line);
  if (hr == S_FALSE) {
    // This happens in tests.  It means this code is running inside the
    // unittest exe and not the credential provider dll.  Just ignore saving
    // the account info.
    LOGFN(INFO) << "Not running SAIS";
    return S_OK;
  } else if (FAILED(hr)) {
    LOGFN(ERROR) << "GetCommandLineForEntryPoint hr=" << putHR(hr);
    *status_text = AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  }

  // Mark this process as a child process so that it doesn't try to
  // start a crashpad handler process. Only the main entry point
  // into the dll should start the handler process.
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 "gcpw-save-account-info");

  base::win::ScopedProcessInformation procinfo;
  hr = OSProcessManager::Get()->CreateRunningProcess(
      command_line, startupinfo.GetInfo(), &procinfo);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "OSProcessManager::CreateRunningProcess hr=" << putHR(hr);
    *status_text = AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  }

  // Write account info to stdin of child process.  This buffer is read by
  // SaveAccountInfoW() in dllmain.cpp.  If this fails, chrome won't pick up
  // the credentials from the credential provider and will need to sign in
  // manually.  TODO(crbug.com/902911): Figure out how to handle this.
  std::string json;
  if (base::JSONWriter::Write(*dict, &json)) {
    DWORD written;
    if (!::WriteFile(parent_handles.hstdin_write.Get(), json.c_str(),
                     json.length() + 1, &written, /*lpOverlapped=*/nullptr)) {
      HRESULT hrWrite = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "WriteFile hr=" << putHR(hrWrite);
    }
  } else {
    LOGFN(ERROR) << "base::JSONWriter::Write failed";
  }

  return S_OK;
}

// static
unsigned __stdcall CGaiaCredentialBase::WaitForLoginUI(void* param) {
  USES_CONVERSION;
  DCHECK(param);
  std::unique_ptr<UIProcessInfo> uiprocinfo(
      reinterpret_cast<UIProcessInfo*>(param));

  // Make sure COM is initialized in this thread. This thread must be
  // initialized as an MTA or the call to enroll with MDM causes a crash in COM.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedCOMInitializer failed hr=" << putHR(hr);
    return hr;
  }

  CComBSTR status_text;
  DWORD exit_code;
  std::string json_result;
  HRESULT hr = WaitForLoginUIAndGetResult(uiprocinfo.get(), &json_result,
                                          &exit_code, &status_text);
  if (SUCCEEDED(hr)) {
    // Notify that the new user is created.
    // TODO(rogerta): Docs say this should not be called on a background
    // thread, but on the thread that received the
    // CGaiaCredentialBase::Advise() call. Seems to work for now though, but I
    // suspect there could be a problem if this call races with a call to
    // CGaiaCredentialBase::Unadvise().
    hr = uiprocinfo->credential->OnUserAuthenticated(
        CComBSTR(A2COLE(json_result.c_str())), &status_text);
  }

  // If the process was killed by the credential in Terminate(), don't process
  // the error message since it is possible that the credential and/or the
  // provider no longer exists.
  if (FAILED(hr) && exit_code != kUiecKilled) {
    if (hr != E_ABORT)
      LOGFN(ERROR) << "WaitForLoginUIAndGetResult hr=" << putHR(hr);

    // If hr is E_ABORT, this is a user initiated cancel.  Don't consider this
    // an error.
    LONG sts = hr == E_ABORT ? STATUS_SUCCESS : HRESULT_CODE(hr);

    // Either WaitForLoginUIAndGetResult did not fail or there should be an
    // error message to display.
    DCHECK(sts == STATUS_SUCCESS || status_text != nullptr);
    hr = uiprocinfo->credential->ReportError(sts, STATUS_SUCCESS, status_text);
    if (FAILED(hr))
      LOGFN(ERROR) << "uiprocinfo->credential->ReportError hr=" << putHR(hr);
  }

  LOGFN(INFO) << "done";
  return 0;
}

// static
HRESULT CGaiaCredentialBase::SaveAccountInfo(
    const base::DictionaryValue& properties) {
  LOGFN(INFO);

  base::string16 sid = GetDictString(&properties, kKeySID);
  if (sid.empty()) {
    LOGFN(ERROR) << "SID is empty";
    return E_INVALIDARG;
  }

  base::string16 username = GetDictString(&properties, kKeyUsername);
  if (username.empty()) {
    LOGFN(ERROR) << "Username is empty";
    return E_INVALIDARG;
  }

  base::string16 password = GetDictString(&properties, kKeyPassword);
  if (password.empty()) {
    LOGFN(ERROR) << "Password is empty";
    return E_INVALIDARG;
  }

  base::string16 domain = GetDictString(&properties, kKeyDomain);

  // Load the user's profile so that their registry hive is available.
  auto profile = ScopedUserProfile::Create(sid, domain, username, password);
  if (!profile) {
    LOGFN(ERROR) << "Could not load user profile";
    return E_UNEXPECTED;
  }

  HRESULT hr = profile->SaveAccountInfo(properties);
  if (FAILED(hr))
    LOGFN(ERROR) << "profile.SaveAccountInfo failed (cont) hr=" << putHR(hr);

  return hr;
}

HRESULT CGaiaCredentialBase::ReportResult(
    NTSTATUS status,
    NTSTATUS substatus,
    wchar_t** ppszOptionalStatusText,
    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) {
  LOGFN(INFO) << "status=" << putHR(status)
              << " substatus=" << putHR(substatus);

  if (status == STATUS_SUCCESS && authentication_results_) {
    // Update the sid, domain, username and password in
    // |authentication_results_| with the real Windows information for the user
    // so that the SaveAccountInfo process can correctly sign in to the user
    // account.
    authentication_results_->SetKey(
        kKeySID, base::Value(base::UTF16ToUTF8((BSTR)user_sid_)));
    authentication_results_->SetKey(
        kKeyDomain, base::Value(base::UTF16ToUTF8((BSTR)domain_)));
    authentication_results_->SetKey(
        kKeyUsername, base::Value(base::UTF16ToUTF8((BSTR)username_)));
    authentication_results_->SetKey(
        kKeyPassword, base::Value(base::UTF16ToUTF8((BSTR)password_)));

    // At this point the user and password stored in authentication_results_
    // should match what is stored in username_ and password_ so the
    // SaveAccountInfo process can be forked.
    CComBSTR status_text;
    HRESULT hr = ForkSaveAccountInfoStub(authentication_results_, &status_text);
    if (FAILED(hr))
      LOGFN(ERROR) << "ForkSaveAccountInfoStub hr=" << putHR(hr);
  }

  *ppszOptionalStatusText = nullptr;
  *pcpsiOptionalStatusIcon = CPSI_NONE;
  ResetInternalState();
  return S_OK;
}

HRESULT CGaiaCredentialBase::GetUserSid(wchar_t** sid) {
  *sid = nullptr;
  return S_FALSE;
}

HRESULT CGaiaCredentialBase::Initialize(IGaiaCredentialProvider* provider) {
  LOGFN(INFO);
  DCHECK(provider);

  provider_ = provider;
  return S_OK;
}

HRESULT CGaiaCredentialBase::Terminate() {
  LOGFN(INFO);
  SetDeselected();
  provider_.Release();
  return S_OK;
}

void CGaiaCredentialBase::TerminateLogonProcess() {
  // Terminate login UI process if started.  This is best effort since it may
  // have already terminated.
  if (logon_ui_process_ != INVALID_HANDLE_VALUE) {
    LOGFN(INFO) << "Attempting to kill logon UI process";
    ::TerminateProcess(logon_ui_process_, kUiecKilled);
    logon_ui_process_ = INVALID_HANDLE_VALUE;
  }
}

HRESULT CGaiaCredentialBase::ValidateOrCreateUser(
    const base::DictionaryValue* result,
    BSTR* domain,
    BSTR* username,
    BSTR* sid,
    BSTR* error_text) {
  LOGFN(INFO);
  DCHECK(domain);
  DCHECK(username);
  DCHECK(sid);
  DCHECK(error_text);
  DCHECK(sid);

  *error_text = nullptr;
  base::string16 local_password = GetDictString(result, kKeyPassword);

  wchar_t found_username[kWindowsUsernameBufferLength];
  wchar_t found_domain[kWindowsDomainBufferLength];
  wchar_t found_sid[kWindowsSidBufferLength];
  base::string16 gaia_id;
  MakeUsernameForAccount(
      result, &gaia_id, found_username, base::size(found_username),
      found_domain, base::size(found_domain), found_sid, base::size(found_sid));

  // If an existing user associated to the gaia id was found, make sure that it
  // is valid for this credential.
  if (found_sid[0]) {
    HRESULT hr = ValidateExistingUser(found_username, found_domain, found_sid,
                                      error_text);

    if (FAILED(hr)) {
      LOGFN(ERROR) << "ValidateExistingUser hr=" << putHR(hr);
      return hr;
    }

    *username = ::SysAllocString(found_username);
    *domain = ::SysAllocString(found_domain);
    *sid = ::SysAllocString(found_sid);

    return S_OK;
  }

  DWORD cpus = 0;
  provider()->GetUsageScenario(&cpus);

  // New users creation is not allowed during work station unlock. This code
  // prevents users from being created when the "Other User" tile appears on the
  // lock screen through certain system policy settings. In this situation only
  // the user who locked the computer is allowed to sign in.
  if (cpus == CPUS_UNLOCK_WORKSTATION) {
    *error_text = AllocErrorString(IDS_INVALID_UNLOCK_WORKSTATION_USER_BASE);
    return HRESULT_FROM_WIN32(ERROR_LOGON_TYPE_NOT_GRANTED);
    // This code prevents users from being created when the "Other User" tile
    // appears on the sign in scenario and only 1 user is allowed to use this
    // system.
  } else if (!CGaiaCredentialProvider::CanNewUsersBeCreated(
                 static_cast<CREDENTIAL_PROVIDER_USAGE_SCENARIO>(cpus))) {
    *error_text = AllocErrorString(IDS_ADD_USER_DISALLOWED_BASE);
    return HRESULT_FROM_WIN32(ERROR_LOGON_TYPE_NOT_GRANTED);
  }

  base::string16 local_fullname = GetDictString(result, kKeyFullname);
  base::string16 comment(GetStringResource(IDS_USER_ACCOUNT_COMMENT_BASE));
  HRESULT hr = CreateNewUser(
      OSUserManager::Get(), found_username, local_password.c_str(),
      local_fullname.c_str(), comment.c_str(),
      /*add_to_users_group=*/true, kMaxUsernameAttempts, username, sid);

  // May return user exists if this is the anonymous credential and the maximum
  // attempts to generate a new username has been reached.
  if (hr == HRESULT_FROM_WIN32(NERR_UserExists)) {
    LOGFN(ERROR) << "Could not find a new username based on desired username '"
                 << found_domain << "\\" << found_username
                 << "'. Maximum attempts reached.";
    *error_text = AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  }

  *domain = ::SysAllocString(found_domain);

  return hr;
}

HRESULT CGaiaCredentialBase::OnUserAuthenticated(BSTR authentication_info,
                                                 BSTR* status_text) {
  USES_CONVERSION;
  DCHECK(status_text);

  // Logon UI process is no longer needed and should already be finished by now
  // so clear the handle so that calls to HandleAutoLogon do not block further
  // processing thinking that there is still a logon process active.
  logon_ui_process_ = INVALID_HANDLE_VALUE;

  // Convert the string to a base::Dictionary and add the calculated username
  // to it to be passed to the SaveAccountInfo process.
  std::string json_string;
  base::UTF16ToUTF8(OLE2CW(authentication_info),
                    ::SysStringLen(authentication_info), &json_string);
  std::unique_ptr<base::Value> properties = base::JSONReader::ReadDeprecated(
      json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!properties || !properties->is_dict()) {
    LOGFN(ERROR) << "base::JSONReader::Read failed to translate to JSON";
    *status_text = AllocErrorString(IDS_INVALID_UI_RESPONSE_BASE);
    return E_FAIL;
  }

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(properties));

  HRESULT hr = ValidateResult(dict.get(), status_text);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "ValidateResult hr=" << putHR(hr);
    return hr;
  }

  hr = credential_provider::EnrollToGoogleMdmIfNeeded(*dict);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "EnrollToGoogleMdmIfNeeded hr=" << putHR(hr);
    *status_text = AllocErrorString(IDS_MDM_ENROLLMENT_FAILED_BASE);
    return hr;
  }

  // The value in |dict| is now known to contain everything that is needed
  // from the GLS. Try to validate the user that wants to sign in and then
  // add additional information into |dict| as needed.
  hr = ValidateOrCreateUser(dict.get(), &domain_, &username_, &user_sid_,
                            status_text);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "ValidateOrCreateUser hr=" << putHR(hr);
    return hr;
  }

  authentication_results_ = std::move(dict);

  password_ = ::SysAllocString(
      GetDictString(authentication_results_, kKeyPassword).c_str());

  // Disable the submit button. Either the signon will succeed with the given
  // credentials or a password update will be needed and that flow will handle
  // re-enabling the submit button in HandleAutoLogon.
  if (events_)
    events_->SetFieldInteractiveState(this, FID_SUBMIT, CPFIS_DISABLED);

  // Check if the credentials are valid for the user. If they aren't show the
  // password update prompt and continue without authenticating on the provider.
  if (!AreCredentialsValid()) {
    DisplayPasswordField(IDS_PASSWORD_UPDATE_NEEDED_BASE);
    return S_FALSE;
  }

  result_status_ = STATUS_SUCCESS;

  // When this function returns, winlogon will be told to logon to the newly
  // created account.  This is important, as the save account info process
  // can't actually save the info until the user's profile is created, which
  // happens on first logon.
  return provider_->OnUserAuthenticated(static_cast<IGaiaCredential*>(this),
                                        username_, password_, user_sid_, TRUE);
}

HRESULT CGaiaCredentialBase::ReportError(LONG status,
                                         LONG substatus,
                                         BSTR status_text) {
  USES_CONVERSION;
  LOGFN(INFO);

  result_status_ = status;

  // If the user cancelled out of the logon, the process may be already
  // terminated, but if the handle to the process is still valid the
  // credential provider will not start a new GLS process when requested so
  // try to terminate the logon process now and clear the handle.
  TerminateLogonProcess();
  UpdateSubmitButtonInteractiveState();

  DisplayErrorInUI(status, STATUS_SUCCESS, status_text);

  return provider_->OnUserAuthenticated(nullptr, CComBSTR(), CComBSTR(),
                                        CComBSTR(), FALSE);
}

void CGaiaCredentialBase::UpdateSubmitButtonInteractiveState() {
  if (events_) {
    bool should_enable =
        logon_ui_process_ == INVALID_HANDLE_VALUE &&
        (!needs_windows_password_ || current_windows_password_.Length());
    events_->SetFieldInteractiveState(
        this, FID_SUBMIT, should_enable ? CPFIS_NONE : CPFIS_DISABLED);
  }
}

void CGaiaCredentialBase::DisplayPasswordField(int password_message) {
  needs_windows_password_ = true;
  if (events_) {
    events_->SetFieldString(this, FID_DESCRIPTION,
                            GetStringResource(password_message).c_str());
    events_->SetFieldState(this, FID_CURRENT_PASSWORD_FIELD,
                           CPFS_DISPLAY_IN_SELECTED_TILE);
    events_->SetFieldInteractiveState(this, FID_CURRENT_PASSWORD_FIELD,
                                      CPFIS_FOCUSED);
    events_->SetFieldSubmitButton(this, FID_SUBMIT, FID_CURRENT_PASSWORD_FIELD);
  }
}

HRESULT CGaiaCredentialBase::ValidateExistingUser(
    const base::string16& username,
    const base::string16& domain,
    const base::string16& sid,
    BSTR* error_text) {
  return S_OK;
}

}  // namespace credential_provider
