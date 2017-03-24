// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/experiments/delay_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

base::TimeDelta GetNavigationDelayFromParams() {
  double delay_probability = base::GetFieldTrialParamByFeatureAsDouble(
      kDelayNavigationFeature,
      DelayNavigationThrottle::kParamDelayNavigationProbability,
      0 /* default value */);

  DCHECK_GE(delay_probability, 0.0);
  DCHECK_LE(delay_probability, 1.0);
  if (delay_probability == 0 || delay_probability < base::RandDouble())
    return base::TimeDelta();

  int navigation_delay_millis = base::GetFieldTrialParamByFeatureAsInt(
      kDelayNavigationFeature,
      DelayNavigationThrottle::kParamDelayNavigationDurationMillis,
      -1 /* default value */);

  if (navigation_delay_millis <= 0)
    return base::TimeDelta();

  bool randomize_delay = base::GetFieldTrialParamByFeatureAsBool(
      kDelayNavigationFeature,
      DelayNavigationThrottle::kParamDelayNavigationRandomize,
      false /* default value */);

  if (randomize_delay) {
    // RandGenerator produces a value in [0, navigation_delay_millis). We want
    // a value in [1, navigation_delay_millis].
    navigation_delay_millis = base::RandGenerator(navigation_delay_millis) + 1;
  }

  return base::TimeDelta::FromMilliseconds(navigation_delay_millis);
}

}  // namespace

const base::Feature kDelayNavigationFeature{"DelayNavigation",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const char DelayNavigationThrottle::kParamDelayNavigationDurationMillis[] =
    "delay_millis";

const char DelayNavigationThrottle::kParamDelayNavigationRandomize[] =
    "randomize_delay";

const char DelayNavigationThrottle::kParamDelayNavigationProbability[] =
    "delay_probability";

// static
std::unique_ptr<DelayNavigationThrottle>
DelayNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame() ||
      !base::FeatureList::IsEnabled(kDelayNavigationFeature) ||
      !handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return nullptr;
  }

  // Do not delay the NTP, which in some cases has an HTTPS URL.
  if (search::IsNTPURL(handle->GetURL(),
                       Profile::FromBrowserContext(
                           handle->GetWebContents()->GetBrowserContext()))) {
    return nullptr;
  }

  base::TimeDelta navigation_delay = GetNavigationDelayFromParams();
  if (navigation_delay.is_zero())
    return nullptr;

  return base::MakeUnique<DelayNavigationThrottle>(
      handle, base::ThreadTaskRunnerHandle::Get(), navigation_delay);
}

DelayNavigationThrottle::DelayNavigationThrottle(
    content::NavigationHandle* handle,
    scoped_refptr<base::TaskRunner> task_runner,
    base::TimeDelta navigation_delay)
    : content::NavigationThrottle(handle),
      task_runner_(task_runner),
      navigation_delay_(navigation_delay),
      weak_ptr_factory_(this) {}

DelayNavigationThrottle::~DelayNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
DelayNavigationThrottle::WillStartRequest() {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DelayNavigationThrottle::OnDelayComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      navigation_delay_);
  return content::NavigationThrottle::DEFER;
}

void DelayNavigationThrottle::OnDelayComplete() {
  navigation_handle()->Resume();
}
