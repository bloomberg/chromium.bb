// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_BACKGROUND_BUDGET_SERVICE_H_
#define CHROME_BROWSER_PUSH_MESSAGING_BACKGROUND_BUDGET_SERVICE_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;

namespace base {
class Clock;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A budget service to help Chrome decide how much background work a service
// worker should be able to do on behalf of the user. The budget is calculated
// based on the Site Engagment Score and is consumed when a service worker does
// background work on behalf of the user.
class BackgroundBudgetService : public KeyedService {
 public:
  explicit BackgroundBudgetService(Profile* profile);
  ~BackgroundBudgetService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  enum class CostType {
    // The cost of silencing a push message.
    SILENT_PUSH = 0,
  };

  // Query for the base cost for any background processing.
  static double GetCost(CostType type);

  // Get the budget associated with the origin. This is returned as the double
  // budget. Budget will be a value between 0.0 and
  // SiteEngagementScore::kMaxPoints.
  double GetBudget(const GURL& origin);

  // Store the budget associated with the origin. Budget should be a value
  // between 0.0 and SiteEngagementScore::kMaxPoints.
  void StoreBudget(const GURL& origin, double budget);

 private:
  friend class BackgroundBudgetServiceTest;

  // Used to allow tests to fast forward/reverse time.
  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

  // The clock used to vend times.
  std::unique_ptr<base::Clock> clock_;

  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(BackgroundBudgetService);
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_BACKGROUND_BUDGET_SERVICE_H_
