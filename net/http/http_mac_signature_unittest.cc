// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "net/http/http_mac_signature.h"

namespace net {

TEST(HttpMacSignatureTest, BogusAddStateInfo) {
  HttpMacSignature signature;
  EXPECT_FALSE(signature.AddStateInfo("exciting-id",
                                      "the-mac-key",
                                      "bogus-hmac-algorithm",
                                      "the-issuer"));
  EXPECT_FALSE(signature.AddStateInfo("",
                                      "the-mac-key",
                                      "hmac-sha-1",
                                      "the-issuer"));
  EXPECT_FALSE(signature.AddStateInfo("exciting-id",
                                      "",
                                      "hmac-sha-1",
                                      "the-issuer"));
  EXPECT_FALSE(signature.AddStateInfo("exciting-id",
                                      "the-mac-key",
                                      "",
                                      "the-issuer"));
  EXPECT_FALSE(signature.AddStateInfo("exciting-id",
                                      "the-mac-key",
                                      "hmac-sha-1",
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
                                     "adiMf03j0f3nOenc003r",
                                     "hmac-sha-1",
                                     "login.eXampLe.com:443"));
  EXPECT_TRUE(signature.AddHttpInfo("GeT",
                                    "/pAth?to=%22enlightenment%22&dest=magic",
                                    "eXaMple.com",
                                    80));

  std::string timestamp = "239034";
  std::string nonce = "mn4302j0n+32r2/f3r=";

  EXPECT_EQ("MAC id=\"dfoi30j0qnf\", "
            "issuer=\"login.eXampLe.com:443\", "
            "timestamp=\"239034\", "
            "nonce=\"mn4302j0n+32r2/f3r=\", "
            "mac=\"zQWLNI5eHOfY5/wCJ6yzZ8bXDw==\"",
            signature.GenerateHeaderString(timestamp, nonce));
}


TEST(HttpMacSignatureTest, GenerateNormalizedRequest) {
  HttpMacSignature signature;
  EXPECT_TRUE(signature.AddStateInfo("dfoi30j0qnf",
                                     "adiMf03j0f3nOenc003r",
                                     "hmac-sha-1",
                                     "login.eXampLe.com:443"));
  EXPECT_TRUE(signature.AddHttpInfo("GeT",
                                    "/pAth?to=%22enlightenment%22&dest=magic",
                                    "eXaMple.com",
                                    80));

  std::string timestamp = "239034";
  std::string nonce = "mn4302j0n+32r2/f3r=";

  EXPECT_EQ("dfoi30j0qnf\n"
            "login.eXampLe.com:443\n"
            "239034\n"
            "mn4302j0n+32r2/f3r=\n"
            "GET\n"
            "/pAth?to=%22enlightenment%22&dest=magic\n"
            "example.com\n"
            "80\n",
            signature.GenerateNormalizedRequest(timestamp, nonce));
}

TEST(HttpMacSignatureTest, GenerateMAC) {
  HttpMacSignature signature;
  EXPECT_TRUE(signature.AddStateInfo("dfoi30j0qnf",
                                     "adiMf03j0f3nOenc003r",
                                     "hmac-sha-1",
                                     "login.eXampLe.com:443"));
  EXPECT_TRUE(signature.AddHttpInfo("GeT",
                                     "/pAth?to=%22enlightenment%22&dest=magic",
                                     "eXaMple.com",
                                     80));

  std::string timestamp = "239034";
  std::string nonce = "mn4302j0n+32r2/f3r=";

  EXPECT_EQ("zQWLNI5eHOfY5/wCJ6yzZ8bXDw==",
            signature.GenerateMAC(timestamp, nonce));
}
}
