// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// A macro, so that the IsRequestorURLInVerifiedSite calls are inside of the
// the test, which is marked as a friend of WebstoreStandaloneInstaller.
#define IsVerified(requestor_url, verified_site) \
  WebstoreStandaloneInstaller::IsRequestorURLInVerifiedSite( \
      GURL(requestor_url), verified_site)

TEST(WebstoreStandaloneInstallerTest, DomainVerification) {
  // Exact domain match.
  EXPECT_TRUE(IsVerified("http://example.com", "example.com"));

  // The HTTPS scheme is allowed.
  EXPECT_TRUE(IsVerified("https://example.com", "example.com"));

  // The file: scheme is not allowed.
  EXPECT_FALSE(IsVerified("file:///example.com", "example.com"));

  // Trailing slash in URL.
  EXPECT_TRUE(IsVerified("http://example.com/", "example.com"));

  // Page on the domain.
  EXPECT_TRUE(IsVerified("http://example.com/page.html", "example.com"));

  // Page on a subdomain.
  EXPECT_TRUE(IsVerified("http://sub.example.com/page.html", "example.com"));

  // Root domain when only a subdomain is verified.
  EXPECT_FALSE(IsVerified("http://example.com/", "sub.example.com"));

  // Different subdomain when only a subdomain is verified.
  EXPECT_FALSE(IsVerified("http://www.example.com/", "sub.example.com"));

  // Port matches.
  EXPECT_TRUE(IsVerified("http://example.com:123/", "example.com:123"));

  // Port doesn't match.
  EXPECT_FALSE(IsVerified("http://example.com:456/", "example.com:123"));

  // Port is missing in the requestor URL.
  EXPECT_FALSE(IsVerified("http://example.com/", "example.com:123"));

  // Port is missing in the verified site (any port matches).
  EXPECT_TRUE(IsVerified("http://example.com:123/", "example.com"));

  // Path matches.
  EXPECT_TRUE(IsVerified("http://example.com/path", "example.com/path"));

  // Path doesn't match.
  EXPECT_FALSE(IsVerified("http://example.com/foo", "example.com/path"));

  // Path is missing.
  EXPECT_FALSE(IsVerified("http://example.com", "example.com/path"));

  // Path matches (with trailing slash).
  EXPECT_TRUE(IsVerified("http://example.com/path/", "example.com/path"));

  // Path matches (is a file under the path).
  EXPECT_TRUE(IsVerified(
      "http://example.com/path/page.html", "example.com/path"));

  // Path and port match.
  EXPECT_TRUE(IsVerified(
      "http://example.com:123/path/page.html", "example.com:123/path"));
}

}  // namespace extensions
