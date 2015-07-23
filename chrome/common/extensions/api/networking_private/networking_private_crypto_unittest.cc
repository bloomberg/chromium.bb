// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/cast/cast_cert_validator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

namespace cast_crypto = ::extensions::api::cast_crypto;

}  // namespace

// Tests of networking_private_crypto support for Networking Private API.
class NetworkingPrivateCryptoTest : public testing::Test {
 protected:
  // Verify that decryption of |encrypted| data using |private_key_pem| matches
  // |plain| data.
  bool VerifyByteString(const std::string& private_key_pem,
                        const std::string& plain,
                        const std::vector<uint8>& encrypted) {
    std::string decrypted;
    if (networking_private_crypto::DecryptByteString(
            private_key_pem, encrypted, &decrypted))
      return decrypted == plain;
    return false;
  }
};

// Test that networking_private_crypto::VerifyCredentials behaves as expected.
TEST_F(NetworkingPrivateCryptoTest, VerifyCredentials) {
  std::string keys =
      "CrMCCiBSnZzWf+XraY5w3SbX2PEmWfHm5SNIv2pc9xbhP0EOcxKOAjCCAQoCggEBALwigL2A"
      "9johADuudl41fz3DZFxVlIY0LwWHKM33aYwXs1CnuIL638dDLdZ+q6BvtxNygKRHFcEgmVDN"
      "7BRiCVukmM3SQbY2Tv/oLjIwSoGoQqNsmzNuyrL1U2bgJ1OGGoUepzk/SneO+1RmZvtYVMBe"
      "Ocf1UAYL4IrUzuFqVR+LFwDmaaMn5gglaTwSnY0FLNYuojHetFJQ1iBJ3nGg+a0gQBLx3SXr"
      "1ea4NvTWj3/KQ9zXEFvmP1GKhbPz//YDLcsjT5ytGOeTBYysUpr3TOmZer5ufk0K48YcqZP6"
      "OqWRXRy9ZuvMYNyGdMrP+JIcmH1X+mFHnquAt+RIgCqSxRsCAwEAAQqzAgogmNZt6BxWR4RN"
      "lkNNN8SNws5/CHJQGee26JJ/VtaBqhgSjgIwggEKAoIBAQC8IoC9gPY6IQA7rnZeNX89w2Rc"
      "VZSGNC8FhyjN92mMF7NQp7iC+t/HQy3Wfqugb7cTcoCkRxXBIJlQzewUYglbpJjN0kG2Nk7/"
      "6C4yMEqBqEKjbJszbsqy9VNm4CdThhqFHqc5P0p3jvtUZmb7WFTAXjnH9VAGC+CK1M7halUf"
      "ixcA5mmjJ+YIJWk8Ep2NBSzWLqIx3rRSUNYgSd5xoPmtIEAS8d0l69XmuDb01o9/ykPc1xBb"
      "5j9RioWz8//2Ay3LI0+crRjnkwWMrFKa90zpmXq+bn5NCuPGHKmT+jqlkV0cvWbrzGDchnTK"
      "z/iSHJh9V/phR56rgLfkSIAqksUbAgMBAAE=";
  std::string signature =
      "eHMoa7dP2ByNtDnxM/Q6yV3ZyUyihBFgOthq937yuiu2uwW2X/i8h1YrJFaWrA0iTTfSLAa6"
      "PBAN1hhnwXlWYy8MvViJ9eJqf5FfCCkOjdRN0QIFPpmIJm/EcIv91bNMWnOGANgSW1Hons+s"
      "C0/kROPbPABPLLwfgGizBDSZNapxgj8G+iDvi1JRRvvNdmjUs2AUIPNrSp3Knt3FyZ5F2Smk"
      "Khpo7XVTWgSuWOzUJu6zNHn2krm64Ymd2HxRDyKTm1DBzy1MoXv4/8mbLYdj+KAvhqKJfRcr"
      "GkUXVK++wCHERwxcvfk7e6lN6adcCVYP9pZPMhE/UyAJY6/uE1X0cw==";
  EXPECT_TRUE(
      cast_crypto::SetTrustedCertificateAuthoritiesForTest(keys, signature));

  static const char kCertData[] =
      "-----BEGIN CERTIFICATE-----"
      "MIIDhzCCAm8CBFE2SCMwDQYJKoZIhvcNAQEFBQAwfTELMAkGA1UEBhMCVVMxEzARBgNVBAgM"
      "CkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEzARBgNVBAoMCkdvb2dsZSBJ"
      "bmMxEjAQBgNVBAsMCUdvb2dsZSBUVjEYMBYGA1UEAwwPRXVyZWthIEdlbjEgSUNBMB4XDTEz"
      "MDMwNTE5MzE0N1oXDTMzMDIyODE5MzE0N1owgYMxFjAUBgNVBAcTDU1vdW50YWluIFZpZXcx"
      "EjAQBgNVBAsTCUdvb2dsZSBUVjETMBEGA1UEChMKR29vZ2xlIEluYzETMBEGA1UECBMKQ2Fs"
      "aWZvcm5pYTELMAkGA1UEBhMCVVMxHjAcBgNVBAMUFWV2dF9lMTYxIDAwMWExMWZmYWNkZjCC"
      "ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAPHGDV0lLoTYK78q13y/2u77YTjgbBlW"
      "AOxgrSNcMmGHx1K0aPyo50p99dGQnjapW6jtGrMzReWV2Wz3VL8rYlqY7oWjeJwsLQwo2tcn"
      "7vIZ/PuvPz9xgnGMUbBOfhCf3Epb1N4Jz82pxxrOFhUawWAglC9C4fUeZLCZpOJsQd4QeAzn"
      "kydl3xbqdSm74kwxE6vkGEzSCDnC7aYx0Rvvr1mZOKdl4AinYrxzWgmVsTnaFT1soSjmC5e/"
      "i6Jcrs4dDFgY6mKy9Qtly2XPSCYljm6L4SgqgJNmlpY0qYJgO++BdofIbU2jsOiCMvIuKkbM"
      "n72NsPQG0QhnVMwk7kYg6kkCAwEAAaMNMAswCQYDVR0TBAIwADANBgkqhkiG9w0BAQUFAAOC"
      "AQEAW0bQl9yjBc7DgMp94i7ZDOUxKQrzthephuwzb3/wWiTHcw6KK6FRPefXn6NPWxKKeQmv"
      "/tBxHbVlmYRXUbrhksnD0aUki4InvtL2m0H1fPfMxmJRFE+HoSXu+s0sGON831JaMcYRbAku"
      "5uHnltaGNzOI0KPHFGoCDmjAZD+IuoR2LR4FuuTrECK7KLjkdf//z5d5j7nBDPZS7uTCwC/B"
      "wM9asRj3tJA5VRFbLbsit1VI7IaRCk9rsSKkpBUaVeKbPLz+y/Z6JonXXT6AxsfgUSKDd4B7"
      "MYLrTwMQfGuUaaaKko6ldKIrovjrcPloQr1Hxb2bipFcjLmG7nxQLoS6vQ=="
      "-----END CERTIFICATE-----";
  static const char kICAData[] =
      "-----BEGIN CERTIFICATE-----"
      "MIIDzTCCArWgAwIBAgIBAzANBgkqhkiG9w0BAQUFADB1MQswCQYDVQQGEwJVUzET"
      "MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzETMBEG"
      "A1UECgwKR29vZ2xlIEluYzENMAsGA1UECwwEQ2FzdDEVMBMGA1UEAwwMQ2FzdCBS"
      "b290IENBMB4XDTE0MDQwMjIwNTg1NFoXDTE5MDQwMjIwNTg1NFowfTELMAkGA1UE"
      "BhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZp"
      "ZXcxEzARBgNVBAoMCkdvb2dsZSBJbmMxEjAQBgNVBAsMCUdvb2dsZSBUVjEYMBYG"
      "A1UEAwwPRXVyZWthIEdlbjEgSUNBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB"
      "CgKCAQEAvCKAvYD2OiEAO652XjV/PcNkXFWUhjQvBYcozfdpjBezUKe4gvrfx0Mt"
      "1n6roG+3E3KApEcVwSCZUM3sFGIJW6SYzdJBtjZO/+guMjBKgahCo2ybM27KsvVT"
      "ZuAnU4YahR6nOT9Kd477VGZm+1hUwF45x/VQBgvgitTO4WpVH4sXAOZpoyfmCCVp"
      "PBKdjQUs1i6iMd60UlDWIEnecaD5rSBAEvHdJevV5rg29NaPf8pD3NcQW+Y/UYqF"
      "s/P/9gMtyyNPnK0Y55MFjKxSmvdM6Zl6vm5+TQrjxhypk/o6pZFdHL1m68xg3IZ0"
      "ys/4khyYfVf6YUeeq4C35EiAKpLFGwIDAQABo2AwXjAPBgNVHRMECDAGAQH/AgEA"
      "MB0GA1UdDgQWBBQyr35sod0oQuWz4VmnWjnJ/4pinzAfBgNVHSMEGDAWgBR8mh59"
      "33lUvNfMXsqZhkV5ZXQoGTALBgNVHQ8EBAMCAQYwDQYJKoZIhvcNAQEFBQADggEB"
      "ABPENY9iGt6qsc5yq4JOO6EEqYbKVtkSf1AqW2yJc4M4EZ65eA6bpj9EVIKvDxYq"
      "NI7q40f7jCXiS+Y73OXFaC3Xue8+DV7WVjAvf9QYy79ohnbqadA4U/Sb7vw4AzwT"
      "KCMlH2fUJ5PCNFfTj6lAkeZOhxtegnEMTIB8zvXEb42H0hN4UxRRhCeKS9tIlAmI"
      "Ql1ib0jTDDN6IgQYslrx0dyZzBAsRocq/d3ycXX71iMykoIHZ7rNJ2bDMddRdFk2"
      "D0Ljj4fZjrQNyD4mot/9mqSrF1Q2/AdWQO3pJONcXRWRynJ4Ian3sWdq2B5Dq8Iz"
      "kqrjM7lOq9YEQ+hMRdmOHP4="
      "-----END CERTIFICATE-----";
  static const char kName[] = "eureka8997";
  static const char kSsdpUdn[] = "c5b2a83b-5958-7ce6-b179-e1f44699429b";
  static const char kHotspotBssid[] = "00:1A:11:FF:AC:DF";
  static const char kPublicKey[] =
      "MIGJAoGBAK3SXmWZBOhJibv8It05qIbgHXXhnCXxHkW+C6jNMHR5sZgDpFaOY1xwXERjKdJx"
      "cwrEy3VAT5Uv9MgHPBvxxJku76HYh1yVfIw1rhLnHBTHSxwUzJNCrgc3l3t/UACacLjVNIzc"
      "cDpYf2vnOcA+t1t6IXRjzuU2NdwY4dJXNtWPAgMBAAE=";
  static const char kNonce[] = "+6KSGuRu833m1+TP";
  static const char kSignedData[] =
      "vwMBgANrp5XpCswLyk/OTXT56ORPeIWjH7xAdCk3qgjkwI6+8o56zJS02+tC5hhIHWh7oppT"
      "mWYF4tKvBQ3GeCz7IW9f7HWDMtO7x7yRWxzJyehaJbCfXvLdfs0/WKllzvGVBgNpcIAwU2NS"
      "FUG/jpXclntFzds0EUJG9wHxS6PXXSYRu+PlIFdCDcQJsUlnwO9AGFOJRV/aARGh8YUTWCFI"
      "QPOtPEqT5eegt+TLf01Gq0YcrRwSTKy1I3twOnWiMfIdkJdQKPtBwwbvuAyGuqYFocfjKABb"
      "nH9Tvl04yyO3euKbYlSqaF/l8CXmzDJTyO7tDOFK59bV9auE4KljrQ==";
  static const char kBadSubjectCertData[] =
      "-----BEGIN CERTIFICATE-----"
      "MIIDejCCAmICBFEtN4wwDQYJKoZIhvcNAQEFBQAwfTELMAkGA1UEBhMCVVMxEzARBgNVBAgM"
      "CkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEzARBgNVBAoMCkdvb2dsZSBJ"
      "bmMxEjAQBgNVBAsMCUdvb2dsZSBUVjEYMBYGA1UEAwwPRXVyZWthIEdlbjEgSUNBMB4XDTEz"
      "MDIyNjIyMzAzNloXDTMzMDIyMTIyMzAzNlowdzETMBEGA1UECBMKQ2FsaWZvcm5pYTELMAkG"
      "A1UEBhMCVVMxFjAUBgNVBAcTDU1vdW50YWluIFZpZXcxEjAQBgNVBAsTCUdvb2dsZSBUVjET"
      "MBEGA1UEChMKR29vZ2xlIEluYzESMBAGA1UEAxQJZXZ0X2UxMjYyMIIBIjANBgkqhkiG9w0B"
      "AQEFAAOCAQ8AMIIBCgKCAQEAo7Uu+bdyCjtiUYpmNU4ZvRjDg6VkEh/g0YPDG2pICBU4XKvs"
      "qHH1i0hbtWp1J79hV9Rqst1yHT02Oeh3o1SOd2zeamYzmvXRVN7AZqfQlzWxwxk/ltpXGwew"
      "m+EIR2bP4kpvyEKvvziTMtTxviOK+A395QyodMhMXClKTus/Gme2r1fBoQqJJR/zrmwXCsl5"
      "kpdhj7FOIII3BCYV0zejjQquzywjsKfCVON28VGgJdaKgmXxkeRYYWVNnuTNna57vXe16FP6"
      "hS1ty1U77ESffLTpNJ/M4tsd2dMVVTDuGeX3q8Ix4TN8cqpqu1AKEf59hygys9j6cHZRKR/d"
      "iv0+uQIDAQABow0wCzAJBgNVHRMEAjAAMA0GCSqGSIb3DQEBBQUAA4IBAQAZx6XyEK9SLHE+"
      "rbKCVsLN9+hTEa50aikPmxOZt+lFuB4+VJZ/GCPQCZJIde2tlWUe2YBgoZw2xUKgIsM3Yq42"
      "Gawi35/oZ3qycTgYU8KJP9kUMbYNAH90mz9BDH7MmnRID5dFexHyBCG88EJ+ZvxmUVn0EVDc"
      "sSMt11wIAZ/T+/gsE1120d/GxhjYQ9YZz7SZXBQfRdqCdcPNl2+QSHHl+WvYLzdJa2xYj39/"
      "kQu47Vp7X5rZrHSBvzdVymH0Od2D18t+Q6lxbSdyUNhP1MVhdkT1Ct4OmRS3FJ4aannXMhfq"
      "Ng7k4Sfif5iktYT4VRKpThe0EGJNfqKJKYtvHEVC"
      "-----END CERTIFICATE-----";
  static const char kBadCertData[] = "not a certificate";
  static const char kBadNonce[] = "bad nonce";
  static const char kBadHotspotBssid[] = "bad bssid";

  std::string unsigned_data = base::StringPrintf(
      "%s,%s,%s,%s,%s", kName, kSsdpUdn, kHotspotBssid, kPublicKey, kNonce);
  std::string signed_data;
  base::Base64Decode(kSignedData, &signed_data);

  // Checking basic verification operation.
  EXPECT_TRUE(networking_private_crypto::VerifyCredentials(
      kCertData, std::vector<std::string>(), signed_data, unsigned_data,
      kHotspotBssid));

  // Checking verification operation with an ICA
  std::vector<std::string> icas;
  icas.push_back(kICAData);
  EXPECT_TRUE(networking_private_crypto::VerifyCredentials(
      kCertData, icas, signed_data, unsigned_data, kHotspotBssid));

  // Checking that verification fails when the certificate is signed, but
  // subject is malformed.
  EXPECT_FALSE(networking_private_crypto::VerifyCredentials(
      kBadSubjectCertData, std::vector<std::string>(), signed_data,
      unsigned_data, kHotspotBssid));

  // Checking that verification fails when certificate has invalid format.
  EXPECT_FALSE(networking_private_crypto::VerifyCredentials(
      kBadCertData, std::vector<std::string>(), signed_data, unsigned_data,
      kHotspotBssid));

  // Checking that verification fails if we supply a bad ICA.
  std::vector<std::string> bad_icas;
  bad_icas.push_back(kCertData);
  EXPECT_FALSE(networking_private_crypto::VerifyCredentials(
      kCertData, bad_icas, signed_data, unsigned_data, kHotspotBssid));

  // Checking that verification fails when Hotspot Bssid is invalid.
  EXPECT_FALSE(networking_private_crypto::VerifyCredentials(
      kCertData, std::vector<std::string>(), signed_data, unsigned_data,
      kBadHotspotBssid));

  // Checking that verification fails when there is bad nonce in unsigned_data.
  unsigned_data = base::StringPrintf(
      "%s,%s,%s,%s,%s", kName, kSsdpUdn, kHotspotBssid, kPublicKey, kBadNonce);
  EXPECT_FALSE(networking_private_crypto::VerifyCredentials(
      kCertData, std::vector<std::string>(), signed_data, unsigned_data,
      kHotspotBssid));
}

