// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service.h"

#include <algorithm>

#include "base/command_line.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "url/gurl.h"

// static
SiteEngagementService* SiteEngagementService::Get(Profile* profile) {
  return SiteEngagementServiceFactory::GetForProfile(profile);
}

// static
bool SiteEngagementService::IsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSiteEngagementService);
}

SiteEngagementService::SiteEngagementService(Profile* profile)
    : profile_(profile) {
}

SiteEngagementService::~SiteEngagementService() {
}

void SiteEngagementService::HandleNavigation(const GURL& url) {
  GURL origin = url.GetOrigin();
  scores_[origin] = scores_[origin] + 1;
}

int SiteEngagementService::GetScore(const GURL& url) {
  return scores_[url.GetOrigin()];
}

