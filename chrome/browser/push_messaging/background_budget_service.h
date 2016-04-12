// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_BACKGROUND_BUDGET_SERVICE_H_
#define CHROME_BROWSER_PUSH_MESSAGING_BACKGROUND_BUDGET_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// A budget service to help Chrome decide how much background work a service
// worker should be able to do on behalf of the user. The budget currently
// implements a grace period of 1 non-visual notification in 10.
class BackgroundBudgetService {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Get the budget associated with the origin. This currently tracks a string
  // which holds 0 and 1 for the last 10 push messages and whether they
  // triggered a visual notification.
  static std::string GetBudget(Profile* profile, const GURL& origin);

  // Store the budget associated with the origin. notifications_shown is
  // expected to be a string encoding whether the last 10 push messages
  // triggered
  // a visual notification.
  static void StoreBudget(Profile* profile,
                          const GURL& origin,
                          const std::string& notifications_shown);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BackgroundBudgetService);
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_BACKGROUND_BUDGET_SERVICE_H_
