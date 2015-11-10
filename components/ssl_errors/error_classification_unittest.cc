// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ssl_errors/error_classification.h"

#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SSLErrorClassificationTest : public testing::Test {};

TEST_F(SSLErrorClassificationTest, TestNameMismatch) {
  scoped_refptr<net::X509Certificate> google_cert(
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));
  ASSERT_NE(static_cast<net::X509Certificate*>(NULL), google_cert.get());
  std::vector<std::string> dns_names_google;
  dns_names_google.push_back("www");
  dns_names_google.push_back("google");
  dns_names_google.push_back("com");
  std::vector<std::vector<std::string>> dns_name_tokens_google;
  dns_name_tokens_google.push_back(dns_names_google);
  {
    GURL origin("https://google.com");
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_TRUE(ssl_errors::IsWWWSubDomainMatch(origin, *google_cert));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_FALSE(ssl_errors::IsSubDomainOutsideWildcard(origin, *google_cert));
    EXPECT_FALSE(
        ssl_errors::IsCertLikelyFromMultiTenantHosting(origin, *google_cert));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  {
    GURL origin("https://foo.blah.google.com");
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(ssl_errors::IsWWWSubDomainMatch(origin, *google_cert));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  {
    GURL origin("https://foo.www.google.com");
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(ssl_errors::IsWWWSubDomainMatch(origin, *google_cert));
    EXPECT_TRUE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                              dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  {
    GURL origin("https://www.google.com.foo");
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(ssl_errors::IsWWWSubDomainMatch(origin, *google_cert));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_FALSE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  {
    GURL origin("https://www.foogoogle.com.");
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(ssl_errors::IsWWWSubDomainMatch(origin, *google_cert));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_FALSE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  scoped_refptr<net::X509Certificate> webkit_cert(
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));
  ASSERT_NE(static_cast<net::X509Certificate*>(NULL), webkit_cert.get());
  std::vector<std::string> dns_names_webkit;
  dns_names_webkit.push_back("webkit");
  dns_names_webkit.push_back("org");
  std::vector<std::vector<std::string>> dns_name_tokens_webkit;
  dns_name_tokens_webkit.push_back(dns_names_webkit);
  {
    GURL origin("https://a.b.webkit.org");
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(ssl_errors::IsWWWSubDomainMatch(origin, *webkit_cert));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_webkit));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_webkit,
                                               host_name_tokens));
    EXPECT_TRUE(ssl_errors::IsSubDomainOutsideWildcard(origin, *webkit_cert));
    EXPECT_FALSE(
        ssl_errors::IsCertLikelyFromMultiTenantHosting(origin, *webkit_cert));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *webkit_cert));
  }
}

TEST_F(SSLErrorClassificationTest, TestHostNameHasKnownTLD) {
  EXPECT_TRUE(ssl_errors::IsHostNameKnownTLD("www.google.com"));
  EXPECT_TRUE(ssl_errors::IsHostNameKnownTLD("b.appspot.com"));
  EXPECT_FALSE(ssl_errors::IsHostNameKnownTLD("a.private"));
}

TEST_F(SSLErrorClassificationTest, TestPrivateURL) {
  EXPECT_FALSE(ssl_errors::IsHostnameNonUniqueOrDotless("www.foogoogle.com."));
  EXPECT_TRUE(ssl_errors::IsHostnameNonUniqueOrDotless("go"));
  EXPECT_TRUE(ssl_errors::IsHostnameNonUniqueOrDotless("172.17.108.108"));
  EXPECT_TRUE(ssl_errors::IsHostnameNonUniqueOrDotless("foo.blah"));
}
