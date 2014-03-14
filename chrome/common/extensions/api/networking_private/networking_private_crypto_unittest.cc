// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests of NetworkingPrivateCrypto support for Networking Private API.
class NetworkingPrivateCryptoTest : public testing::Test {
 protected:
  // Verify that decryption of |encrypted| data using |private_key_pem| matches
  // |plain| data.
  bool VerifyByteString(const std::string& private_key_pem,
                        const std::string& plain,
                        const std::vector<uint8>& encrypted) {
    NetworkingPrivateCrypto crypto;
    std::string decrypted;
    if (crypto.DecryptByteString(private_key_pem, encrypted, &decrypted))
      return decrypted == plain;
    return false;
  }
};

// Test that NetworkingPrivateCrypto::VerifyCredentials behaves as expected.
TEST_F(NetworkingPrivateCryptoTest, VerifyCredentials) {
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

  NetworkingPrivateCrypto crypto;
  // Checking basic verification operation.
  EXPECT_TRUE(crypto.VerifyCredentials(
      kCertData, signed_data, unsigned_data, kHotspotBssid));

  // Checking that verification fails when the certificate is signed, but
  // subject is malformed.
  EXPECT_FALSE(crypto.VerifyCredentials(
      kBadSubjectCertData, signed_data, unsigned_data, kHotspotBssid));

  // Checking that verification fails when certificate has invalid format.
  EXPECT_FALSE(crypto.VerifyCredentials(
      kBadCertData, signed_data, unsigned_data, kHotspotBssid));

  // Checking that verification fails when Hotspot Bssid is invalid.
  EXPECT_FALSE(crypto.VerifyCredentials(
      kCertData, signed_data, unsigned_data, kBadHotspotBssid));

  // Checking that verification fails when there is bad nonce in unsigned_data.
  unsigned_data = base::StringPrintf(
      "%s,%s,%s,%s,%s", kName, kSsdpUdn, kHotspotBssid, kPublicKey, kBadNonce);
  EXPECT_FALSE(crypto.VerifyCredentials(
      kCertData, signed_data, unsigned_data, kHotspotBssid));
}

// Test that NetworkingPrivateCrypto::EncryptByteString behaves as expected.
TEST_F(NetworkingPrivateCryptoTest, EncryptByteString) {
  NetworkingPrivateCrypto crypto;
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
  EXPECT_TRUE(crypto.EncryptByteString(public_key, plain, &encrypted_output));
  EXPECT_TRUE(VerifyByteString(kPrivateKey, plain, encrypted_output));

  // Checking that we can encrypt the empty string.
  plain = kEmptyData;
  EXPECT_TRUE(crypto.EncryptByteString(public_key, plain, &encrypted_output));

  // Checking graceful fail for too much data to encrypt.
  EXPECT_FALSE(crypto.EncryptByteString(
      public_key, std::string(500, 'x'), &encrypted_output));

  // Checking graceful fail for a bad key format.
  EXPECT_FALSE(
      crypto.EncryptByteString(kBadKeyData, kTestData, &encrypted_output));
}
