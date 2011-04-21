// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "base/logging.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"

using base::Time;
using base::TimeDelta;

static const char kInfoUrlPrefix[] = "http://info.prefix.com/foo";
static const char kMacKeyUrlPrefix[] = "https://key.prefix.com/bar";
static const char kClient[] = "unittest";
static const char kAppVer[] = "1.0";
static const char kClientKey[] = "SCg9lcLHd0dfksXgYsacwQ==";
static const char kWrappedKey[] =
    "AKEgNisjLl7iRYrjWHmpd_XwCiilxrw8nNaYH47tiQ7pDe9cEErjVHGZaPPUau5h61tbXSDqA"
    "BiJZnDFByc_g8B5vTwxkhBf9g==";
static const char kAdditionalQuery[] = "additional_query";

class SafeBrowsingProtocolManagerTest : public testing::Test {
};

// Ensure that we respect section 5 of the SafeBrowsing protocol specification.
TEST_F(SafeBrowsingProtocolManagerTest, TestBackOffTimes) {
  SafeBrowsingProtocolManager pm(NULL, kClient, kClientKey, kWrappedKey, NULL,
                                 kInfoUrlPrefix, kMacKeyUrlPrefix, false);
  pm.next_update_sec_ = 1800;
  DCHECK(pm.back_off_fuzz_ >= 0.0 && pm.back_off_fuzz_ <= 1.0);

  // No errors received so far.
  EXPECT_EQ(pm.GetNextUpdateTime(false), 1800 * 1000);

  // 1 error.
  EXPECT_EQ(pm.GetNextUpdateTime(true), 60 * 1000);

  // 2 errors.
  int next_time = pm.GetNextUpdateTime(true) / (60 * 1000);  // Minutes
  EXPECT_TRUE(next_time >= 30 && next_time <= 60);

  // 3 errors.
  next_time = pm.GetNextUpdateTime(true) / (60 * 1000);
  EXPECT_TRUE(next_time >= 60 && next_time <= 120);

  // 4 errors.
  next_time = pm.GetNextUpdateTime(true) / (60 * 1000);
  EXPECT_TRUE(next_time >= 120 && next_time <= 240);

  // 5 errors.
  next_time = pm.GetNextUpdateTime(true) / (60 * 1000);
  EXPECT_TRUE(next_time >= 240 && next_time <= 480);

  // 6 errors, reached max backoff.
  EXPECT_EQ(pm.GetNextUpdateTime(true), 480 * 60 * 1000);

  // 7 errors.
  EXPECT_EQ(pm.GetNextUpdateTime(true), 480 * 60 * 1000);

  // Received a successful response.
  EXPECT_EQ(pm.GetNextUpdateTime(false), 1800 * 1000);
}

// Test string combinations with and without MAC.
TEST_F(SafeBrowsingProtocolManagerTest, TestChunkStrings) {
  SafeBrowsingProtocolManager pm(NULL, kClient, kClientKey, kWrappedKey, NULL,
                                 kInfoUrlPrefix, kMacKeyUrlPrefix, false);

  // Add and Sub chunks.
  SBListChunkRanges phish("goog-phish-shavar");
  phish.adds = "1,4,6,8-20,99";
  phish.subs = "16,32,64-96";
  EXPECT_EQ(pm.FormatList(phish, false),
            "goog-phish-shavar;a:1,4,6,8-20,99:s:16,32,64-96\n");
  EXPECT_EQ(pm.FormatList(phish, true),
            "goog-phish-shavar;a:1,4,6,8-20,99:s:16,32,64-96:mac\n");

  // Add chunks only.
  phish.subs = "";
  EXPECT_EQ(pm.FormatList(phish, false),
            "goog-phish-shavar;a:1,4,6,8-20,99\n");
  EXPECT_EQ(pm.FormatList(phish, true),
            "goog-phish-shavar;a:1,4,6,8-20,99:mac\n");

  // Sub chunks only.
  phish.adds = "";
  phish.subs = "16,32,64-96";
  EXPECT_EQ(pm.FormatList(phish, false), "goog-phish-shavar;s:16,32,64-96\n");
  EXPECT_EQ(pm.FormatList(phish, true),
            "goog-phish-shavar;s:16,32,64-96:mac\n");

  // No chunks of either type.
  phish.adds = "";
  phish.subs = "";
  EXPECT_EQ(pm.FormatList(phish, false), "goog-phish-shavar;\n");
  EXPECT_EQ(pm.FormatList(phish, true), "goog-phish-shavar;mac\n");
}