// Test that networking_private_crypto::EncryptByteString behaves as expected.
TEST_F(NetworkingPrivateCryptoTest, EncryptByteString) {
  static const char kPublicKey[] =
      "MIGJAoGBANTjeoILNkSKHVkd3my/rSwNi+9t473vPJU0lkM8nn9C7+gmaPvEWg4ZNkMd12aI"
      "XDXVHrjgjcS80bPE0ykhN9J7EYkJ+43oulJMrEnyDy5KQo7U3MKBdjaKFTS+OPyohHpI8GqH"
      "KM8UMkLPVtAKu1BXgGTSDvEaBAuoVT2PM4XNAgMBAAE=";
  static const char kPrivateKey[] =
      "-----BEGIN PRIVATE KEY-----"
      "MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBANTjeoILNkSKHVkd"
      "3my/rSwNi+9t473vPJU0lkM8nn9C7+gmaPvEWg4ZNkMd12aIXDXVHrjgjcS80bPE"
      "0ykhN9J7EYkJ+43oulJMrEnyDy5KQo7U3MKBdjaKFTS+OPyohHpI8GqHKM8UMkLP"
      "VtAKu1BXgGTSDvEaBAuoVT2PM4XNAgMBAAECgYEAt91H/2zjj8qhkkhDxDS/wd5p"
      "T37fRTmMX2ktpiCC23LadOxHm7p39Nk9jjYFxV5cFXpdsFrw1kwl6VdC8LDp3eGu"
      "Ku1GCqj5H2fpnkmL2goD01HRkPR3ro4uBHPtTXDbCIz0qp+NGlGG4gPUysMXxHSb"
      "E5FIWeUx6gcPvidwrpkCQQD40FXY46KDJT8JVYJMqY6nFQZvptFl+9BGWfheVVSF"
      "KBlTQBx/QA+XcC/W9Q/I+NEhdGcxLlkEMUpihSpYffKbAkEA2wmFfccdheTtoOuY"
      "8oTurbnFHsS7gLtcR2IbRJKXw80CJxTQA/LMWz0YuFOAYJNl/9ILMfp6MQiI4L9F"
      "l6pbtwJAJqkAXcXo72WvKL0flNfXsYBj0p9h8+2vi+7Y15d8nYAAh13zz5XdllM5"
      "K7ZCMKDwpbkXe53O+QbLnwk/7iYLtwJAERT6AygfJk0HNzCIeglh78x4EgE3uj9i"
      "X/LHu55PFacMTu3xlw09YLQwFFf2wBFeuAeyddBZ7S8ENbrU+5H+mwJBAO2E6gwG"
      "e5ZqY4RmsQmv6K0rn5k+UT4qlPeVp1e6LnvO/PcKWOaUvDK59qFZoX4vN+iFUAbk"
      "IuvhmL9u/uPWWck="
      "-----END PRIVATE KEY-----";
  static const std::vector<uint8> kBadKeyData(5, 111);
  static const char kTestData[] = "disco boy";
  static const char kEmptyData[] = "";

  std::string public_key_string;
  base::Base64Decode(kPublicKey, &public_key_string);
  std::vector<uint8> public_key(public_key_string.begin(),
                                public_key_string.end());
  std::string plain;
  std::vector<uint8> encrypted_output;

  // Checking basic encryption operation.
  plain = kTestData;
  EXPECT_TRUE(networking_private_crypto::EncryptByteString(
      public_key, plain, &encrypted_output));
  EXPECT_TRUE(VerifyByteString(kPrivateKey, plain, encrypted_output));

  // Checking that we can encrypt the empty string.
  plain = kEmptyData;
  EXPECT_TRUE(networking_private_crypto::EncryptByteString(
      public_key, plain, &encrypted_output));

  // Checking graceful fail for too much data to encrypt.
  EXPECT_FALSE(networking_private_crypto::EncryptByteString(
      public_key, std::string(500, 'x'), &encrypted_output));

  // Checking graceful fail for a bad key format.
  EXPECT_FALSE(networking_private_crypto::EncryptByteString(
      kBadKeyData, kTestData, &encrypted_output));
}
