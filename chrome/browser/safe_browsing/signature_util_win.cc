// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/signature_util.h"

#include <windows.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "crypto/scoped_capi_types.h"
#include "chrome/common/safe_browsing/csd.pb.h"

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "wintrust.lib")

namespace safe_browsing {
namespace {
using crypto::ScopedCAPIHandle;
using crypto::CAPIDestroyer;
using crypto::CAPIDestroyerWithFlags;

// Free functor for scoped_ptr_malloc.
class FreeConstCertContext {
 public:
  void operator()(const CERT_CONTEXT* cert_context) const {
    CertFreeCertificateContext(cert_context);
  }
};

// Tries to extract the signing certificate from |file_path|.  On success,
// the |certificate_contents| field of |signature_info| is populated with the
// DER-encoded X.509 certificate.
void GetCertificateContents(
    const FilePath& file_path,
    ClientDownloadRequest_SignatureInfo* signature_info) {
  // Largely based on http://support.microsoft.com/kb/323809
  // Get message handle and store handle from the signed file.
  typedef CAPIDestroyer<HCRYPTMSG, CryptMsgClose> CryptMsgDestroyer;
  ScopedCAPIHandle<HCRYPTMSG, CryptMsgDestroyer> crypt_msg;
  typedef CAPIDestroyerWithFlags<
      HCERTSTORE, CertCloseStore, 0> CertStoreDestroyer;
  ScopedCAPIHandle<HCERTSTORE, CertStoreDestroyer> cert_store;

  VLOG(2) << "Looking for signature in: " << file_path.value();
  if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                        file_path.value().c_str(),
                        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                        CERT_QUERY_FORMAT_FLAG_BINARY,
                        0,      // flags
                        NULL,   // encoding
                        NULL,   // content type
                        NULL,   // format type
                        cert_store.receive(),
                        crypt_msg.receive(),
                        NULL)) {  // context
    VLOG(2) << "No signature found.";
    return;
  }

  // Get the signer information size.
  DWORD signer_info_size;
  if (!CryptMsgGetParam(crypt_msg.get(),
                        CMSG_SIGNER_INFO_PARAM,
                        0,  // index
                        NULL,  // no buffer when checking the size
                        &signer_info_size)) {
    VLOG(2) << "Failed to get signer info size";
    return;
  }

  // Get the signer information.
  scoped_array<BYTE> signer_info_buffer(new BYTE[signer_info_size]);
  CMSG_SIGNER_INFO* signer_info =
      reinterpret_cast<CMSG_SIGNER_INFO*>(signer_info_buffer.get());
  if (!CryptMsgGetParam(crypt_msg.get(),
                        CMSG_SIGNER_INFO_PARAM,
                        0,  // index
                        signer_info,
                        &signer_info_size)) {
    VLOG(2) << "Failed to get signer info";
    return;
  }

  // Search for the signer certificate in the temporary certificate store.
  CERT_INFO cert_info;
  cert_info.Issuer = signer_info->Issuer;
  cert_info.SerialNumber = signer_info->SerialNumber;

  scoped_ptr_malloc<const CERT_CONTEXT, FreeConstCertContext> cert_context(
      CertFindCertificateInStore(
          cert_store.get(),
          X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
          0, // flags
          CERT_FIND_SUBJECT_CERT,
          &cert_info,
          NULL));  // no previous context
  if (!cert_context.get()) {
    VLOG(2) << "Failed to get CERT_CONTEXT";
    return;
  }

  signature_info->set_certificate_contents(cert_context->pbCertEncoded,
                                           cert_context->cbCertEncoded);
  VLOG(2) << "Successfully extracted cert";
}

bool CheckTrust(const FilePath& file_path) {
  WINTRUST_FILE_INFO file_info;
  file_info.cbStruct = sizeof(file_info);
  file_info.pcwszFilePath = file_path.value().c_str();
  file_info.hFile = NULL;
  file_info.pgKnownSubject = NULL;

  WINTRUST_DATA wintrust_data;
  wintrust_data.cbStruct = sizeof(wintrust_data);
  wintrust_data.pPolicyCallbackData = NULL;
  wintrust_data.pSIPClientData = NULL;
  wintrust_data.dwUIChoice = WTD_UI_NONE;
  wintrust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  wintrust_data.dwUnionChoice = WTD_CHOICE_FILE;
  wintrust_data.pFile = &file_info;
  wintrust_data.dwStateAction = WTD_STATEACTION_IGNORE;
  wintrust_data.hWVTStateData = NULL;
  wintrust_data.pwszURLReference = NULL;
  wintrust_data.dwProvFlags = 0;  // don't set any flags
  wintrust_data.dwUIContext = WTD_UICONTEXT_EXECUTE;

  // The WINTRUST_ACTION_GENERIC_VERIFY_V2 policy verifies that the certificate
  // chains up to a trusted root CA, and that it has appropriate permission to
  // sign code.
  GUID policy_guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  return (WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                         &policy_guid,
                         &wintrust_data) == ERROR_SUCCESS);
}
}  // namespace

SignatureUtil::SignatureUtil() {}

SignatureUtil::~SignatureUtil() {}

void SignatureUtil::CheckSignature(
    const FilePath& file_path,
    ClientDownloadRequest_SignatureInfo* signature_info) {
  GetCertificateContents(file_path, signature_info);
  signature_info->set_trusted(CheckTrust(file_path));
}

}  // namespace safe_browsing
