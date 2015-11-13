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
using safe_browsing::HitReport;
using safe_browsing::ThreatSource;

static const char kUrlPrefix[] = "https://prefix.com/foo";
static const char kClient[] = "unittest";
static const char kAppVer[] = "1.0";

namespace safe_browsing {

class SafeBrowsingPingManagerTest : public testing::Test {
 protected:
  std::string key_param_;

  void SetUp() override {
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

  HitReport base_hp;
  base_hp.malicious_url = GURL("http://malicious.url.com");
  base_hp.page_url = GURL("http://page.url.com");
  base_hp.referrer_url = GURL("http://referrer.url.com");

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    hp.threat_source = ThreatSource::LOCAL_PVER3;
    hp.is_subresource = true;
    hp.is_extended_reporting = true;
    hp.is_metrics_reporting_active = true;

    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=1&evts=malblhit&evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l3&m=1",
        pm.SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::DATA_SAVER;
    hp.is_subresource = false;
    hp.is_extended_reporting = true;
    hp.is_metrics_reporting_active = true;
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=ds&m=1",
        pm.SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_BINARY_MALWARE_URL;
    hp.threat_source = ThreatSource::REMOTE;
    hp.is_extended_reporting = false;
    hp.is_metrics_reporting_active = true;
    hp.is_subresource = false;
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=0&evts=binurlhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=rem&m=1",
        pm.SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.is_extended_reporting = false;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = false;
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=0&evts=phishcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=l4&m=0",
        pm.SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.is_extended_reporting = false;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = true;
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=0&evts=malcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l4&m=0",
        pm.SafeBrowsingHitUrl(hp).spec());
  }
}

TEST_F(SafeBrowsingPingManagerTest, TestThreatDetailsUrl) {
  SafeBrowsingProtocolConfig config;
  config.client_name = kClient;
  config.url_prefix = kUrlPrefix;
  SafeBrowsingPingManager pm(NULL, config);

  pm.version_ = kAppVer;
  EXPECT_EQ("https://prefix.com/foo/clientreport/malware?"
            "client=unittest&appver=1.0&pver=1.0" + key_param_,
            pm.ThreatDetailsUrl().spec());
}

}  // namespace safe_browsing
