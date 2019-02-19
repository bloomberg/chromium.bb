// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of Windows authentication package building functions need
// to create the authentication packages used to sign the user into a Windows
// system.

#include <vector>

#include "chrome/credential_provider/gaiacp/auth_utils.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/stl_util.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

namespace {

// If |password| should be encrypted, fills |protected_password| with a copy
// encrypted with CredProtect, otherwise just copies |password| to
// |protected_password|.
//
// CredProtectW and CredIsProtectedW expect a non-const password so |password|
// is passed in as a non-const pointer.
HRESULT ProtectIfNecessaryAndCopyPassword(
    wchar_t* password,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    std::vector<wchar_t>* protected_password) {
  DCHECK(protected_password);
  DCHECK(password);

  protected_password->clear();

  if (cpus != CPUS_UNLOCK_WORKSTATION && cpus != CPUS_LOGON)
    return E_UNEXPECTED;

  DWORD password_length = static_cast<DWORD>(wcslen(password));

  // ProtectAndCopyString is intended for non-empty strings only.  Empty
  // passwords do not need to be encrypted.
  if (!password_length)
    return S_OK;

  CRED_PROTECTION_TYPE protection_type;
  // Determine if password can be encrypted.
  // Encryption should be skipped if the password is already encrypted.
  // An encrypted password may be received through SetSerialization in the
  // CPUS_LOGON scenario during a Terminal Services connection, for
  // instance.
  if ((!::CredIsProtectedW(password, &protection_type) ||
       CredUnprotected != protection_type)) {
    protected_password->resize(password_length + 1);
    wcscpy_s(&(*protected_password)[0], password_length + 1, password);
    return S_OK;
  }

  // Determine the size of the buffer needed to generate the protected string.
  //
  // Note that the third parameter to CredProtect, the number of characters of
  // password to encrypt, must include the NULL terminator.
  DWORD needed_length = 0;
  if (::CredProtectW(FALSE, password, password_length + 1, nullptr,
                     &needed_length, nullptr)) {
    LOGFN(ERROR) << "Failed to get required length of protected string.";
    return E_UNEXPECTED;
  }

  DWORD last_error = GetLastError();
  if ((ERROR_INSUFFICIENT_BUFFER != last_error) || (0 == needed_length)) {
    LOGFN(ERROR) << "Unexpected error when getting length of proceted string.";
    return HRESULT_FROM_WIN32(last_error);
  }

  protected_password->resize(needed_length);
  if (!::CredProtectW(FALSE, password, password_length + 1,
                      &(*protected_password)[0], &needed_length, nullptr)) {
    LOGFN(ERROR) << "Failed to protect string.";
    protected_password->clear();
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  return S_OK;
}

// The following function is intended to be used ONLY with the Kerb*Pack
// functions.  It does no bounds-checking because its callers have precise
// requirements and are written to respect its limitations. You can read more
// about the UNICODE_STRING type at:
// https://docs.microsoft.com/en-us/windows/desktop/api/subauth/ns-subauth-_unicode_string
void UnicodeStringPackedUnicodeStringCopy(const UNICODE_STRING& rus,
                                          wchar_t* buffer,
                                          UNICODE_STRING* pus) {
  pus->Length = rus.Length;
  pus->MaximumLength = rus.Length;
  pus->Buffer = buffer;

  CopyMemory(pus->Buffer, rus.Buffer, pus->Length);
}

// Initialize the members of a KERB_INTERACTIVE_UNLOCK_LOGON with weak
// references to the passed-in strings.  Later KerbInteractiveUnlockLogonPack
// will be used to serialize the structure so the strings will not need to
// be copied.
//
// The password is stored in encrypted form for CPUS_LOGON and
// CPUS_UNLOCK_WORKSTATION because the system can accept encrypted credentials.
// For all other scenarios, this function will simply fail.
void KerbInteractiveUnlockLogonInit(wchar_t* domain,
                                    wchar_t* username,
                                    wchar_t* password,
                                    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                                    KERB_INTERACTIVE_UNLOCK_LOGON* pkiul) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(password);
  DCHECK(pkiul);

  ZeroMemory(pkiul, sizeof(KERB_INTERACTIVE_UNLOCK_LOGON));

  KERB_INTERACTIVE_LOGON* pkil = &pkiul->Logon;

  // Note: this method uses custom logic to pack a KERB_INTERACTIVE_UNLOCK_LOGON
  // with a serialized credential.  We could replace the calls to
  // InitWindowsStringWithString and KerbInteractiveUnlockLogonPack with a
  // single call to CredPackAuthenticationBuffer, but that API has a drawback:
  // it returns a KERB_INTERACTIVE_UNLOCK_LOGON whose MessageType is always
  // KerbInteractiveLogon.
  //
  // If we only handled CPUS_LOGON, this drawback would not be a problem.  For
  // CPUS_UNLOCK_WORKSTATION, we could cast the output buffer of
  // CredPackAuthenticationBuffer to KERB_INTERACTIVE_UNLOCK_LOGON and modify
  // the MessageType to KerbWorkstationUnlockLogon, but such a cast would be
  // unsupported -- the output format of CredPackAuthenticationBuffer is not
  // officially documented.

  // Set a MessageType based on the usage scenario.
  switch (cpus) {
    case CPUS_UNLOCK_WORKSTATION:
      pkil->MessageType = KerbWorkstationUnlockLogon;
      break;
    case CPUS_LOGON:
      pkil->MessageType = KerbInteractiveLogon;
      break;
    default:
      NOTREACHED();
      return;
  }

  // Initialize the UNICODE_STRINGS to share domain, username and password
  // strings.
  InitWindowsStringWithString(domain, &pkil->LogonDomainName);
  InitWindowsStringWithString(username, &pkil->UserName);
  InitWindowsStringWithString(password, &pkil->Password);
}

// WinLogon and LSA consume "packed" KERB_INTERACTIVE_UNLOCK_LOGONs.  In these,
// the PWSTR members of each UNICODE_STRING are not actually pointers but byte
// offsets into the overall buffer represented by the packed
// KERB_INTERACTIVE_UNLOCK_LOGON.  For example:
//
// Length is in bytes, not characters
// input_logon.Logon.LogonDomainName.Length = 14;
// LogonDomainName begins immediately after the KERB_... struct in the buffer
// input_logon.Logon.LogonDomainName.Buffer =
//      sizeof(KERB_INTERACTIVE_UNLOCK_LOGON);
// input_logon.Logon.UserName.Length = 10
// input_logon.Logon.UserName.Buffer = sizeof(KERB_INTERACTIVE_UNLOCK_LOGON) +
// 14 -> UNICODE_STRINGS are NOT null-terminated
//
// input_logon.Logon.Password.Length = 16
// input_logon.Logon.Password.Buffer = sizeof(KERB_INTERACTIVE_UNLOCK_LOGON) +
// 14 + 10
HRESULT KerbInteractiveUnlockLogonPack(
    const KERB_INTERACTIVE_UNLOCK_LOGON& input_unlock_logon,
    BYTE** prgb,
    DWORD* pcb) {
  DCHECK(prgb);
  DCHECK(pcb);
  const KERB_INTERACTIVE_LOGON* input_logon = &input_unlock_logon.Logon;

  // Allocate space for struct plus extra for the three strings.
  DWORD cb = sizeof(input_unlock_logon) + input_logon->LogonDomainName.Length +
             input_logon->UserName.Length + input_logon->Password.Length;

  KERB_INTERACTIVE_UNLOCK_LOGON* output_unlock_logon =
      reinterpret_cast<KERB_INTERACTIVE_UNLOCK_LOGON*>(CoTaskMemAlloc(cb));
  if (!output_unlock_logon) {
    LOGFN(ERROR) << "Failed to allocate kerberos logon package.";
    return E_OUTOFMEMORY;
  }

  ::SecureZeroMemory(&output_unlock_logon->LogonId,
                     sizeof(output_unlock_logon->LogonId));

  // Point output_buffer at the beginning of the extra space.
  BYTE* output_buffer = reinterpret_cast<BYTE*>(output_unlock_logon) +
                        sizeof(*output_unlock_logon);

  // Set up the Logon structure within the KERB_INTERACTIVE_UNLOCK_LOGON.
  KERB_INTERACTIVE_LOGON* output_logon = &output_unlock_logon->Logon;

  output_logon->MessageType = input_logon->MessageType;

  // For each string: fix up appropriate buffer pointer to be offset, advance
  // buffer pointer over copied characters in extra space.
  UnicodeStringPackedUnicodeStringCopy(input_logon->LogonDomainName,
                                       (wchar_t*)output_buffer,
                                       &output_logon->LogonDomainName);
  output_logon->LogonDomainName.Buffer =
      reinterpret_cast<wchar_t*>(output_buffer - (BYTE*)output_unlock_logon);
  output_buffer += output_logon->LogonDomainName.Length;

  UnicodeStringPackedUnicodeStringCopy(
      input_logon->UserName, reinterpret_cast<wchar_t*>(output_buffer),
      &output_logon->UserName);
  output_logon->UserName.Buffer =
      reinterpret_cast<wchar_t*>(output_buffer - (BYTE*)output_unlock_logon);
  output_buffer += output_logon->UserName.Length;

  UnicodeStringPackedUnicodeStringCopy(
      input_logon->Password, reinterpret_cast<wchar_t*>(output_buffer),
      &output_logon->Password);
  output_logon->Password.Buffer =
      reinterpret_cast<wchar_t*>(output_buffer - (BYTE*)output_unlock_logon);

  *prgb = (BYTE*)output_unlock_logon;
  *pcb = cb;

  return S_OK;
}

// Gets the auth package id for NEGOSSP_NAME_A.
HRESULT GetAuthenticationPackageId(ULONG* id) {
  DCHECK(id);

  HANDLE lsa;
  NTSTATUS status = ::LsaConnectUntrusted(&lsa);
  HRESULT hr = HRESULT_FROM_NT(status);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "LsaConnectUntrusted hr=" << putHR(hr);
    return hr;
  }