TEST_F(SafeBrowsingProtocolManagerTest, TestGetHashBackOffTimes) {
  SafeBrowsingProtocolManager pm(NULL, kClient, kClientKey, kWrappedKey, NULL,
                                 kInfoUrlPrefix, kMacKeyUrlPrefix, false);

  // No errors or back off time yet.
  EXPECT_EQ(pm.gethash_error_count_, 0);
  EXPECT_TRUE(pm.next_gethash_time_.is_null());

  Time now = Time::Now();

  // 1 error.
  pm.HandleGetHashError(now);
  EXPECT_EQ(pm.gethash_error_count_, 1);
  TimeDelta margin = TimeDelta::FromSeconds(5);  // Fudge factor.
  Time future = now + TimeDelta::FromMinutes(1);
  EXPECT_TRUE(pm.next_gethash_time_ >= future - margin &&
              pm.next_gethash_time_ <= future + margin);

  // 2 errors.
  pm.HandleGetHashError(now);
  EXPECT_EQ(pm.gethash_error_count_, 2);
  EXPECT_TRUE(pm.next_gethash_time_ >= now + TimeDelta::FromMinutes(30));
  EXPECT_TRUE(pm.next_gethash_time_ <= now + TimeDelta::FromMinutes(60));

  // 3 errors.
  pm.HandleGetHashError(now);
  EXPECT_EQ(pm.gethash_error_count_, 3);
  EXPECT_TRUE(pm.next_gethash_time_ >= now + TimeDelta::FromMinutes(60));
  EXPECT_TRUE(pm.next_gethash_time_ <= now + TimeDelta::FromMinutes(120));

  // 4 errors.
  pm.HandleGetHashError(now);
  EXPECT_EQ(pm.gethash_error_count_, 4);
  EXPECT_TRUE(pm.next_gethash_time_ >= now + TimeDelta::FromMinutes(120));
  EXPECT_TRUE(pm.next_gethash_time_ <= now + TimeDelta::FromMinutes(240));

  // 5 errors.
  pm.HandleGetHashError(now);
  EXPECT_EQ(pm.gethash_error_count_, 5);
  EXPECT_TRUE(pm.next_gethash_time_ >= now + TimeDelta::FromMinutes(240));
  EXPECT_TRUE(pm.next_gethash_time_ <= now + TimeDelta::FromMinutes(480));

  // 6 errors, reached max backoff.
  pm.HandleGetHashError(now);
  EXPECT_EQ(pm.gethash_error_count_, 6);
  EXPECT_TRUE(pm.next_gethash_time_ == now + TimeDelta::FromMinutes(480));

  // 7 errors.
  pm.HandleGetHashError(now);
  EXPECT_EQ(pm.gethash_error_count_, 7);
  EXPECT_TRUE(pm.next_gethash_time_== now + TimeDelta::FromMinutes(480));
}

