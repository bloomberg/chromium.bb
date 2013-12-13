// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/signature_validator_win.h"

#include <atlstr.h>
#include <softpub.h>
#include <wincrypt.h>
#include <windows.h>
#include <wintrust.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "crypto/sha2.h"

namespace {

bool ExtractPublicKeyHash(const CERT_CONTEXT* cert_context,
                          std::string* public_key_hash) {
  public_key_hash->clear();

  CRYPT_BIT_BLOB crypt_blob =
      cert_context->pCertInfo->SubjectPublicKeyInfo.PublicKey;

  // Key blobs that are not an integral number of bytes are unsupported.
  if (crypt_blob.cUnusedBits != 0)
    return false;

  uint8 hash[crypto::kSHA256Length] = {};

  base::StringPiece key_bytes(reinterpret_cast<char*>(
      crypt_blob.pbData), crypt_blob.cbData);
  crypto::SHA256HashString(key_bytes, hash, crypto::kSHA256Length);

  *public_key_hash = StringToLowerASCII(base::HexEncode(hash, arraysize(hash)));
  return true;
}

// The traits class for HCERTSTORE handles that can be closed via
// CertCloseStore() API.
class CertStoreHandleTraits {
 public:
  typedef HCERTSTORE Handle;

  // Closes the handle.
  static bool CloseHandle(HCERTSTORE handle) {
    return CertCloseStore(handle, 0) != FALSE;
  }

  // Returns true if the handle value is valid.
  static bool IsHandleValid(HCERTSTORE handle) {
    return handle != NULL;
  }

  // Returns NULL handle value.
  static HANDLE NullHandle() {
    return NULL;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CertStoreHandleTraits);
};

typedef base::win::GenericScopedHandle<CertStoreHandleTraits,
    base::win::DummyVerifierTraits> ScopedCertStoreHandle;

// Class: CertInfo
//
// CertInfo holds all sensible details of a certificate. During verification of
// a signature, one CertInfo object is made for each certificate encountered in
// the signature.
class CertInfo {
 public:
  explicit CertInfo(const CERT_CONTEXT* given_cert_context)
      : cert_context_(NULL) {
    if (given_cert_context) {
      // CertDuplicateCertificateContext just increases reference count of a
      // given CERT_CONTEXT.
      cert_context_ = CertDuplicateCertificateContext(given_cert_context);
      not_valid_before_ = cert_context_->pCertInfo->NotBefore;
      not_valid_after_ = cert_context_->pCertInfo->NotAfter;

      ExtractPublicKeyHash(cert_context_, &public_key_hash_);
    }
  }

  ~CertInfo() {  // Decrement reference count, if needed.
    if (cert_context_) {
      CertFreeCertificateContext(cert_context_);
      cert_context_ = NULL;
    }
  }

  // IsValidNow() functions returns true if this certificate is valid at this
  // moment, based on the validity period specified in the certificate.
  bool IsValidNow() const {
    // we cannot directly get current time in FILETIME format.
    // so first get it in SYSTEMTIME format and convert it into FILETIME.
    base::Time now = base::Time::NowFromSystemTime();
    FILETIME filetime_now = now.ToFileTime();
    // CompareFileTime() is a windows function
    return ((CompareFileTime(&filetime_now, &not_valid_before_) > 0) &&
            (CompareFileTime(&filetime_now, &not_valid_after_) < 0));
  }

  std::string public_key_hash() const {
    return public_key_hash_;
  }

 private:
  // cert_context structure, defined by Crypto API, contains all the info
  // about the certificate.
  const CERT_CONTEXT* cert_context_;

  // Lower-case hex SHA-256 hash of the certificate subject's public key.
  std::string public_key_hash_;

  // Validity period start-date
  FILETIME not_valid_before_;

  // Validity period end-date
  FILETIME not_valid_after_;
};

}  // namespace

bool VerifyAuthenticodeSignature(const base::FilePath& signed_file) {
  // Don't pop up any windows
  const HWND window_mode = reinterpret_cast<HWND>(INVALID_HANDLE_VALUE);

  // Verify file & certificates using the Microsoft Authenticode Policy
  // Provider.
  GUID verification_type = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  // Info for the file we're going to verify.
  WINTRUST_FILE_INFO file_info = {};
  file_info.cbStruct = sizeof(file_info);
  file_info.pcwszFilePath = signed_file.value().c_str();

  // Info for request to WinVerifyTrust.
  WINTRUST_DATA trust_data = {};
  trust_data.cbStruct = sizeof(trust_data);
  trust_data.dwUIChoice = WTD_UI_NONE;               // no graphics
  trust_data.fdwRevocationChecks = WTD_REVOKE_NONE;  // no revocation checking
  trust_data.dwUnionChoice = WTD_CHOICE_FILE;        // check a file
  trust_data.pFile = &file_info;                     // check this file
  trust_data.dwProvFlags = WTD_REVOCATION_CHECK_NONE;

  // From the WinVerifyTrust documentation:
  //   http://msdn2.microsoft.com/en-us/library/aa388208.aspx:
  //   "If the trust provider verifies that the subject is trusted
  //   for the specified action, the return value is zero. No other
  //   value besides zero should be considered a successful return."
  LONG result = WinVerifyTrust(window_mode, &verification_type, &trust_data);
  return !result;
}

bool VerifySignerIsGoogle(const base::FilePath& signed_file,
                          const std::string& subject_name,
                          const std::vector<std::string>& expected_hashes) {
  if (signed_file.empty())
    return false;

  // If successful, cert_store will be populated by a store containing all the
  // certificates related to the file signature.
  HCERTSTORE cert_store = NULL;

  BOOL succeeded = CryptQueryObject(
      CERT_QUERY_OBJECT_FILE,
      signed_file.value().c_str(),
      CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
      CERT_QUERY_FORMAT_FLAG_ALL,
      0,               // Reserved: must be 0.
      NULL,
      NULL,
      NULL,
      &cert_store,
      NULL,            // Do not return the message.
      NULL);           // Do not return additional context.

  ScopedCertStoreHandle scoped_cert_store(cert_store);

  if (!succeeded || !scoped_cert_store.IsValid())
    return false;

  PCCERT_CONTEXT cert_context_ptr = NULL;
  cert_context_ptr = CertFindCertificateInStore(
      cert_store,
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      0,
      CERT_FIND_SUBJECT_STR,
      base::UTF8ToWide(subject_name).c_str(),
      cert_context_ptr);

  // No cert found with this subject, so stop here.
  if (!cert_context_ptr)
    return false;

  CertInfo cert_info(cert_context_ptr);

  CertFreeCertificateContext(cert_context_ptr);
  cert_context_ptr = NULL;

  // Check the hashes to make sure subject isn't being faked, and the time
  // to make sure the cert is current.
  std::vector<std::string>::const_iterator it = std::find(
      expected_hashes.begin(),
      expected_hashes.end(),
      cert_info.public_key_hash());
  if (it == expected_hashes.end() || !cert_info.IsValidNow())
    return false;

  return true;
}
