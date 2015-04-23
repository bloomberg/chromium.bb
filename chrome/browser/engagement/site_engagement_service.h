// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_

#include <map>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class Profile;

// Stores and retrieves the engagement score of an origin.
//
// An engagement score is a positive integer that represents how much a user has
// engaged with an origin - the higher it is, the more engagement the user has
// had with this site recently.
//
// Positive user activity, such as visiting the origin often and adding it to
// the homescreen, will increase the site engagement score. Negative activity,
// such as rejecting permission prompts or not responding to notifications, will
// decrease the site engagement score.
class SiteEngagementService : public KeyedService {
 public:
  static SiteEngagementService* Get(Profile* profile);

  // Returns whether or not the SiteEngagementService is enabled.
  static bool IsEnabled();

  explicit SiteEngagementService(Profile* profile);
  ~SiteEngagementService() override;

  // Update the karma score of the origin matching |url| for user navigation.
  void HandleNavigation(const GURL& url);

  // Returns a non-negative integer representing the engagement score of the
  // origin for this URL.
  int GetScore(const GURL& url);

 private:
  Profile* profile_;

  // Temporary non-persistent score database for testing.
  std::map<GURL, int> scores_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementService);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_
