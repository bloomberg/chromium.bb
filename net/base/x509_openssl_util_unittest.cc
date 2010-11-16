// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_openssl_util.h"

#include <algorithm>

#include "base/string_split.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace x509_openssl_util {

namespace {

struct CertificateNameVerifyTestData {
  // true iff we expect hostname to match an entry in cert_names.
  const bool expected;
  // The hostname to match.
  const char* const hostname;
  // '/' separated list of certificate names to match against. Any occurrence
  // of '#' will be replaced with a null character before processing.
  const char* const cert_names;
};

CertificateNameVerifyTestData kNameVerifyTestData[] = {
    { true, "foo.com", "foo.com" },
    { true, "foo.com", "foo.com." },
    { true, "f", "f" },
    { true, "f", "f." },
    { true, "bar.foo.com", "*.foo.com" },
    { true, "www-3.bar.foo.com", "*.bar.foo.com." },
    { true, "www.test.fr", "*.test.com/*.test.co.uk/*.test.de/*.test.fr" },
    { true, "wwW.tESt.fr", "//*.*/*.test.de/*.test.FR/www" },
    { false, "foo.com", "*.com" },
    { false, "f.uk", ".uk" },
    { true,  "h.co.uk", "*.co.uk" },
    { false, "192.168.1.11", "*.168.1.11" },
    { false, "foo.us", "*.us" },
    { false, "www.bar.foo.com",
      "*.foo.com/*.*.foo.com/*.*.bar.foo.com/*w*.bar.foo.com/*..bar.foo.com" },
    { false, "w.bar.foo.com", "?.bar.foo.com" },
    { false, "www.foo.com", "(www|ftp).foo.com" },
    { false, "www.foo.com", "www.foo.com#*.foo.com/#" },  // # = null char.
    { false, "foo", "*" },
    { false, "foo.", "*." },
    { false, "test.org", "www.test.org/*.test.org/*.org" },
    { false, "1.2.3.4.5.6", "*.2.3.4.5.6" },
    // IDN tests
    { true, "xn--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br" },
    { true, "www.xn--poema-9qae5a.com.br", "*.xn--poema-9qae5a.com.br" },
    { false, "xn--poema-9qae5a.com.br", "*.xn--poema-9qae5a.com.br" },
    // The following are adapted from the examples in http://tools.ietf.org/html/draft-saintandre-tls-server-id-check-09#section-4.4.3
    { true, "foo.example.com", "*.example.com" },
    { false, "bar.foo.example.com", "*.example.com" },
    { false, "example.com", "*.example.com" },
    { false, "baz1.example.net", "baz*.example.net" },
    { false, "baz2.example.net", "baz*.example.net" },
    { false, "bar.*.example.net", "bar.*.example.net" },
    { false, "bar.f*o.example.net", "bar.f*o.example.net" },
    // IP addresses currently not supported.
    { false, "192.168.1.1", "192.168.1.1" },
    { false, "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210",
      "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210" },
    { false, "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", "*.]" },
    { false, "::192.9.5.5", "::192.9.5.5" },
    { false, "::192.9.5.5", "*.9.5.5" },
    { false, "2010:836B:4179::836B:4179", "*:836B:4179::836B:4179" },
    // Invalid host names.
    { false, "www%26.foo.com", "www%26.foo.com" },
    { false, "www.*.com", "www.*.com" },
    { false, "w$w.f.com", "w$w.f.com" },
    { false, "www-1.[::FFFF:129.144.52.38]", "*.[::FFFF:129.144.52.38]" },
};

class X509CertificateNameVerifyTest
    : public testing::TestWithParam<CertificateNameVerifyTestData> {
};

TEST_P(X509CertificateNameVerifyTest, VerifyHostname) {
  CertificateNameVerifyTestData test_data(GetParam());

  std::string cert_name_line(test_data.cert_names);
  std::replace(cert_name_line.begin(), cert_name_line.end(), '#', '\0');
  std::vector<std::string> cert_names;
  base::SplitString(cert_name_line, '/', &cert_names);

  EXPECT_EQ(test_data.expected, VerifyHostname(test_data.hostname, cert_names))
      << "Host [" << test_data.hostname
      << "], cert name [" << test_data.cert_names << "]";
}

INSTANTIATE_TEST_CASE_P(, X509CertificateNameVerifyTest,
                        testing::ValuesIn(kNameVerifyTestData));

}  // namespace

}  // namespace x509_openssl_util

}  // namespace net
