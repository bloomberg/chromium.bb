// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/csp_validator.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::csp_validator::ContentSecurityPolicyIsLegal;
using extensions::csp_validator::ContentSecurityPolicyIsSecure;
using extensions::csp_validator::ContentSecurityPolicyIsSandboxed;
using extensions::Extension;

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
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "img-src https://google.com", Extension::TYPE_EXTENSION));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self'", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'none'", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' ftp://google.com", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://google.com", Extension::TYPE_EXTENSION));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *; default-src 'self'", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self'; default-src *", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self'; default-src *; script-src *; script-src 'self'",
       Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self'; default-src *; script-src 'self'; script-src *",
      Extension::TYPE_EXTENSION));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *; script-src 'self'", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *; script-src 'self'; img-src 'self'",
      Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src *; script-src 'self'; object-src 'self'",
      Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "script-src 'self'; object-src 'self'", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-eval'", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-eval'", Extension::TYPE_PACKAGED_APP));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-eval'", Extension::TYPE_PLATFORM_APP));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-inline'", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-inline' 'none'", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://google.com", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://google.com", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' chrome://resources", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' chrome-extension://aabbcc",
      Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
     "default-src 'self' chrome-extension-resource://aabbcc",
     Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https:", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http:", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' *", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' google.com", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com", Extension::TYPE_EXTENSION));

  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://127.0.0.1", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://localhost", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://lOcAlHoSt", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://127.0.0.1:9999", Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://localhost:8888", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://127.0.0.1.example.com",
      Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://localhost.example.com",
      Extension::TYPE_EXTENSION));

  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' blob:", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' blob:http://example.com/XXX",
      Extension::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' filesystem:", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' filesystem:http://example.com/XXX",
      Extension::TYPE_EXTENSION));
}

TEST(ExtensionCSPValidator, IsSandboxed) {
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed("", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(
      "img-src https://google.com", Extension::TYPE_EXTENSION));

  // Sandbox directive is required.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox", Extension::TYPE_EXTENSION));

  // Additional sandbox tokens are OK.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-scripts", Extension::TYPE_EXTENSION));
  // Except for allow-same-origin.
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-same-origin", Extension::TYPE_EXTENSION));

  // Additional directives are OK.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox; img-src https://google.com", Extension::TYPE_EXTENSION));

  // Extensions allow navigation and popups, platform apps don't.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-top-navigation", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-top-navigation", Extension::TYPE_PLATFORM_APP));
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-popups", Extension::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-popups", Extension::TYPE_PLATFORM_APP));
}
