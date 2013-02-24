// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/signature_util.h"

#include <windows.h>
#include <softpub.h>
#include <wintrust.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/common/safe_browsing/csd.pb.h"

#pragma comment(lib, "wintrust.lib")

namespace safe_browsing {

SignatureUtil::SignatureUtil() {}

SignatureUtil::~SignatureUtil() {}

void SignatureUtil::CheckSignature(
    const base::FilePath& file_path,
    ClientDownloadRequest_SignatureInfo* signature_info) {
  VLOG(2) << "Checking signature for " << file_path.value();

  WINTRUST_FILE_INFO file_info = {0};
  file_info.cbStruct = sizeof(file_info);
  file_info.pcwszFilePath = file_path.value().c_str();
  file_info.hFile = NULL;
  file_info.pgKnownSubject = NULL;

  WINTRUST_DATA wintrust_data = {0};
  wintrust_data.cbStruct = sizeof(wintrust_data);
  wintrust_data.pPolicyCallbackData = NULL;
  wintrust_data.pSIPClientData = NULL;
  wintrust_data.dwUIChoice = WTD_UI_NONE;
  wintrust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  wintrust_data.dwUnionChoice = WTD_CHOICE_FILE;
  wintrust_data.pFile = &file_info;
  wintrust_data.dwStateAction = WTD_STATEACTION_VERIFY;
  wintrust_data.hWVTStateData = NULL;
  wintrust_data.pwszURLReference = NULL;
  // Disallow revocation checks over the network.
  wintrust_data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
  wintrust_data.dwUIContext = WTD_UICONTEXT_EXECUTE;

  // The WINTRUST_ACTION_GENERIC_VERIFY_V2 policy verifies that the certificate
  // chains up to a trusted root CA, and that it has appropriate permission to
  // sign code.
  GUID policy_guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  LONG result = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                               &policy_guid,
                               &wintrust_data);

  CRYPT_PROVIDER_DATA* prov_data = WTHelperProvDataFromStateData(
      wintrust_data.hWVTStateData);
  if (prov_data) {
    if (prov_data->csSigners > 0) {
      signature_info->set_trusted(result == ERROR_SUCCESS);
    }
    for (DWORD i = 0; i < prov_data->csSigners; ++i) {
      const CERT_CHAIN_CONTEXT* cert_chain_context =
          prov_data->pasSigners[i].pChainContext;
      if (!cert_chain_context)
        break;
      for (DWORD j = 0; j < cert_chain_context->cChain; ++j) {
        CERT_SIMPLE_CHAIN* simple_chain = cert_chain_context->rgpChain[j];
        ClientDownloadRequest_CertificateChain* chain =
            signature_info->add_certificate_chain();
        if (!simple_chain)
          break;
        for (DWORD k = 0; k < simple_chain->cElement; ++k) {
          CERT_CHAIN_ELEMENT* element = simple_chain->rgpElement[k];
          chain->add_element()->set_certificate(
              element->pCertContext->pbCertEncoded,
              element->pCertContext->cbCertEncoded);
        }
      }
    }

    // Free the provider data.
    wintrust_data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                   &policy_guid, &wintrust_data);
  }
}

}  // namespace safe_browsing
