// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/csp_validator.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::csp_validator::ContentSecurityPolicyIsLegal;
using extensions::csp_validator::ContentSecurityPolicyIsSecure;
using extensions::csp_validator::ContentSecurityPolicyIsSandboxed;
using extensions::Manifest;

TEST(ExtensionCSPValidator, IsLegal) {
  EXPECT_TRUE(ContentSecurityPolicyIsLegal("foo"));
  EXPECT_TRUE(ContentSecurityPolicyIsLegal(
      "default-src 'self'; script-src http://www.google.com"));
  EXPECT_FALSE(ContentSecurityPolicyIsLegal(
      "default-src 'self';\nscript-src http://www.google.com"));
  EXPECT_FALSE(ContentSecurityPolicyIsLegal(
      "default-src 'self';\rscript-src http://www.google.com"));
  EXPECT_FALSE(ContentSecurityPolicyIsLegal(
      "default-src 'self';,script-src http://www.google.com"));
}

TEST(ExtensionCSPValidator, IsSecure) {
  EXPECT_FALSE(
      ContentSecurityPolicyIsSecure(std::string(), Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure("img-src https://google.com",
                                             Manifest::TYPE_EXTENSION));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self'", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'none'", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' ftp://google.com", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://google.com", Manifest::TYPE_EXTENSION));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *; default-src 'self'", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self'; default-src *", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self'; default-src *; script-src *; script-src 'self'",
       Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self'; default-src *; script-src 'self'; script-src *",
      Manifest::TYPE_EXTENSION));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *; script-src 'self'", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *; script-src 'self'; img-src 'self'",
      Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src *; script-src 'self'; object-src 'self'",
      Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "script-src 'self'; object-src 'self'", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-eval'", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-eval'", Manifest::TYPE_LEGACY_PACKAGED_APP));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-eval'", Manifest::TYPE_PLATFORM_APP));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-inline'", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-inline' 'none'", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://google.com", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://google.com", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' chrome://resources", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' chrome-extension://aabbcc",
      Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
     "default-src 'self' chrome-extension-resource://aabbcc",
     Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https:", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http:", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' google.com", Manifest::TYPE_EXTENSION));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' *", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' *:*", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' *:*/", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' *:*/path", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*:*", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*:*/", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*:*/path", Manifest::TYPE_EXTENSION));

  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com:1", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com:*", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com:1/", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com:*/", Manifest::TYPE_EXTENSION));

  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://127.0.0.1", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://localhost", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://lOcAlHoSt", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://127.0.0.1:9999", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://localhost:8888", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://127.0.0.1.example.com",
      Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://localhost.example.com",
      Manifest::TYPE_EXTENSION));

  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' blob:", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' blob:http://example.com/XXX",
      Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' filesystem:", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' filesystem:http://example.com/XXX",
      Manifest::TYPE_EXTENSION));
}

TEST(ExtensionCSPValidator, IsSandboxed) {
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(std::string(),
                                                Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed("img-src https://google.com",
                                                Manifest::TYPE_EXTENSION));

  // Sandbox directive is required.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox", Manifest::TYPE_EXTENSION));

  // Additional sandbox tokens are OK.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-scripts", Manifest::TYPE_EXTENSION));
  // Except for allow-same-origin.
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-same-origin", Manifest::TYPE_EXTENSION));

  // Additional directives are OK.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox; img-src https://google.com", Manifest::TYPE_EXTENSION));

  // Extensions allow navigation, platform apps don't.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-top-navigation", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-top-navigation", Manifest::TYPE_PLATFORM_APP));

  // Popups are OK.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-popups", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-popups", Manifest::TYPE_PLATFORM_APP));
}
