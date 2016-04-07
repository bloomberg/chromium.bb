// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"

#include <stdint.h>

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
                        const std::vector<uint8_t>& encrypted) {
    std::string decrypted;
    if (networking_private_crypto::DecryptByteString(
            private_key_pem, encrypted, &decrypted))
      return decrypted == plain;
    return false;
  }
};

// Test that networking_private_crypto::VerifyCredentials behaves as expected.
TEST_F(NetworkingPrivateCryptoTest, VerifyCredentials) {
  static const char kCertData[] =
      "-----BEGIN CERTIFICATE-----"
      "MIIECjCCAvKgAwIBAgIBbTANBgkqhkiG9w0BAQsFADCBgjELMAkGA1UEBhMCVVMx"
      "EzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEzAR"
      "BgNVBAoMCkdvb2dsZSBJbmMxDTALBgNVBAsMBENhc3QxIjAgBgNVBAMMGUF1ZGlv"
      "IFJlZmVyZW5jZSBEZXYgTW9kZWwwHhcNMTYwMTIyMDYxMjU3WhcNMTYwNTAxMDYx"
      "MjU3WjCBgTELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNV"
      "BAcMDU1vdW50YWluIFZpZXcxEzARBgNVBAoMCkdvb2dsZSBJbmMxDTALBgNVBAsM"
      "BENhc3QxITAfBgNVBAMMGEF1ZGlvIFJlZmVyZW5jZSBEZXYgVGVzdDCCASIwDQYJ"
      "KoZIhvcNAQEBBQADggEPADCCAQoCggEBAKUks+y3cbT3MxuYrD10pEfGHVKfeWRY"
      "1a2Ef6XyvSRD38cRrsDLzW9IEdpb2UOsvyusJ4HpqdQEs6xbl2wuwsqY63gUoWdj"
      "kdWoKRoz5/vs0SfjwefN/8wuxs/wrV/UVycNoYvvYCwdERG7THrFGB8gINvsg4gv"
      "h2lLDH1zJk9GYyTeIAWDpV08WotNKN5XUxigyFRPpymxLV3PW9qUiMInkXQjJAEt"
      "dVFE5qRqAiGm7vxF72/0UywYzotrBka4VN7MUsOGzlN6kAFdFMjor+zNngsa7pbl"
      "K/0Ew4uy5PhzGGQMhDU71kbT8nJVBvwymd8UyRpARe5hjIKrYmt+VTUCAwEAAaOB"
      "iTCBhjAJBgNVHRMEAjAAMB0GA1UdDgQWBBRxGT9wLjw1GOGVKmzE7N9BmeHk/zAf"
      "BgNVHSMEGDAWgBRgKi+tSIAsd/ynRBV9W+ebY6oR/jALBgNVHQ8EBAMCB4AwEwYD"
      "VR0lBAwwCgYIKwYBBQUHAwIwFwYDVR0gBBAwDjAMBgorBgEEAdZ5AgUCMA0GCSqG"
      "SIb3DQEBCwUAA4IBAQBOyNW90Wim20HXLi87BWTeISEEaGlWL9ptUPk7OaE04eRl"
      "LJYUfWhNMYZdJm8gck7zlrM/lvDja/P+GD9YxyoXVQOvRJB9WTRRTGPceLXAqAw+"
      "Ap7w8hdgw6bDlsUEisBAgX6RCo0Dr57wWd9qu83nUCQK8MYgjV6RjHWR3rc9YjOd"
      "lh8KIb6kGSTcgbMC5WpbLVLYxOCyUHSpN1M/fSXxGYGgQJx/bZFF0LZQEn+9lU1w"
      "AMzYwb6kjlRzqERQDebw7knPkhlDL9CHYNHdEEkYoQOqD0DGaxR5vaqB2QYJiN5B"
      "/lQ8TqmvSr6pssHaknPh+jvohhtVMeee94VGlq2A"
      "-----END CERTIFICATE-----";

  static const char kICA1Data[] =
      "-----BEGIN CERTIFICATE-----"
      "MIID9jCCAt6gAwIBAgIBbDANBgkqhkiG9w0BAQsFADB/MQswCQYDVQQGEwJVUzET"
      "MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzETMBEG"
      "A1UECgwKR29vZ2xlIEluYzENMAsGA1UECwwEQ2FzdDEfMB0GA1UEAwwWQ2FzdCBB"
      "dWRpbyBEZXYgUm9vdCBDQTAeFw0xNjAxMjIwNjEyNDVaFw0xNjA1MDEwNjEyNDVa"
      "MIGCMQswCQYDVQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwN"
      "TW91bnRhaW4gVmlldzETMBEGA1UECgwKR29vZ2xlIEluYzENMAsGA1UECwwEQ2Fz"
      "dDEiMCAGA1UEAwwZQXVkaW8gUmVmZXJlbmNlIERldiBNb2RlbDCCASIwDQYJKoZI"
      "hvcNAQEBBQADggEPADCCAQoCggEBAOaUGMZIMbWpqbcotxcyBOUMgyZzvJf46OF8"
      "LuQjV+ZCDwuvL5aw2aVL24lZlJpS4xC16Us4vnkKSuxPkL3rO6/gg/eYNxtX4JFy"
      "m9jaggk6nPAKxu/9kqOz5JIqaCM3itWmm9uavWjOKh6DDo1LsFLwPB9+3ZSHBkVb"
      "uwxSUO3TcZsoUaftCfwsUnm7mKV+F0jB8jOQSwMbKBcRQOHEkz+FUfHJoecjE22B"
      "p4a5xGAArVEulrNBrKkem5MYEfNr6Dq608n5fgLrxr+V3LYk+9dcjxZbFD2aMQ8L"
      "pD7smT9NvOR/H2bWkddeFclgsWVkXKuhCTI/Z5p25vqqBwU6DJUCAwEAAaN5MHcw"
      "DwYDVR0TBAgwBgEB/wIBADAdBgNVHQ4EFgQUYCovrUiALHf8p0QVfVvnm2OqEf4w"
      "HwYDVR0jBBgwFoAUT5PquKZgr6uos163pjd+Zr1DvAYwCwYDVR0PBAQDAgEGMBcG"
      "A1UdIAQQMA4wDAYKKwYBBAHWeQIFAjANBgkqhkiG9w0BAQsFAAOCAQEAJmkzx4JN"
      "/g6n9VtpphzrLGpIK9vhrkC/+8SdU3Et5nRAfPbxwBaYcOIVlDhmnjFU15kz4Mpm"
      "xRzdLdL/nnbBf2mssIn3RXD/J1/+7BClM2Ew/B0NStJ0aRV8gN+t6hkOmZz6Ikjn"
      "dYaeAUvS1jCCskSCEE1hwQE3aJ8dAddng4V+bZiIO72LCHUMb+BywWIzEqlLeTnY"
      "Th/2240ZdTIzwYpLD+A6+ft6uJFJTtv1E0tT3EJ5kDzrkZoQTwJbWR7YgK6UjafH"
      "/9WmhhymOsmVnw43xJ0cEwWonitX8xq6wv3VWJvlYmJ6i0MMwktNTzVedaHa9nN/"
      "zgfBYe0mPzwEvQ=="
      "-----END CERTIFICATE-----";

  static const char kICA2Data[] =
      "-----BEGIN CERTIFICATE-----"
      "MIID6DCCAtCgAwIBAgIBZDANBgkqhkiG9w0BAQsFADB1MQswCQYDVQQGEwJVUzET"
      "MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzETMBEG"
      "A1UECgwKR29vZ2xlIEluYzENMAsGA1UECwwEQ2FzdDEVMBMGA1UEAwwMQ2FzdCBS"
      "b290IENBMB4XDTE2MDEyMjA2MTAyN1oXDTE2MDUwMTA2MTAyN1owfzELMAkGA1UE"
      "BhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZp"
      "ZXcxEzARBgNVBAoMCkdvb2dsZSBJbmMxDTALBgNVBAsMBENhc3QxHzAdBgNVBAMM"
      "FkNhc3QgQXVkaW8gRGV2IFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw"
      "ggEKAoIBAQC5hu6BmwfxxhVqMB5BvTtw9JaIfffTKWM9uMqh7D2BQwiFKzBfS2tU"
      "GamSRpiaLir/nfNIsl3WCpxJgWpjGhLnjjw5dGdnx9XU83xFZQeEFbHfCxYNId5x"
      "JLCIAIppz65wJIJkYEjIWlKGUHM24CRTXOhYE3opuIqoOWiYEr+fN99gZ+A/H/re"
      "t9GkF8PCxbW+15jhPQ1ZZuUHSZq7nk/zNzg33wwZi839LPz1qQlrStMTIo/9+WTl"
      "LF++WqWEMpzlKnP13KpXwn8+1nyfVfAonCG65plh/DkNMawUncGXtejOlxsZuMCu"
      "UhoqRnos+MQYfWpzEiDsOUfg3uPVjYIPAgMBAAGjeTB3MA8GA1UdEwQIMAYBAf8C"
      "AQEwHQYDVR0OBBYEFE+T6rimYK+rqLNet6Y3fma9Q7wGMB8GA1UdIwQYMBaAFHya"
      "Hn3feVS818xeypmGRXlldCgZMAsGA1UdDwQEAwIBBjAXBgNVHSAEEDAOMAwGCisG"
      "AQQB1nkCBQIwDQYJKoZIhvcNAQELBQADggEBALhO43XjmlqcZdNa3sMSHLxbl1ip"
      "wRdTcRzaR7REUVnr05dWubZNy7q3h7jeGDP0eML5eyULy25qbN+g4IhPCCXssfVf"
      "JNRHxspPx4a4hOrp0/Wybfq2HqL+r6xhkfB7GppSxYrWuZ8bTArlDEW529GXmW/M"
      "7qbWQc7Uz2OI5AHuBadhbOhBvSlZVKu0lPccMMLqi5ie585qAiim1mHp6VgjKtUh"
      "LAFi+BHdbo4txcau+onG/dngYr70/35YFcrb08vakkVp1EbGSLqWp+++nicIdZKU"
      "hciORE5xdaHj9l4lWYBdng8Bfm1Bci2uLVaxbdayk/xsBSLfKA8JYmFlRi0="
      "-----END CERTIFICATE-----";

  unsigned char kData[] = {
      0x5f, 0x76, 0x0d, 0xc8, 0x4b, 0xe7, 0x6e, 0xcb, 0x31, 0x58, 0xca, 0xd3,
      0x7d, 0x23, 0x55, 0xbe, 0x8d, 0x52, 0x87, 0x83, 0x27, 0x52, 0x78, 0xfa,
      0xa6, 0xdd, 0xdf, 0x13, 0x00, 0x51, 0x57, 0x6a, 0x83, 0x15, 0xcc, 0xc5,
      0xb2, 0x5c, 0xdf, 0xe6, 0x81, 0xdc, 0x13, 0x58, 0x7b, 0x94, 0x0f, 0x69,
      0xcc, 0xdf, 0x68, 0x41, 0x8a, 0x95, 0xe2, 0xcd, 0xf8, 0xde, 0x0f, 0x2f,
      0x30, 0xcf, 0x73, 0xbf, 0x37, 0x52, 0x87, 0x23, 0xd7, 0xbe, 0xba, 0x7c,
      0xde, 0x50, 0xd3, 0x77, 0x9c, 0x06, 0x82, 0x28, 0x67, 0xc1, 0x1a, 0xf5,
      0x8a, 0xa0, 0xf2, 0x32, 0x09, 0x95, 0x41, 0x41, 0x93, 0x8e, 0x62, 0xaa,
      0xf3, 0xe3, 0x22, 0x17, 0x43, 0x94, 0x9b, 0x63, 0xfa, 0x68, 0x20, 0x69,
      0x38, 0xf6, 0x75, 0x6c, 0xe0, 0x3b, 0xe0, 0x8d, 0x63, 0xac, 0x7f, 0xe3,
      0x09, 0xd8, 0xde, 0x91, 0xc8, 0x1e, 0x07, 0x4a, 0xb2, 0x1e, 0xe1, 0xe3,
      0xf4, 0x4d, 0x3e, 0x8a, 0xf4, 0xf8, 0x83, 0x39, 0x2b, 0x50, 0x98, 0x61,
      0x91, 0x50, 0x00, 0x34, 0x57, 0xd2, 0x0d, 0xf7, 0xfa, 0xc9, 0xcc, 0xd9,
      0x7a, 0x3d, 0x39, 0x7a, 0x1a, 0xbd, 0xf8, 0xbe, 0x65, 0xb6, 0xea, 0x4e,
      0x86, 0x74, 0xdd, 0x51, 0x74, 0x6e, 0xa6, 0x7f, 0x14, 0x6c, 0x6a, 0x46,
      0xb8, 0xaf, 0xcd, 0x6c, 0x78, 0x43, 0x76, 0x47, 0x5b, 0xdc, 0xb6, 0xf6,
      0x4d, 0x1b, 0xe0, 0xb5, 0xf9, 0xa2, 0xb8, 0x26, 0x3f, 0x3f, 0xb8, 0x80,
      0xed, 0xce, 0xfd, 0x0e, 0xcb, 0x48, 0x7a, 0x3b, 0xdf, 0x92, 0x44, 0x04,
      0x81, 0xe4, 0xd3, 0x1e, 0x07, 0x9b, 0x02, 0xae, 0x05, 0x5a, 0x11, 0xf2,
      0xc2, 0x75, 0x85, 0xd5, 0xf1, 0x53, 0x4c, 0x09, 0xd0, 0x99, 0xf8, 0x3e,
      0xf6, 0x24, 0x46, 0xae, 0x83, 0x35, 0x3e, 0x6c, 0x8c, 0x2a, 0x9f, 0x1c,
      0x5b, 0xfb, 0x89, 0x56};

  unsigned char kSignature[] = {
      0x52, 0x56, 0xcd, 0x53, 0xfa, 0xd9, 0x44, 0x31, 0x00, 0x2e, 0x85, 0x18,
      0x56, 0xae, 0xf9, 0xf2, 0x70, 0x16, 0xc9, 0x59, 0x53, 0xc0, 0x17, 0xd9,
      0x09, 0x65, 0x75, 0xee, 0xba, 0xc8, 0x0d, 0x06, 0x2e, 0xb7, 0x1b, 0xd0,
      0x6a, 0x4d, 0x58, 0xde, 0x8e, 0xbe, 0x92, 0x22, 0x53, 0x19, 0xbf, 0x74,
      0x8f, 0xb8, 0xfc, 0x3c, 0x9b, 0x42, 0x14, 0x7d, 0xe1, 0xfc, 0xa3, 0x71,
      0x91, 0x6c, 0x5d, 0x28, 0x69, 0x8d, 0xd2, 0xde, 0xd1, 0x8f, 0xac, 0x6d,
      0xf6, 0x48, 0xd8, 0x6f, 0x0e, 0xc9, 0x0a, 0xfa, 0xde, 0x20, 0xe0, 0x9d,
      0x7a, 0xf8, 0x30, 0xa8, 0xd4, 0x79, 0x15, 0x63, 0xfb, 0x97, 0xa9, 0xef,
      0x9f, 0x9c, 0xac, 0x16, 0xba, 0x1b, 0x2c, 0x14, 0xb4, 0xa4, 0x54, 0x5e,
      0xec, 0x04, 0x10, 0x84, 0xc2, 0xa0, 0xd9, 0x6f, 0x05, 0xd4, 0x09, 0x8c,
      0x85, 0xe9, 0x7a, 0xd1, 0x5a, 0xa3, 0x70, 0x00, 0x30, 0x9b, 0x19, 0x44,
      0x2a, 0x90, 0x7a, 0xcd, 0x91, 0x94, 0x90, 0x66, 0xf9, 0x2e, 0x5e, 0x43,
      0x27, 0x33, 0x2c, 0x45, 0xa7, 0xe2, 0x3a, 0x6d, 0xc9, 0x44, 0x58, 0x39,
      0x45, 0xcb, 0xbd, 0x2f, 0xc5, 0xb4, 0x08, 0x41, 0x4d, 0x45, 0x67, 0x55,
      0x0d, 0x43, 0x3c, 0xb6, 0x81, 0xbb, 0xb4, 0x34, 0x07, 0x10, 0x28, 0x17,
      0xc2, 0xad, 0x40, 0x3b, 0xaf, 0xcb, 0xc0, 0xf6, 0x9d, 0x0e, 0x9b, 0xca,
      0x2b, 0x20, 0xdf, 0xd0, 0xa3, 0xbe, 0xea, 0x3e, 0xe0, 0x82, 0x7b, 0x93,
      0xfd, 0x9c, 0xaf, 0x97, 0x00, 0x05, 0x44, 0x91, 0x73, 0x68, 0x92, 0x3a,
      0x8b, 0xbc, 0x0e, 0x96, 0x5e, 0x92, 0x98, 0x70, 0xab, 0xaa, 0x6e, 0x9a,
      0x8e, 0xb0, 0xf4, 0x92, 0xc5, 0xa0, 0xa0, 0x4b, 0xb3, 0xd5, 0x44, 0x99,
      0x8e, 0xa1, 0xd1, 0x8f, 0xe3, 0xac, 0x71, 0x1e, 0x3f, 0xc2, 0xfd, 0x0a,
      0x57, 0xed, 0xea, 0x04};

  // TODO(eroman): This should look like 00:1A:11:FF:AC:DF. Instead this is
  // using an existing test certificate. It works because the implementation is
  // just doing a suffix test and not parsing the format.
  static const char kHotspotBssid[] = "Reference Dev Test";

  static const char kBadCertData[] = "not a certificate";
  static const char kBadHotspotBssid[] = "bad bssid";

  // April 1, 2016
  base::Time::Exploded time = {0};
  time.year = 2016;
  time.month = 4;
  time.day_of_month = 1;

  // September 1, 2035
  base::Time::Exploded expired_time = {0};
  expired_time.year = 2035;
  expired_time.month = 9;
  expired_time.day_of_month = 1;

  std::string unsigned_data = std::string(std::begin(kData), std::end(kData));
  std::string signed_data =
      std::string(std::begin(kSignature), std::end(kSignature));

  // Check that verification fails when the intermediaries are not provided.
  EXPECT_FALSE(networking_private_crypto::VerifyCredentialsAtTime(
      kCertData, std::vector<std::string>(), signed_data, unsigned_data,
      kHotspotBssid, time));

  // Checking basic verification operation.
  std::vector<std::string> icas;
  icas.push_back(kICA1Data);
  icas.push_back(kICA2Data);

  EXPECT_TRUE(networking_private_crypto::VerifyCredentialsAtTime(
      kCertData, icas, signed_data, unsigned_data, kHotspotBssid, time));

  // Checking that verification fails when the certificate is expired.
  EXPECT_FALSE(networking_private_crypto::VerifyCredentialsAtTime(
      kCertData, icas, signed_data, unsigned_data, kHotspotBssid,
      expired_time));

  // Checking that verification fails when certificate has invalid format.
  EXPECT_FALSE(networking_private_crypto::VerifyCredentialsAtTime(
      kBadCertData, icas, signed_data, unsigned_data, kHotspotBssid, time));

  // Checking that verification fails if we supply a bad ICA.
  std::vector<std::string> bad_icas;
  bad_icas.push_back(kCertData);
  EXPECT_FALSE(networking_private_crypto::VerifyCredentialsAtTime(
      kCertData, bad_icas, signed_data, unsigned_data, kHotspotBssid, time));

  // Checking that verification fails when Hotspot Bssid does not match the
  // certificate's common name.
  EXPECT_FALSE(networking_private_crypto::VerifyCredentialsAtTime(
      kCertData, icas, signed_data, unsigned_data, kBadHotspotBssid, time));

  // Checking that verification fails when the signature is wrong.
  unsigned_data = "bad data";
  EXPECT_FALSE(networking_private_crypto::VerifyCredentialsAtTime(
      kCertData, icas, signed_data, unsigned_data, kHotspotBssid, time));
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
  static const std::vector<uint8_t> kBadKeyData(5, 111);
  static const char kTestData[] = "disco boy";
  static const char kEmptyData[] = "";

  std::string public_key_string;
  base::Base64Decode(kPublicKey, &public_key_string);
  std::vector<uint8_t> public_key(public_key_string.begin(),
                                  public_key_string.end());
  std::string plain;
  std::vector<uint8_t> encrypted_output;

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
