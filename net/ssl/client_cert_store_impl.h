// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_IMPL_H_
#define NET_SSL_CLIENT_CERT_STORE_IMPL_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "net/base/net_export.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

class NET_EXPORT ClientCertStoreImpl : public ClientCertStore {
 public:
  ClientCertStoreImpl() {}

  virtual ~ClientCertStoreImpl() {}

  // ClientCertStore:
  virtual bool GetClientCerts(const SSLCertRequestInfo& cert_request_info,
                              CertificateList* selected_certs) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ClientCertStoreImplTest, EmptyQuery);
  FRIEND_TEST_ALL_PREFIXES(ClientCertStoreImplTest, AllIssuersAllowed);
  FRIEND_TEST_ALL_PREFIXES(ClientCertStoreImplTest, CertAuthorityFiltering);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  FRIEND_TEST_ALL_PREFIXES(ClientCertStoreImplTest, FilterOutThePreferredCert);
  FRIEND_TEST_ALL_PREFIXES(ClientCertStoreImplTest, PreferredCertGoesFirst);
#endif

  // A hook for testing. Filters |input_certs| using the logic being used to
  // filter the system store when GetClientCerts() is called. Depending on the
  // implementation, this might be:
  // - Implemented by creating a temporary in-memory store and filtering it
  // using the common logic (preferable, currently on Windows).
  // - Implemented by creating a list of certificates that otherwise would be
  // extracted from the system store and filtering it using the common logic
  // (less adequate, currently on NSS and Mac).
  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         CertificateList* selected_certs);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Testing hook specific to Mac, where the internal logic recognizes preferred
  // certificates for particular domains. If the preferred certificate is
  // present in the output list (i.e. it doesn't get filtered out), it should
  // always come first.
  bool SelectClientCertsGivenPreferred(
      const scoped_refptr<X509Certificate>& preferred_cert,
      const CertificateList& regular_certs,
      const SSLCertRequestInfo& request,
      CertificateList* selected_certs);
#endif

  DISALLOW_COPY_AND_ASSIGN(ClientCertStoreImpl);
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_IMPL_H_
