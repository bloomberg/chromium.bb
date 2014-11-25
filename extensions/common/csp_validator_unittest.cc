// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/csp_validator.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::csp_validator::ContentSecurityPolicyIsLegal;
using extensions::csp_validator::ContentSecurityPolicyIsSecure;
using extensions::csp_validator::ContentSecurityPolicyIsSandboxed;
using extensions::csp_validator::OPTIONS_NONE;
using extensions::csp_validator::OPTIONS_ALLOW_UNSAFE_EVAL;
using extensions::csp_validator::OPTIONS_ALLOW_INSECURE_OBJECT_SRC;
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
      ContentSecurityPolicyIsSecure(std::string(), OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure("img-src https://google.com",
                                             OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self'", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'none'", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' ftp://google.com", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://google.com", OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *; default-src 'self'", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self'; default-src *", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self'; default-src *; script-src *; script-src 'self'",
       OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self'; default-src *; script-src 'self'; script-src *",
      OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *; script-src 'self'", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src *; script-src 'self'; img-src 'self'",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src *; script-src 'self'; object-src 'self'",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "script-src 'self'; object-src 'self'", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-eval'", OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-eval'", OPTIONS_NONE));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-inline'", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'unsafe-inline' 'none'", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://google.com", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://google.com", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' chrome://resources", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' chrome-extension://aabbcc",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
     "default-src 'self' chrome-extension-resource://aabbcc",
     OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https:", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http:", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' google.com", OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' *", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' *:*", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' *:*/", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' *:*/path", OPTIONS_ALLOW_UNSAFE_EVAL));
  // "https://" is an invalid CSP, so it will be ignored by Blink.
  // TODO(robwu): Change to EXPECT_FALSE once http://crbug.com/434773 is fixed.
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*:*", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*:*/", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*:*/path", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.com", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.*.google.com/", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.*.google.com:*/",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://www.*.google.com/",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://www.*.google.com:*/",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' chrome://*", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' chrome-extension://*", OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com:1", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com:*", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com:1/", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.google.com:*/", OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://127.0.0.1", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://localhost", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://lOcAlHoSt", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://127.0.0.1:9999", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://localhost:8888", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://127.0.0.1.example.com",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' http://localhost.example.com",
      OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' blob:", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' blob:http://example.com/XXX",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' filesystem:", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
      "default-src 'self' filesystem:http://example.com/XXX",
      OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://*.googleapis.com",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
      "default-src 'self' https://x.googleapis.com",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  // "chrome-extension://" is an invalid CSP and ignored by Blink, but extension
  // authors have been using this string anyway, so we cannot refuse this string
  // until extensions can be loaded with an invalid CSP. http://crbug.com/434773
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
     "default-src 'self' chrome-extension://", OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
     "script-src 'self'; object-src *", OPTIONS_NONE));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
     "script-src 'self'; object-src *", OPTIONS_ALLOW_INSECURE_OBJECT_SRC));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
     "script-src 'self'; object-src http://www.example.com",
     OPTIONS_ALLOW_INSECURE_OBJECT_SRC));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
     "object-src http://www.example.com blob:; script-src 'self'",
     OPTIONS_ALLOW_INSECURE_OBJECT_SRC));
  EXPECT_TRUE(ContentSecurityPolicyIsSecure(
     "script-src 'self'; object-src http://*.example.com",
     OPTIONS_ALLOW_INSECURE_OBJECT_SRC));
  EXPECT_FALSE(ContentSecurityPolicyIsSecure(
     "script-src *; object-src *;", OPTIONS_ALLOW_INSECURE_OBJECT_SRC));
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
