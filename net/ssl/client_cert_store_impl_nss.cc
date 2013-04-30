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

// Examines the certificates in |cert_list| to find all certificates that match
// the client certificate request in |request|, storing the matching
// certificates in |selected_certs|.
// If |query_nssdb| is true, NSS will be queried to construct full certificate
// chains. If it is false, only the certificate will be considered.
bool GetClientCertsImpl(CERTCertList* cert_list,
                        const SSLCertRequestInfo& request,
                        bool query_nssdb,
                        CertificateList* selected_certs) {
  DCHECK(cert_list);
  DCHECK(selected_certs);

  selected_certs->clear();

  // Create a "fake" CERTDistNames structure. No public API exists to create
  // one from a list of issuers.
  CERTDistNames ca_names;
  ca_names.arena = NULL;
  ca_names.nnames = 0;
  ca_names.names = NULL;
  ca_names.head = NULL;

  std::vector<SECItem> ca_names_items(request.cert_authorities.size());
  for (size_t i = 0; i < request.cert_authorities.size(); ++i) {
    const std::string& authority = request.cert_authorities[i];
    ca_names_items[i].type = siBuffer;
    ca_names_items[i].data =
        reinterpret_cast<unsigned char*>(const_cast<char*>(authority.data()));
    ca_names_items[i].len = static_cast<unsigned int>(authority.size());
  }
  ca_names.nnames = static_cast<int>(ca_names_items.size());
  if (!ca_names_items.empty())
    ca_names.names = &ca_names_items[0];

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
    if (request.cert_authorities.empty() ||
        (!query_nssdb &&
         cert->IsIssuedByEncoded(request.cert_authorities)) ||
        (query_nssdb &&
         NSS_CmpCertChainWCANames(node->cert, &ca_names) == SECSuccess)) {
      selected_certs->push_back(cert);
    }
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

  bool rv = GetClientCertsImpl(client_certs, request, true, selected_certs);
  CERT_DestroyCertList(client_certs);
  return rv;
}

bool ClientCertStoreImpl::SelectClientCertsForTesting(
    const CertificateList& input_certs,
    const SSLCertRequestInfo& request,
    CertificateList* selected_certs) {
  CERTCertList* cert_list = CERT_NewCertList();
  if (!cert_list)
    return false;
  for (size_t i = 0; i < input_certs.size(); ++i) {
    CERT_AddCertToListTail(
        cert_list, CERT_DupCertificate(input_certs[i]->os_cert_handle()));
  }

  bool rv = GetClientCertsImpl(cert_list, request, false, selected_certs);
  CERT_DestroyCertList(cert_list);
  return rv;
}

}  // namespace net
