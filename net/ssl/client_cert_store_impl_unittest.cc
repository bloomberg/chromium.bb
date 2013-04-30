// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_impl.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/test_data_directory.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// "CN=B CA" - DER encoded DN of the issuer of client_1.pem
const unsigned char kAuthority1DN[] = {
  0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
  0x04, 0x42, 0x20, 0x43, 0x41
};

// "CN=E CA" - DER encoded DN of the issuer of client_2.pem
unsigned char kAuthority2DN[] = {
  0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
  0x04, 0x45, 0x20, 0x43, 0x41
};

}  // namespace

class ClientCertStoreImplTest : public ::testing::Test {
 protected:
  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         CertificateList* selected_certs) {
    return store_.SelectClientCertsForTesting(
        input_certs, cert_request_info, selected_certs);
  }

#if defined(OS_MACOSX) && !defined(OS_IOS)
  bool SelectClientCertsGivenPreferred(
      const scoped_refptr<X509Certificate>& preferred_cert,
      const CertificateList& regular_certs,
      const SSLCertRequestInfo& request,
      CertificateList* selected_certs) {
    return store_.SelectClientCertsGivenPreferredForTesting(
        preferred_cert, regular_certs, request, selected_certs);
  }
#endif

 private:
  ClientCertStoreImpl store_;
};

TEST_F(ClientCertStoreImplTest, EmptyQuery) {
  std::vector<scoped_refptr<X509Certificate> > certs;
  scoped_refptr<SSLCertRequestInfo> request(new SSLCertRequestInfo());

  std::vector<scoped_refptr<X509Certificate> > selected_certs;
  bool rv = SelectClientCerts(certs, *request, &selected_certs);
  EXPECT_TRUE(rv);
  EXPECT_EQ(0u, selected_certs.size());
}

// Verify that CertRequestInfo with empty |cert_authorities| matches all
// issuers, rather than no issuers.
TEST_F(ClientCertStoreImplTest, AllIssuersAllowed) {
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert);

  std::vector<scoped_refptr<X509Certificate> > certs;
  certs.push_back(cert);
  scoped_refptr<SSLCertRequestInfo> request(new SSLCertRequestInfo());

  std::vector<scoped_refptr<X509Certificate> > selected_certs;
  bool rv = SelectClientCerts(certs, *request, &selected_certs);
  EXPECT_TRUE(rv);
  ASSERT_EQ(1u, selected_certs.size());
  EXPECT_TRUE(selected_certs[0]->Equals(cert));
}

// Verify that certificates are correctly filtered against CertRequestInfo with
// |cert_authorities| containing only |authority_1_DN|.
TEST_F(ClientCertStoreImplTest, CertAuthorityFiltering) {
  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1);
  scoped_refptr<X509Certificate> cert_2(
      ImportCertFromFile(GetTestCertsDirectory(), "client_2.pem"));
  ASSERT_TRUE(cert_2);

  std::vector<std::string> authority_1(
      1, std::string(reinterpret_cast<const char*>(kAuthority1DN),
                     sizeof(kAuthority1DN)));
  std::vector<std::string> authority_2(
      1, std::string(reinterpret_cast<const char*>(kAuthority2DN),
                     sizeof(kAuthority2DN)));
  EXPECT_TRUE(cert_1->IsIssuedByEncoded(authority_1));
  EXPECT_FALSE(cert_1->IsIssuedByEncoded(authority_2));
  EXPECT_TRUE(cert_2->IsIssuedByEncoded(authority_2));
  EXPECT_FALSE(cert_2->IsIssuedByEncoded(authority_1));

  std::vector<scoped_refptr<X509Certificate> > certs;
  certs.push_back(cert_1);
  certs.push_back(cert_2);
  scoped_refptr<SSLCertRequestInfo> request(new SSLCertRequestInfo());
  request->cert_authorities = authority_1;

  std::vector<scoped_refptr<X509Certificate> > selected_certs;
  bool rv = SelectClientCerts(certs, *request, &selected_certs);
  EXPECT_TRUE(rv);
  ASSERT_EQ(1u, selected_certs.size());
  EXPECT_TRUE(selected_certs[0]->Equals(cert_1));
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Verify that the preferred cert gets filtered out when it doesn't match the
// server criteria.
TEST_F(ClientCertStoreImplTest, FilterOutThePreferredCert) {
  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1);

  std::vector<std::string> authority_2(
      1, std::string(reinterpret_cast<const char*>(kAuthority2DN),
                     sizeof(kAuthority2DN)));
  EXPECT_FALSE(cert_1->IsIssuedByEncoded(authority_2));

  std::vector<scoped_refptr<X509Certificate> > certs;
  scoped_refptr<SSLCertRequestInfo> request(new SSLCertRequestInfo());
  request->cert_authorities = authority_2;

  std::vector<scoped_refptr<X509Certificate> > selected_certs;
  bool rv =
      SelectClientCertsGivenPreferred(cert_1, certs, *request, &selected_certs);
  EXPECT_TRUE(rv);
  EXPECT_EQ(0u, selected_certs.size());
}

// Verify that the preferred cert takes the first position in the output list,
// when it does not get filtered out.
TEST_F(ClientCertStoreImplTest, PreferredCertGoesFirst) {
  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1);
  scoped_refptr<X509Certificate> cert_2(
      ImportCertFromFile(GetTestCertsDirectory(), "client_2.pem"));
  ASSERT_TRUE(cert_2);

  std::vector<scoped_refptr<X509Certificate> > certs;
  certs.push_back(cert_2);
  scoped_refptr<SSLCertRequestInfo> request(new SSLCertRequestInfo());

  std::vector<scoped_refptr<X509Certificate> > selected_certs;
  bool rv =
      SelectClientCertsGivenPreferred(cert_1, certs, *request, &selected_certs);
  EXPECT_TRUE(rv);
  ASSERT_EQ(2u, selected_certs.size());
  EXPECT_TRUE(selected_certs[0]->Equals(cert_1));
  EXPECT_TRUE(selected_certs[1]->Equals(cert_2));
}
#endif

}  // namespace net
