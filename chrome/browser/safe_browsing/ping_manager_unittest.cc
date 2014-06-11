// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

static const char kUrlPrefix[] = "https://prefix.com/foo";
static const char kClient[] = "unittest";
static const char kAppVer[] = "1.0";

class SafeBrowsingPingManagerTest : public testing::Test {
 protected:
  std::string key_param_;

  virtual void SetUp() {
    std::string key = google_apis::GetAPIKey();
    if (!key.empty()) {
      key_param_ = base::StringPrintf(
          "&key=%s",
          net::EscapeQueryParamValue(key, true).c_str());
    }
  }
};

TEST_F(SafeBrowsingPingManagerTest, TestSafeBrowsingHitUrl) {
  SafeBrowsingProtocolConfig config;
  config.client_name = kClient;
  config.url_prefix = kUrlPrefix;
  SafeBrowsingPingManager pm(NULL, config);

  pm.version_ = kAppVer;

  GURL malicious_url("http://malicious.url.com");
  GURL page_url("http://page.url.com");
  GURL referrer_url("http://referrer.url.com");
  EXPECT_EQ("https://prefix.com/foo/report?client=unittest&appver=1.0&"
            "pver=3.0" + key_param_ +
            "&evts=malblhit&evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1",
            pm.SafeBrowsingHitUrl(
                malicious_url, page_url, referrer_url,
                true, SB_THREAT_TYPE_URL_MALWARE).spec());

  EXPECT_EQ("https://prefix.com/foo/report?client=unittest&appver=1.0&"
            "pver=3.0" + key_param_ + "&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0",
            pm.SafeBrowsingHitUrl(
                malicious_url, page_url, referrer_url,
                false, SB_THREAT_TYPE_URL_PHISHING).spec());

  EXPECT_EQ("https://prefix.com/foo/report?client=unittest&appver=1.0&"
            "pver=3.0" + key_param_ + "&evts=binurlhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0",
            pm.SafeBrowsingHitUrl(
                malicious_url, page_url, referrer_url,
                false, SB_THREAT_TYPE_BINARY_MALWARE_URL).spec());

  EXPECT_EQ("https://prefix.com/foo/report?client=unittest&appver=1.0&"
            "pver=3.0" + key_param_ + "&evts=phishcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0",
            pm.SafeBrowsingHitUrl(
                malicious_url, page_url, referrer_url,
                false, SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL).spec());

    EXPECT_EQ("https://prefix.com/foo/report?client=unittest&appver=1.0&"
            "pver=3.0" + key_param_ + "&evts=malcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1",
            pm.SafeBrowsingHitUrl(
                malicious_url, page_url, referrer_url,
                true, SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL).spec());
}

TEST_F(SafeBrowsingPingManagerTest, TestMalwareDetailsUrl) {
  SafeBrowsingProtocolConfig config;
  config.client_name = kClient;
  config.url_prefix = kUrlPrefix;
  SafeBrowsingPingManager pm(NULL, config);

  pm.version_ = kAppVer;
  EXPECT_EQ("https://prefix.com/foo/clientreport/malware?"
            "client=unittest&appver=1.0&pver=1.0" + key_param_,
            pm.MalwareDetailsUrl().spec());
}
