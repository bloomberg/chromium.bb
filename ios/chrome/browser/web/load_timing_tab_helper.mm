// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/load_timing_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#import "ios/web/public/web_state/web_state.h"

DEFINE_WEB_STATE_USER_DATA_KEY(LoadTimingTabHelper);

const char LoadTimingTabHelper::kOmnibarToPageLoadedMetric[] =
    "IOS.PageLoadTiming.OmnibarToPageLoaded";

LoadTimingTabHelper::LoadTimingTabHelper(web::WebState* web_state)
    : web::WebStateObserver(web_state) {}

LoadTimingTabHelper::~LoadTimingTabHelper() = default;

void LoadTimingTabHelper::DidInitiatePageLoad() {
  load_start_time_ = base::TimeTicks::Now();
}

void LoadTimingTabHelper::DidPromotePrerenderTab() {
  if (web_state()->IsLoading()) {
    DidInitiatePageLoad();
  } else {
    ReportLoadTime(base::TimeDelta());
    ResetTimer();
  }
}

void LoadTimingTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (!load_start_time_.is_null() &&
      load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    ReportLoadTime(base::TimeTicks::Now() - load_start_time_);
  }
  ResetTimer();
}

void LoadTimingTabHelper::ReportLoadTime(const base::TimeDelta& elapsed) {
  UMA_HISTOGRAM_TIMES(kOmnibarToPageLoadedMetric, elapsed);
}

void LoadTimingTabHelper::ResetTimer() {
  load_start_time_ = base::TimeTicks();
}
