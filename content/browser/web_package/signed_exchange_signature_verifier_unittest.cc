// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_signature_verifier.h"

#include "base/callback.h"
#include "content/browser/web_package/signed_exchange_header.h"
#include "content/browser/web_package/signed_exchange_header_parser.h"
#include "net/cert/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(SignedExchangeSignatureVerifier, EncodeCanonicalExchangeHeaders) {
  SignedExchangeHeader header;
  header.set_request_method("GET");
  header.set_request_url(GURL("https://example.com/index.html"));
  header.set_response_code(net::HTTP_OK);
  header.AddResponseHeader("content-type", "text/html; charset=utf-8");
  header.AddResponseHeader("content-encoding", "mi-sha256");

  base::Optional<std::vector<uint8_t>> encoded =
      SignedExchangeSignatureVerifier::EncodeCanonicalExchangeHeaders(header);
  ASSERT_TRUE(encoded.has_value());

  static const uint8_t kExpected[] = {
      // clang-format off
      0x82, // array(2)
        0xa2, // map(2)
          0x44, 0x3a, 0x75, 0x72, 0x6c, // bytes ":url"
          0x58, 0x1e, 0x68, 0x74, 0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x65,
          0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f,
          0x69, 0x6e, 0x64, 0x65, 0x78, 0x2e, 0x68, 0x74, 0x6d, 0x6c,
          // bytes "https://example.com/index.html"

          0x47, 0x3a, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64, // bytes ":method"
          0x43, 0x47, 0x45, 0x54, // bytes "GET"

        0xa3, // map(3)
          0x47, 0x3a, 0x73,0x74, 0x61, 0x74, 0x75, 0x73, // bytes ":status"
          0x43, 0x32, 0x30, 0x30, // bytes "200"

          0x4c, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x74, 0x79,
          0x70, 0x65, // bytes "content-type"
          0x58, 0x18, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x68, 0x74, 0x6d, 0x6c,
          0x3b, 0x20, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65, 0x74, 0x3d, 0x75,
          0x74, 0x66, 0x2d, 0x38, // bytes "text/html; charset=utf-8"

          0x50, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x65, 0x6e,
          0x63, 0x6f, 0x64, 0x69, 0x6e, 0x67, // bytes "content-encoding"
          0x49, 0x6d, 0x69, 0x2d, 0x73, 0x68, 0x61, 0x32, 0x35, 0x36,
          // bytes "mi-sha256"
      // clang-format on
  };
  EXPECT_THAT(*encoded,
              testing::ElementsAreArray(kExpected, arraysize(kExpected)));
}

const uint64_t kSignatureHeaderDate = 1517892341;
const uint64_t kSignatureHeaderExpires = 1517895941;

// clang-format off
constexpr char kSignatureHeader[] =
    "sig; "
    "sig=*RhjjWuXi87riQUu90taBHFJgTo8XBhiCe9qTJMP7/XVPu2diRGipo06HoGsyXkidHiiW"
    "743JgoNmO7CjfeVXLXQgKDxtGidATtPsVadAT4JpBDZJWSUg5qAbWcASXjyO38Uhq9gJkeu4w"
    "1MRMGkvpgVXNjYhi5/9NUer1xEUuJh5UbIDhGrfMihwj+c30nW+qz0n5lCrYonk+Sc0jGcLgc"
    "aDLptqRhOG5S+avwKmbQoqtD0JSc/53L5xXjppyvSA2fRmoDlqVQpX4uzRKq9cny7fZ3qgpZ/"
    "YOCuT7wMj7oVEur175QLe2F8ktKH9arSEiquhFJxBIIIXza8PJnmL5w;"
    "validityUrl=\"https://example.com/resource.validity.msg\"; "
    "integrity=\"mi\"; "
    "certUrl=\"https://example.com/cert.msg\"; "
    "certSha256=*3wfzkF4oKGUwoQ0rE7U11FIdcA/8biGzlaACeRQQH6k; "
    "date=1517892341; expires=1517895941";
// clang-format on

