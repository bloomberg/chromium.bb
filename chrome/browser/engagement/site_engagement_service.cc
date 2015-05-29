// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service.h"

#include <algorithm>
#include <cmath>

#include "base/command_line.h"
#include "base/time/clock.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "url/gurl.h"

const double SiteEngagementScore::kMaxPoints = 100;
const double SiteEngagementScore::kMaxPointsPerDay = 5;
const double SiteEngagementScore::kNavigationPoints = 1;
const int SiteEngagementScore::kDecayPeriodInDays = 7;
const double SiteEngagementScore::kDecayPoints = 5;

SiteEngagementScore::SiteEngagementScore() : SiteEngagementScore(nullptr) {
}

SiteEngagementScore::SiteEngagementScore(base::Clock* clock)
    : clock_(clock), raw_score_(0), added_today_(0), last_engagement_() {
}

SiteEngagementScore::~SiteEngagementScore() {
}

double SiteEngagementScore::Score() const {
  return DecayedScore();
}

void SiteEngagementScore::AddPoints(double points) {
  // As the score is about to be updated, commit any decay that has happened
  // since the last update.
  raw_score_ = DecayedScore();

  base::Time now = Now();
  if (!last_engagement_.is_null() &&
      now.LocalMidnight() != last_engagement_.LocalMidnight()) {
    added_today_ = 0;
  }

  double to_add =
      std::min(kMaxPoints - raw_score_, kMaxPointsPerDay - added_today_);
  to_add = std::min(to_add, points);

  added_today_ += to_add;
  raw_score_ += to_add;

  last_engagement_ = now;
}

double SiteEngagementScore::DecayedScore() const {
  // Note that users can change their clock, so from this system's perspective
  // time can go backwards. If that does happen and the system detects that the
  // current day is earlier than the last engagement, no decay (or growth) is
  // applied.
  int days_since_engagement = (Now() - last_engagement_).InDays();
  if (days_since_engagement < 0)
    return raw_score_;

  int periods = days_since_engagement / kDecayPeriodInDays;
  double decayed_score = raw_score_ - periods * kDecayPoints;
  return std::max(0.0, decayed_score);
}

base::Time SiteEngagementScore::Now() const {
  if (clock_)
    return clock_->Now();

  return base::Time::Now();
}

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
  scores_[origin].AddPoints(SiteEngagementScore::kNavigationPoints);
}

int SiteEngagementService::GetScore(const GURL& url) {
  return scores_[url.GetOrigin()].Score();
}