TEST_F(SafeBrowsingProtocolManagerTest, TestGetHashUrl) {
  SafeBrowsingProtocolManager pm(NULL, kClient, kClientKey, kWrappedKey, NULL,
                                 kInfoUrlPrefix, kMacKeyUrlPrefix, false);
  pm.version_ = kAppVer;
  EXPECT_EQ("http://info.prefix.com/foo/gethash?client=unittest&appver=1.0&"
            "pver=2.2", pm.GetHashUrl(false).spec());
  EXPECT_EQ("http://info.prefix.com/foo/gethash?client=unittest&appver=1.0&"
            "pver=2.2&wrkey=AKEgNisjLl7iRYrjWHmpd_XwCiilxrw8nNaYH47tiQ7pDe9cE"
            "ErjVHGZaPPUau5h61tbXSDqABiJZnDFByc_g8B5vTwxkhBf9g==",
            pm.GetHashUrl(true).spec());

  pm.set_additional_query(kAdditionalQuery);
  EXPECT_EQ("http://info.prefix.com/foo/gethash?client=unittest&appver=1.0&"
            "pver=2.2&additional_query",
            pm.GetHashUrl(false).spec());
  EXPECT_EQ("http://info.prefix.com/foo/gethash?client=unittest&appver=1.0&"
            "pver=2.2&additional_query&wrkey=AKEgNisjLl7iRYrjWHmpd_XwCiilxrw8"
            "nNaYH47tiQ7pDe9cEErjVHGZaPPUau5h61tbXSDqABiJZnDFByc_g8B5vTwxkhBf"
            "9g==", pm.GetHashUrl(true).spec());
}

TEST_F(SafeBrowsingProtocolManagerTest, TestUpdateUrl) {
  SafeBrowsingProtocolManager pm(NULL, kClient, kClientKey, kWrappedKey, NULL,
                                 kInfoUrlPrefix, kMacKeyUrlPrefix, false);
  pm.version_ = kAppVer;

  EXPECT_EQ("http://info.prefix.com/foo/downloads?client=unittest&appver=1.0&"
            "pver=2.2", pm.UpdateUrl(false).spec());
  EXPECT_EQ("http://info.prefix.com/foo/downloads?client=unittest&appver=1.0&"
            "pver=2.2&wrkey=AKEgNisjLl7iRYrjWHmpd_XwCiilxrw8nNaYH47tiQ7pDe9cE"
            "ErjVHGZaPPUau5h61tbXSDqABiJZnDFByc_g8B5vTwxkhBf9g==",
            pm.UpdateUrl(true).spec());

  pm.set_additional_query(kAdditionalQuery);
  EXPECT_EQ("http://info.prefix.com/foo/downloads?client=unittest&appver=1.0&"
            "pver=2.2&additional_query", pm.UpdateUrl(false).spec());
  EXPECT_EQ("http://info.prefix.com/foo/downloads?client=unittest&appver=1.0&"
            "pver=2.2&additional_query&wrkey=AKEgNisjLl7iRYrjWHmpd_XwCiilxrw8"
            "nNaYH47tiQ7pDe9cEErjVHGZaPPUau5h61tbXSDqABiJZnDFByc_g8B5vTwxkhBf"
            "9g==", pm.UpdateUrl(true).spec());
}

TEST_F(SafeBrowsingProtocolManagerTest, TestSafeBrowsingHitUrl) {
  SafeBrowsingProtocolManager pm(NULL, kClient, kClientKey, kWrappedKey, NULL,
                                 kInfoUrlPrefix, kMacKeyUrlPrefix, false);
  pm.version_ = kAppVer;

  GURL malicious_url("http://malicious.url.com");
  GURL page_url("http://page.url.com");
  GURL referrer_url("http://referrer.url.com");
  EXPECT_EQ("http://info.prefix.com/foo/report?client=unittest&appver=1.0&"
            "pver=2.2&evts=malblhit&evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1",
            pm.SafeBrowsingHitUrl(
                malicious_url, page_url, referrer_url,
                true, SafeBrowsingService::URL_MALWARE).spec());

  pm.set_additional_query(kAdditionalQuery);
  EXPECT_EQ("http://info.prefix.com/foo/report?client=unittest&appver=1.0&"
            "pver=2.2&additional_query&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0",
            pm.SafeBrowsingHitUrl(
                malicious_url, page_url, referrer_url,
                false, SafeBrowsingService::URL_PHISHING).spec());

  EXPECT_EQ("http://info.prefix.com/foo/report?client=unittest&appver=1.0&"
            "pver=2.2&additional_query&evts=binurlhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0",
            pm.SafeBrowsingHitUrl(
                malicious_url, page_url, referrer_url,
                false, SafeBrowsingService::BINARY_MALWARE_URL).spec());

  EXPECT_EQ("http://info.prefix.com/foo/report?client=unittest&appver=1.0&"
            "pver=2.2&additional_query&evts=binhashhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0",
            pm.SafeBrowsingHitUrl(
                malicious_url, page_url, referrer_url,
                false, SafeBrowsingService::BINARY_MALWARE_HASH).spec());
}

