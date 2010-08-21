// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl_client_certificate_selector.h"

#include <windows.h>
#include <cryptuiapi.h>
#pragma comment(lib, "cryptui.lib")

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/ssl/ssl_client_auth_handler.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"

namespace browser {

void ShowSSLClientCertificateSelector(
    TabContents* parent,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate) {
  // TODO(jcampan): replace this with our own cert selection dialog.
  // CryptUIDlgSelectCertificateFromStore is blocking (but still processes
  // Windows messages), which is scary.
  //
  // TODO(davidben): Make this dialog tab-modal to the
  // TabContents. This depends on the above TODO.
  HCERTSTORE client_certs = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL,
                                          0, NULL);
  BOOL ok;
  for (size_t i = 0; i < cert_request_info->client_certs.size(); ++i) {
    PCCERT_CONTEXT cc = cert_request_info->client_certs[i]->os_cert_handle();
    ok = CertAddCertificateContextToStore(client_certs, cc,
                                          CERT_STORE_ADD_ALWAYS, NULL);
    DCHECK(ok);
  }

  std::wstring title = l10n_util::GetString(IDS_CLIENT_CERT_DIALOG_TITLE);
  std::wstring text = l10n_util::GetStringF(
      IDS_CLIENT_CERT_DIALOG_TEXT,
      ASCIIToWide(cert_request_info->host_and_port));
  PCCERT_CONTEXT cert_context = CryptUIDlgSelectCertificateFromStore(
      client_certs, parent->GetMessageBoxRootWindow(),
      title.c_str(), text.c_str(), 0, 0, NULL);

  net::X509Certificate* cert = NULL;
  if (cert_context) {
    for (size_t i = 0; i < cert_request_info->client_certs.size(); ++i) {
      net::X509Certificate* client_cert = cert_request_info->client_certs[i];
      if (net::X509Certificate::IsSameOSCert(cert_context,
                                             client_cert->os_cert_handle())) {
        cert = client_cert;
        break;
      }
    }
    DCHECK(cert != NULL);
    net::X509Certificate::FreeOSCertHandle(cert_context);
  }

  ok = CertCloseStore(client_certs, CERT_CLOSE_STORE_CHECK_FLAG);
  DCHECK(ok);

  delegate->CertificateSelected(cert);
}

}  // namespace browser