// |expires| (1518497142) is more than 7 days (604800 seconds) after |date|
// (1517892341).
// clang-format off
constexpr char kSignatureHeaderInvalidExpires[] =
    "sig; "
    "sig=*RhjjWuXi87riQUu90taBHFJgTo8XBhiCe9qTJMP7/XVPu2diRGipo06HoGsyXkidHiiW"
    "743JgoNmO7CjfeVXLXQgKDxtGidATtPsVadAT4JpBDZJWSUg5qAbWcASXjyO38Uhq9gJkeu4w"
    "1MRMGkvpgVXNjYhi5/9NUer1xEUuJh5UbIDhGrfMihwj+c30nW+qz0n5lCrYonk+Sc0jGcLgc"
    "aDLptqRhOG5S+avwKmbQoqtD0JSc/53L5xXjppyvSA2fRmoDlqVQpX4uzRKq9cny7fZ3qgpZ/"
    "YOCuT7wMj7oVEur175QLe2F8ktKH9arSEiquhFJxBIIIXza8PJnmL5w;"
    "validityUrl=\"https://example.com/resource.validity.msg\"; "
    "integrity=\"mi\"; "
    "certUrl=\"https://example.com/cert.msg\"; "
    "certSha256=*3wfzkF4oKGUwoQ0rE7U11FIdcA/8biGzlaACeRQQH6k; "
    "date=1517892341; expires=1518497142";
// clang-format on

constexpr char kCertPEM[] = R"(
-----BEGIN CERTIFICATE-----
MIID9zCCAt+gAwIBAgIUde2ndSB4271TAGDk0Ft+WuCCGnMwDQYJKoZIhvcNAQEL
BQAwUDELMAkGA1UEBhMCSlAxEjAQBgNVBAgTCU1pbmF0by1rdTEOMAwGA1UEBxMF
VG9reW8xEDAOBgNVBAoTB1Rlc3QgQ0ExCzAJBgNVBAsTAkNBMB4XDTE4MDIwNTA0
NDUwMFoXDTE5MDIwNTA0NDUwMFowbzELMAkGA1UEBhMCSlAxEjAQBgNVBAgTCU1p
bmF0by1rdTEOMAwGA1UEBxMFVG9reW8xFDASBgNVBAoTC2V4YW1wbGUuY29tMRIw
EAYDVQQLEwl3ZWJzZXJ2ZXIxEjAQBgNVBAMTCWxvY2FsaG9zdDCCASIwDQYJKoZI
hvcNAQEBBQADggEPADCCAQoCggEBAJv0UV5pK/XMtmHuDHUSvU+mNghsDQsKYSeB
r/CySBIbLWtkeC7oxuYT2R+Mz4vVs0WQ1f3F/e3HIIQxWmy5VYErER13c53yeCNF
fcBkwpuCZKEO1BURX+WgjYPnzLYX1xDnpBM++TuEZKdxzUVjs/jQjMNB8sbRYzng
IIbA4HiRUtPvnGjLmY0HxZyskb52yeTWg40jWPdLaC8GMEZXGKynAnGEMl3c/dVw
8+nKS1VVe6k32Ubfl1NlaqbOXi0xHHMUhLY/l8Lu49E0ivPS7BWL/0nMR9EAmu+I
AgK9OD7VRoMA0LEKBzIQUEuK70JxLkV7GNvrtnOX83+EwwdfBdUCAwEAAaOBqTCB
pjAOBgNVHQ8BAf8EBAMCBaAwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMC
MAwGA1UdEwEB/wQCMAAwHQYDVR0OBBYEFDzp/0BrXKIfDGe3KfJLyBH8azW2MB8G
A1UdIwQYMBaAFNAPhK4UBktJcx6TIk5QKwZPit4ZMCcGA1UdEQQgMB6CC2V4YW1w
bGUuY29tgg93d3cuZXhhbXBsZS5jb20wDQYJKoZIhvcNAQELBQADggEBAKSQbsOW
IX2JDv+Vg1lvbOFx+JqwdhvYTkOF4Z9YbRtlqEIZbc8KWjAOzDB1xVJxhSjD+f8+
vrj7YN/ggCQk6DzuF4lkztBDO95Fxmx4EeIKdKt83WP09Os/2yIOKToOnHkmauBB
ijY8oxNs+XxKrPX7DN5QQQhiTsZcL625fcnIwPb0DeeuCT7bJYPv8OojMDTR1uDt
SQ53HYt0aLun+Br3lCwW8cnpxuezJhg0gNezYp/8gC4kByqoT26atpls08eWUdFD
U0/55zFz2OzNoAHaoBzMRxn4ZSc3+lxl0K1+cCP8ivhwkxdz/vhz5RfOjpSinxqt
wYxI2+BLS6X5NpI=
-----END CERTIFICATE-----)";

