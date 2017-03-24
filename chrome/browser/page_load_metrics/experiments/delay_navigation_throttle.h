// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_EXPERIMENTS_DELAY_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_EXPERIMENTS_DELAY_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "content/public/browser/navigation_throttle.h"

// This feature controls whether the DelayNavigationThrottle should be
// instantiated.
extern const base::Feature kDelayNavigationFeature;

// DelayNavigationThrottle introduces a delay to main frame navigations.
class DelayNavigationThrottle : public content::NavigationThrottle {
 public:
  // The duration of the delay to add to the start of each navigation, in
  // milliseconds.
  static const char kParamDelayNavigationDurationMillis[];

  // Whether the delay should be randomized in the interval 0ms and
  // kParamDelayNavigationDurationMillis.
  static const char kParamDelayNavigationRandomize[];

  // The probability that the navigation delay should be introduced. Should be a
  // double in the interval [0, 1], where a value of 0 indicates the delay
  // should never be introduced, and a value of 1 indicates the delay should
  // always be introduced.
  static const char kParamDelayNavigationProbability[];

  // Creates a DelayNavigationThrottle if the DelayNavigation feature is
  // enabled and configured.
  static std::unique_ptr<DelayNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  // Creates a DelayNavigationThrottle directly. Only intended for use in
  // tests. Production code should use MaybeCreateThrottleFor.
  DelayNavigationThrottle(content::NavigationHandle* handle,
                          scoped_refptr<base::TaskRunner> task_runner,
                          base::TimeDelta navigation_delay);
  ~DelayNavigationThrottle() override;

  // Return the navigation delay for this throttle.
  base::TimeDelta navigation_delay() const { return navigation_delay_; }

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;

 private:
  void OnDelayComplete();

  const scoped_refptr<base::TaskRunner> task_runner_;
  const base::TimeDelta navigation_delay_;

  // This has to be the last member of the class.
  base::WeakPtrFactory<DelayNavigationThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DelayNavigationThrottle);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_EXPERIMENTS_DELAY_NAVIGATION_THROTTLE_H_
