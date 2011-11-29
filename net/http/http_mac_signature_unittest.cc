// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "net/http/http_mac_signature.h"

namespace net {

TEST(HttpMacSignatureTest, BogusAddStateInfo) {
  HttpMacSignature signature;
  EXPECT_FALSE(signature.AddStateInfo("exciting-id",
                                      base::Time::Now(),
                                      "the-mac-key",
                                      "bogus-hmac-algorithm"));
  EXPECT_FALSE(signature.AddStateInfo("",
                                      base::Time::Now(),
                                      "the-mac-key",
                                      "hmac-sha-1"));
  EXPECT_FALSE(signature.AddStateInfo("exciting-id",
                                      base::Time(),
                                      "the-mac-key",
                                      "hmac-sha-1"));
  EXPECT_FALSE(signature.AddStateInfo("exciting-id",
                                      base::Time::Now(),
                                      "",
                                      "hmac-sha-1"));
  EXPECT_FALSE(signature.AddStateInfo("exciting-id",
                                      base::Time::Now(),
                                      "the-mac-key",
                                      ""));
}

TEST(HttpMacSignatureTest, BogusAddHttpInfo) {
  HttpMacSignature signature;
  EXPECT_FALSE(signature.AddHttpInfo("GET", "/requested", "example.com", 0));
  EXPECT_FALSE(signature.AddHttpInfo(
      "GET", "/requested", "example.com", 29088983));
  EXPECT_FALSE(signature.AddHttpInfo("", "/requested", "example.com", 80));
  EXPECT_FALSE(signature.AddHttpInfo("GET", "", "example.com", 80));
  EXPECT_FALSE(signature.AddHttpInfo("GET", "/requested", "", 80));
}

TEST(HttpMacSignatureTest, GenerateHeaderString) {
  HttpMacSignature signature;
  EXPECT_TRUE(signature.AddStateInfo("dfoi30j0qnf",
                                     base::Time::Now(),
                                     "adiMf03j0f3nOenc003r",
                                     "hmac-sha-1"));
  EXPECT_TRUE(signature.AddHttpInfo("GeT",
                                    "/pAth?to=%22enlightenment%22&dest=magic",
                                    "eXaMple.com",
                                    80));

  std::string age = "239034";
  std::string nonce = "mn4302j0n+32r2/f3r=";

  std::string header_string;
  EXPECT_TRUE(signature.GenerateHeaderString(age, nonce, &header_string));
  EXPECT_EQ("MAC id=\"dfoi30j0qnf\", "
            "nonce=\"239034:mn4302j0n+32r2/f3r=\", "
            "mac=\"GrkHtPKzB1m1dCHfa7OCWOw6Ecw=\"",
            header_string);
}


TEST(HttpMacSignatureTest, GenerateNormalizedRequest) {
  HttpMacSignature signature;
  EXPECT_TRUE(signature.AddStateInfo("dfoi30j0qnf",
                                     base::Time::Now(),
                                     "adiMf03j0f3nOenc003r",
                                     "hmac-sha-1"));
  EXPECT_TRUE(signature.AddHttpInfo("GeT",
                                    "/pAth?to=%22enlightenment%22&dest=magic",
                                    "eXaMple.com",
                                    80));

  std::string age = "239034";
  std::string nonce = "mn4302j0n+32r2/f3r=";

  EXPECT_EQ("239034:mn4302j0n+32r2/f3r=\n"
            "GET\n"
            "/pAth?to=%22enlightenment%22&dest=magic\n"
            "example.com\n"
            "80\n"
            "\n"
            "\n",
            signature.GenerateNormalizedRequest(age, nonce));
}

TEST(HttpMacSignatureTest, GenerateMAC) {
  HttpMacSignature signature;
  EXPECT_TRUE(signature.AddStateInfo("dfoi30j0qnf",
                                     base::Time::Now(),
                                     "adiMf03j0f3nOenc003r",
                                     "hmac-sha-1"));
  EXPECT_TRUE(signature.AddHttpInfo("GeT",
                                     "/pAth?to=%22enlightenment%22&dest=magic",
                                     "eXaMple.com",
                                     80));

  std::string age = "239034";
  std::string nonce = "mn4302j0n+32r2/f3r=";

  std::string mac;
  EXPECT_TRUE(signature.GenerateMAC(age, nonce, &mac));
  EXPECT_EQ("GrkHtPKzB1m1dCHfa7OCWOw6Ecw=", mac);
}
}
