// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_assistant.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ssl/ssl_error_assistant.pb.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "crypto/sha2.h"
#include "net/cert/asn1_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const net::SHA256HashValue kCertPublicKeyHashValue = {{0x01, 0x02}};

const uint32_t kLargeVersionId = 0xFFFFFFu;

// These certificates are self signed certificates with relevant issuer common
// names generated using the following openssl command:
//  openssl req -new -x509 -keyout server.pem -out server.pem -days 365 -nodes

// Common name: "Misconfig Software"
// Organization name: "Test Company"
const char kMisconfigSoftwareCert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIC5DCCAk2gAwIBAgIJAPYPMpr0AIDBMA0GCSqGSIb3DQEBBQUAMFYxCzAJBgNV\n"
    "BAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMRUwEwYDVQQKEwxUZXN0IENvbXBh\n"
    "bnkxGzAZBgNVBAMTEk1pc2NvbmZpZyBTb2Z0d2FyZTAeFw0xNzEwMTcxOTQyMzFa\n"
    "Fw0xODEwMTcxOTQyMzFaMFYxCzAJBgNVBAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0\n"
    "YXRlMRUwEwYDVQQKEwxUZXN0IENvbXBhbnkxGzAZBgNVBAMTEk1pc2NvbmZpZyBT\n"
    "b2Z0d2FyZTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAyZWnLvuD19iG5PSi\n"
    "8dSVhLeuZDBtwcWBOMzga3hx7HDMd+395gstRLc1VhpMePmxUdyEpStHDiYjNF/k\n"
    "GRsIXfXWpO82L7r+Fm6eym4BOw2sjX1aounljETYasREvXhEB/8WaLJfMcstUwsT\n"
    "PoXgUWYkIBi/76EiWHXvYEiXV2kCAwEAAaOBuTCBtjAdBgNVHQ4EFgQUtakrb0wU\n"
    "gZVXyus1vlj6T5aDEnYwgYYGA1UdIwR/MH2AFLWpK29MFIGVV8rrNb5Y+k+WgxJ2\n"
    "oVqkWDBWMQswCQYDVQQGEwJBVTETMBEGA1UECBMKU29tZS1TdGF0ZTEVMBMGA1UE\n"
    "ChMMVGVzdCBDb21wYW55MRswGQYDVQQDExJNaXNjb25maWcgU29mdHdhcmWCCQD2\n"
    "DzKa9ACAwTAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUAA4GBAFAFlPO3HEWQ\n"
    "0XdRbeIQPVva72VFyF+ESFC6ky7GLDoaSAwRlE1i5qWfxnLbEA0b7CWjyO1tC8Uw\n"
    "OMB5U9qmQouAqf5medr2pECSDimb7qBCz3kKjgZWt1Xv8w0PsW6lFVPmMsO4Zv7F\n"
    "Podf1biETWgaYoT6PrUTtWG3jeSU2r9M\n"
    "-----END CERTIFICATE-----";

// Common name: "ijklmn opqrs"
// Organization name: "abc defgh co"
const char kMisconfigSoftwareRegexCheckCert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIC0jCCAjugAwIBAgIJAOyyORCXGxvDMA0GCSqGSIb3DQEBBQUAMFAxCzAJBgNV\n"
    "BAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMRUwEwYDVQQKEwxhYmMgZGVmZ2gg\n"
    "Y28xFTATBgNVBAMTDGlqa2xtbiBvcHFyczAeFw0xNzEwMTcyMjM4MzJaFw0xODEw\n"
    "MTcyMjM4MzJaMFAxCzAJBgNVBAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMRUw\n"
    "EwYDVQQKEwxhYmMgZGVmZ2ggY28xFTATBgNVBAMTDGlqa2xtbiBvcHFyczCBnzAN\n"
    "BgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAsnuBPW2k4+eFazC8lq7rLRNjpZ5yqEwX\n"
    "LBE8fxbvjXSSZAaJz/iTn+Zg/UMJz9IpulbcA/xf36JuhFYv7aClFrtg5DHaqrPf\n"
    "kt7g9AM3hEIjGsdHtyAqFp/+CpySGzVpTLyT1NtHkqtkiD6HCSpWqL+m/6ibpUhy\n"
    "oy9y/ZV1vVUCAwEAAaOBszCBsDAdBgNVHQ4EFgQUBk+vtSjNipTcWh3NIbtsjVN0\n"
    "uJswgYAGA1UdIwR5MHeAFAZPr7UozYqU3FodzSG7bI1TdLiboVSkUjBQMQswCQYD\n"
    "VQQGEwJBVTETMBEGA1UECBMKU29tZS1TdGF0ZTEVMBMGA1UEChMMYWJjIGRlZmdo\n"
    "IGNvMRUwEwYDVQQDEwxpamtsbW4gb3BxcnOCCQDssjkQlxsbwzAMBgNVHRMEBTAD\n"
    "AQH/MA0GCSqGSIb3DQEBBQUAA4GBACv8KnNmaOqHD8QsmvaD2Yvc7dAFkCgsdQb/\n"
    "Tkyw0sJN8ZH+bummkgGZLw4gzdmhVg8kGIbiDvCYgOVaIg+2H3PtkdIrW2KhyXrN\n"
    "2qIa9nBvuv8LC1TAdB65DDheLh0PuTGcIwfJ7kcKi+Eo8fPbYYdyHGRw+rVWXVPz\n"
    "SgZO4ZYq\n"
    "-----END CERTIFICATE-----";

}  // namespace

class SSLErrorAssistantTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    error_assistant_.reset(new SSLErrorAssistant());

    ssl_info_.cert = net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "subjectAltName_www_example_com.pem");
    ssl_info_.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    ssl_info_.public_key_hashes.push_back(
        net::HashValue(kCertPublicKeyHashValue));
  }

  void TearDown() override {
    error_assistant_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void TestMITMSoftwareMatchFromString(const std::string& cert,
                                       const std::string& match_result) {
    net::CertificateList certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            cert.data(), cert.size(), net::X509Certificate::FORMAT_AUTO);
    ASSERT_FALSE(certs.empty());
    EXPECT_EQ(match_result,
              error_assistant()->MatchKnownMITMSoftware(certs[0]));
  }

 protected:
  SSLErrorAssistantTest() {
    embedded_test_server_ = base::MakeUnique<net::EmbeddedTestServer>();
  }

  ~SSLErrorAssistantTest() override {}

  SSLErrorAssistant* error_assistant() const { return error_assistant_.get(); }

  net::EmbeddedTestServer* embedded_test_server() const {
    return embedded_test_server_.get();
  }

  const net::SSLInfo& ssl_info() { return ssl_info_; }

 private:
  net::SSLInfo ssl_info_;
  std::unique_ptr<SSLErrorAssistant> error_assistant_;
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorAssistantTest);
};

// Test to see if IsKnownCaptivePortalCertificate() returns the correct value.
TEST_F(SSLErrorAssistantTest, CaptivePortalCertificateList) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  // Test without the known captive portal certificate in config_proto.
  auto config_proto =
      base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);
  config_proto->add_captive_portal_cert()->set_sha256_hash("sha256/boxfish");
  config_proto->add_captive_portal_cert()->set_sha256_hash(
      "sha256/treecreeper");
  error_assistant()->SetErrorAssistantProto(std::move(config_proto));
  EXPECT_FALSE(error_assistant()->IsKnownCaptivePortalCertificate(ssl_info()));

  error_assistant()->ResetForTesting();

  // Test with the known captive portal certificate in config_proto.
  config_proto.reset(new chrome_browser_ssl::SSLErrorAssistantConfig());
  config_proto->set_version_id(kLargeVersionId);

  config_proto->add_captive_portal_cert()->set_sha256_hash("sha256/boxfish");
  config_proto->add_captive_portal_cert()->set_sha256_hash(
      ssl_info().public_key_hashes[0].ToString());
  config_proto->add_captive_portal_cert()->set_sha256_hash(
      "sha256/treecreeper");
  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  EXPECT_TRUE(error_assistant()->IsKnownCaptivePortalCertificate(ssl_info()));
}

// Test to see if the MitM Software gets matched correctly.
TEST_F(SSLErrorAssistantTest, MitMSoftwareMatching) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto config_proto =
      base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Tests for a basic and more complex regex match.
  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Basic Check");
  filter->set_issuer_common_name_regex("Misconfig Software");
  filter->set_issuer_organization_regex("Test Company");

  filter = config_proto->add_mitm_software();
  filter->set_name("Regex Check");
  filter->set_issuer_common_name_regex("ij[a-z]+n opqrs");
  filter->set_issuer_organization_regex("abc de[a-z0-9]gh [a-z]+");
  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  TestMITMSoftwareMatchFromString(kMisconfigSoftwareCert, "Basic Check");
  TestMITMSoftwareMatchFromString(kMisconfigSoftwareRegexCheckCert,
                                  "Regex Check");

  error_assistant()->ResetForTesting();

  // Tests for no matches.
  config_proto.reset(new chrome_browser_ssl::SSLErrorAssistantConfig());
  config_proto->set_version_id(kLargeVersionId);

  filter = config_proto->add_mitm_software();
  filter->set_name("Incorrect common name");
  filter->set_issuer_common_name_regex("Misconfig Sotware");
  filter->set_issuer_organization_regex("Test Company");

  filter = config_proto->add_mitm_software();
  filter->set_name("Incorrect company name");
  filter->set_issuer_common_name_regex("Misconfig Software");
  filter->set_issuer_organization_regex("Tst Company");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  TestMITMSoftwareMatchFromString(kMisconfigSoftwareCert, "");
}
