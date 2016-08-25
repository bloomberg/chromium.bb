// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BUDGET_SERVICE_BUDGET_MANAGER_H_
#define CHROME_BROWSER_BUDGET_SERVICE_BUDGET_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/WebKit/public/platform/modules/budget_service/budget_service.mojom.h"
#include "url/gurl.h"

class Profile;

namespace base {
class Clock;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A budget manager to help Chrome decide how much background work a service
// worker should be able to do on behalf of the user. The budget is calculated
// based on the Site Engagment Score and is consumed when a origin does
// background work on behalf of the user.
class BudgetManager : public KeyedService {
 public:
  explicit BudgetManager(Profile* profile);
  ~BudgetManager() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Query for the base cost for any background processing.
  static double GetCost(blink::mojom::BudgetOperationType type);

  using GetBudgetCallback = base::Callback<void(double budget)>;

  // Get the budget associated with the origin. This is passed to the
  // callback. Budget will be a value between 0.0 and
  // SiteEngagementScore::kMaxPoints.
  void GetBudget(const GURL& origin, const GetBudgetCallback& callback);

  // Store the budget associated with the origin. Budget should be a value
  // between 0.0 and SiteEngagementScore::kMaxPoints. closure will be called
  // when the store completes.
  void StoreBudget(const GURL& origin,
                   double budget,
                   const base::Closure& closure);

 private:
  friend class BudgetManagerTest;

  // Used to allow tests to fast forward/reverse time.
  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

  // The clock used to vend times.
  std::unique_ptr<base::Clock> clock_;

  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(BudgetManager);
};

#endif  // CHROME_BROWSER_BUDGET_SERVICE_BUDGET_MANAGER_H_
