// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_impl.h"

#include <nss.h>
#include <ssl.h>

#include "base/logging.h"
#include "net/cert/x509_util.h"

namespace net {

namespace {

bool GetClientCertsImpl(CERTCertList* cert_list,
                        const SSLCertRequestInfo& request,
                        CertificateList* selected_certs) {
  DCHECK(cert_list);
  DCHECK(selected_certs);

  selected_certs->clear();
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    // Only offer unexpired certificates.
    if (CERT_CheckCertValidTimes(node->cert, PR_Now(), PR_TRUE) !=
        secCertTimeValid) {
      continue;
    }

    scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
        node->cert, X509Certificate::OSCertHandles());

    // Check if the certificate issuer is allowed by the server.
    if (!request.cert_authorities.empty() &&
        !cert->IsIssuedByEncoded(request.cert_authorities)) {
      continue;
    }
    selected_certs->push_back(cert);
  }

  std::sort(selected_certs->begin(), selected_certs->end(),
            x509_util::ClientCertSorter());
  return true;
}

}  // namespace

bool ClientCertStoreImpl::GetClientCerts(const SSLCertRequestInfo& request,
                                         CertificateList* selected_certs) {
  CERTCertList* client_certs = CERT_FindUserCertsByUsage(
      CERT_GetDefaultCertDB(), certUsageSSLClient,
      PR_FALSE, PR_FALSE, NULL);
  // It is ok for a user not to have any client certs.
  if (!client_certs)
    return true;

  bool rv = GetClientCertsImpl(client_certs, request, selected_certs);
  CERT_DestroyCertList(client_certs);
  return rv;
}

bool ClientCertStoreImpl::SelectClientCerts(const CertificateList& input_certs,
                                            const SSLCertRequestInfo& request,
                                            CertificateList* selected_certs) {
  CERTCertList* cert_list = CERT_NewCertList();
  if (!cert_list)
    return false;
  for (size_t i = 0; i < input_certs.size(); ++i) {
    CERT_AddCertToListTail(
        cert_list, CERT_DupCertificate(input_certs[i]->os_cert_handle()));
  }

  bool rv = GetClientCertsImpl(cert_list, request, selected_certs);
  CERT_DestroyCertList(cert_list);
  return rv;
}

}  // namespace net
