// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"

using SiteEngagementServiceTest = BrowserWithTestWindowTest;

// Tests that the Site Engagement score is initially 0, and increments by 1 on
// each page request.
TEST_F(SiteEngagementServiceTest, ScoreIncrementsOnPageRequest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSiteEngagementService);

  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile());
  DCHECK(service);

  GURL url("http://www.google.com/");

  AddTab(browser(), GURL("about:blank"));
  EXPECT_EQ(0, service->GetScore(url));

  for (int i = 0; i < 10; ++i) {
    NavigateAndCommitActiveTab(url);
    EXPECT_EQ(i + 1, service->GetScore(url));
  }
}
