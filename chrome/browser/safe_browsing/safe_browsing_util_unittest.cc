// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/sha2.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool VectorContains(const std::vector<std::string>& data,
                    const std::string& str) {
  return std::find(data.begin(), data.end(), str) != data.end();
}

}

// Tests that we generate the required host/path combinations for testing
// according to the Safe Browsing spec.
// See section 6.2 in
// http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec.
TEST(SafeBrowsingUtilTest, UrlParsing) {
  std::vector<std::string> hosts, paths;

  GURL url("http://a.b.c/1/2.html?param=1");
  safe_browsing_util::GenerateHostsToCheck(url, &hosts);
  safe_browsing_util::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(hosts.size(), static_cast<size_t>(2));
  EXPECT_EQ(paths.size(), static_cast<size_t>(4));
  EXPECT_EQ(hosts[0], "b.c");
  EXPECT_EQ(hosts[1], "a.b.c");

  EXPECT_TRUE(VectorContains(paths, "/1/2.html?param=1"));
  EXPECT_TRUE(VectorContains(paths, "/1/2.html"));
  EXPECT_TRUE(VectorContains(paths, "/1/"));
  EXPECT_TRUE(VectorContains(paths, "/"));

  url = GURL("http://a.b.c.d.e.f.g/1.html");
  safe_browsing_util::GenerateHostsToCheck(url, &hosts);
  safe_browsing_util::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(hosts.size(), static_cast<size_t>(5));
  EXPECT_EQ(paths.size(), static_cast<size_t>(2));
  EXPECT_EQ(hosts[0], "f.g");
  EXPECT_EQ(hosts[1], "e.f.g");
  EXPECT_EQ(hosts[2], "d.e.f.g");
  EXPECT_EQ(hosts[3], "c.d.e.f.g");
  EXPECT_EQ(hosts[4], "a.b.c.d.e.f.g");
  EXPECT_TRUE(VectorContains(paths, "/1.html"));
  EXPECT_TRUE(VectorContains(paths, "/"));

  url = GURL("http://a.b/saw-cgi/eBayISAPI.dll/");
  safe_browsing_util::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(paths.size(), static_cast<size_t>(3));
  EXPECT_TRUE(VectorContains(paths, "/saw-cgi/eBayISAPI.dll/"));
  EXPECT_TRUE(VectorContains(paths, "/saw-cgi/"));
  EXPECT_TRUE(VectorContains(paths, "/"));
}


TEST(SafeBrowsingUtilTest, FullHashCompare) {
  GURL url("http://www.evil.com/phish.html");
  SBFullHashResult full_hash;
  base::SHA256HashString(url.host() + url.path(),
                         &full_hash.hash,
                         sizeof(SBFullHash));
  std::vector<SBFullHashResult> full_hashes;
  full_hashes.push_back(full_hash);

  EXPECT_EQ(safe_browsing_util::CompareFullHashes(url, full_hashes), 0);

  url = GURL("http://www.evil.com/okay_path.html");
  EXPECT_EQ(safe_browsing_util::CompareFullHashes(url, full_hashes), -1);
}