TEST_F(SafeBrowsingProtocolManagerTest, TestMalwareDetailsUrl) {
  SafeBrowsingProtocolManager pm(NULL, kClient, kClientKey, kWrappedKey, NULL,
                                 kInfoUrlPrefix, kMacKeyUrlPrefix, false);

  pm.version_ = kAppVer;
  pm.set_additional_query(kAdditionalQuery);  // AdditionalQuery is not used.
  EXPECT_EQ("https://key.prefix.com/bar/clientreport/malware?"
            "client=unittest&appver=1.0&pver=1.0",
            pm.MalwareDetailsUrl().spec());
}

TEST_F(SafeBrowsingProtocolManagerTest, TestMacKeyUrl) {
  SafeBrowsingProtocolManager pm(NULL, kClient, kClientKey, kWrappedKey, NULL,
                                 kInfoUrlPrefix, kMacKeyUrlPrefix, false);
  pm.version_ = kAppVer;

  EXPECT_EQ("https://key.prefix.com/bar/newkey?client=unittest&appver=1.0&"
            "pver=2.2", pm.MacKeyUrl().spec());

  pm.set_additional_query(kAdditionalQuery);
  EXPECT_EQ("https://key.prefix.com/bar/newkey?client=unittest&appver=1.0&"
            "pver=2.2&additional_query", pm.MacKeyUrl().spec());
}

TEST_F(SafeBrowsingProtocolManagerTest, TestNextChunkUrl) {
  SafeBrowsingProtocolManager pm(NULL, kClient, kClientKey, kWrappedKey, NULL,
                                 kInfoUrlPrefix, kMacKeyUrlPrefix, false);
  pm.version_ = kAppVer;

  std::string url_partial = "localhost:1234/foo/bar?foo";
  std::string url_http_full = "http://localhost:1234/foo/bar?foo";
  std::string url_https_full = "https://localhost:1234/foo/bar?foo";
  std::string url_https_no_query = "https://localhost:1234/foo/bar";

  EXPECT_EQ("http://localhost:1234/foo/bar?foo",
            pm.NextChunkUrl(url_partial).spec());
  EXPECT_EQ("http://localhost:1234/foo/bar?foo",
            pm.NextChunkUrl(url_http_full).spec());
  EXPECT_EQ("https://localhost:1234/foo/bar?foo",
            pm.NextChunkUrl(url_https_full).spec());
  EXPECT_EQ("https://localhost:1234/foo/bar",
            pm.NextChunkUrl(url_https_no_query).spec());

  pm.set_additional_query(kAdditionalQuery);
  EXPECT_EQ("http://localhost:1234/foo/bar?foo&additional_query",
            pm.NextChunkUrl(url_partial).spec());
  EXPECT_EQ("http://localhost:1234/foo/bar?foo&additional_query",
            pm.NextChunkUrl(url_http_full).spec());
  EXPECT_EQ("https://localhost:1234/foo/bar?foo&additional_query",
            pm.NextChunkUrl(url_https_full).spec());
  EXPECT_EQ("https://localhost:1234/foo/bar?additional_query",
            pm.NextChunkUrl(url_https_no_query).spec());
}