TEST(SignedExchangeSignatureVerifier, Verify) {
  base::Time verification_time =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromSeconds(kSignatureHeaderDate);
  auto signature = SignedExchangeHeaderParser::ParseSignature(
      kSignatureHeader, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(1u, signature->size());

  net::CertificateList certlist =
      net::X509Certificate::CreateCertificateListFromBytes(
          kCertPEM, arraysize(kCertPEM), net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, certlist.size());

  SignedExchangeHeader header;
  header.set_request_method("GET");
  header.set_request_url(GURL("https://example.com/index.html"));
  header.set_response_code(net::HTTP_OK);
  header.AddResponseHeader("content-type", "text/html; charset=utf-8");
  header.AddResponseHeader("content-encoding", "mi-sha256");
  header.AddResponseHeader(
      "mi", "mi-sha256=4ld4G-h-sQSoLBD39ndIO15O_82NXSzq9UMFEYI02JQ");
  header.SetSignatureForTesting((*signature)[0]);

  auto certificate = certlist[0];

  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kSuccess,
            SignedExchangeSignatureVerifier::Verify(
                header, certificate, verification_time,
                nullptr /* devtools_proxy */));

  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp,
            SignedExchangeSignatureVerifier::Verify(
                header, certificate,
                base::Time::UnixEpoch() +
                    base::TimeDelta::FromSeconds(kSignatureHeaderDate - 1),
                nullptr /* devtools_proxy */));

  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kSuccess,
            SignedExchangeSignatureVerifier::Verify(
                header, certificate,
                base::Time::UnixEpoch() +
                    base::TimeDelta::FromSeconds(kSignatureHeaderExpires),
                nullptr /* devtools_proxy */));

  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp,
            SignedExchangeSignatureVerifier::Verify(
                header, certificate,
                base::Time::UnixEpoch() +
                    base::TimeDelta::FromSeconds(kSignatureHeaderExpires + 1),
                nullptr /* devtools_proxy */));

  SignedExchangeHeader invalid_expires_header(header);
  auto invalid_expires_signature = SignedExchangeHeaderParser::ParseSignature(
      kSignatureHeaderInvalidExpires, nullptr /* devtools_proxy */);
  ASSERT_TRUE(invalid_expires_signature.has_value());
  ASSERT_EQ(1u, invalid_expires_signature->size());
  invalid_expires_header.SetSignatureForTesting(
      (*invalid_expires_signature)[0]);
  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp,
            SignedExchangeSignatureVerifier::Verify(
                invalid_expires_header, certificate, verification_time,
                nullptr /* devtools_proxy */));

  SignedExchangeHeader corrupted_header(header);
  corrupted_header.set_request_url(GURL("https://example.com/bad.html"));
  EXPECT_EQ(
      SignedExchangeSignatureVerifier::Result::kErrSignatureVerificationFailed,
      SignedExchangeSignatureVerifier::Verify(corrupted_header, certificate,
                                              verification_time,
                                              nullptr /* devtools_proxy */));

  SignedExchangeHeader badsig_header(header);
  SignedExchangeHeaderParser::Signature badsig = header.signature();
  badsig.sig[0]++;
  badsig_header.SetSignatureForTesting(badsig);
  EXPECT_EQ(
      SignedExchangeSignatureVerifier::Result::kErrSignatureVerificationFailed,
      SignedExchangeSignatureVerifier::Verify(badsig_header, certificate,
                                              verification_time,
                                              nullptr /* devtools_proxy */));

  SignedExchangeHeader badsigsha256_header(header);
  SignedExchangeHeaderParser::Signature badsigsha256 = header.signature();
  badsigsha256.cert_sha256->data[0]++;
  badsigsha256_header.SetSignatureForTesting(badsigsha256);
  EXPECT_EQ(
      SignedExchangeSignatureVerifier::Result::kErrCertificateSHA256Mismatch,
      SignedExchangeSignatureVerifier::Verify(badsigsha256_header, certificate,
                                              verification_time,
                                              nullptr /* devtools_proxy */));
}

}  // namespace
}  // namespace content