  LSA_STRING name;
  InitWindowsStringWithString(NEGOSSP_NAME_A, &name);

  status = ::LsaLookupAuthenticationPackage(lsa, &name, id);
  ::LsaDeregisterLogonProcess(lsa);
  hr = HRESULT_FROM_NT(status);
  if (FAILED(hr))
    LOGFN(ERROR) << "LsaLookupAuthenticationPackage hr=" << putHR(hr);

  return hr;
}

}  // namespace

HRESULT BuildCredPackAuthenticationBuffer(
    BSTR domain,
    BSTR username,
    BSTR password,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs) {
  DCHECK(username);
  DCHECK(password);
  DCHECK(cpcs);

  HRESULT hr = GetAuthenticationPackageId(&(cpcs->ulAuthenticationPackage));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetAuthenticationPackageId hr=" << putHR(hr);
    return hr;
  }

  // Caller should fill this in.
  cpcs->clsidCredentialProvider = GUID_NULL;

  std::vector<wchar_t> protected_password;
  // Copy the password and pass the copied buffer into
  // ProtectIfNecessaryAndCopyPassword since it expects a non-const input
  // buffer.
  std::vector<wchar_t> copy_password(OLE2W(password),
                                     OLE2W(password) + wcslen(password) + 1);
  hr = ProtectIfNecessaryAndCopyPassword(&copy_password[0], cpus,
                                         &protected_password);

  // Zero out the unencrypted copy of the password.
  ::RtlSecureZeroMemory(&copy_password[0], copy_password.size());
  if (FAILED(hr)) {
    LOGFN(ERROR) << "ProtectIfNecessaryAndCopyPassword hr=" << putHR(hr);
    return hr;
  }

  // Protected password may still be insecure so make sure to zero it out.
  base::ScopedClosureRunner zero_buffer_on_exit(
      base::BindOnce(base::IgnoreResult(&RtlSecureZeroMemory),
                     &protected_password[0], protected_password.size()));

  wchar_t* logon_domain = domain;

  // For domain joined users, use a Kerberos authentication package.
  KERB_INTERACTIVE_UNLOCK_LOGON kiul;
  KerbInteractiveUnlockLogonInit(OLE2W(logon_domain), OLE2W(username),
                                 &protected_password[0], cpus, &kiul);

  // Use KERB_INTERACTIVE_UNLOCK_LOGON in both unlock and logon
  // scenarios. It contains a KERB_INTERACTIVE_LOGON to hold the creds
  // plus a LUID that is filled in for us by Winlogon as necessary.
  hr = KerbInteractiveUnlockLogonPack(kiul, &cpcs->rgbSerialization,
                                      &cpcs->cbSerialization);
  if (FAILED(hr)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "KerbInteractiveUnlockLogonPack user=" << username
                 << " dn=" << logon_domain << " hr=" << putHR(hr)
                 << " length=" << cpcs->cbSerialization;
    return hr;
  }

  return S_OK;
}

}  // namespace credential_provider
