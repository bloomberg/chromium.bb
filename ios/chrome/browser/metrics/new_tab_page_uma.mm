// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/new_tab_page_uma.h"

#include "base/metrics/histogram_macros.h"
#include "components/google/core/browser/google_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#include "url/gurl.h"

namespace new_tab_page_uma {

bool IsCurrentlyOnNTP(ios::ChromeBrowserState* browserState) {
  TabModel* tabModel = GetLastActiveTabModelForChromeBrowserState(browserState);
  return tabModel.currentTab &&
         tabModel.currentTab.url == GURL(kChromeUINewTabURL);
}

void RecordAction(ios::ChromeBrowserState* browserState, ActionType type) {
  DCHECK(browserState);
  if (!IsCurrentlyOnNTP(browserState) || browserState->IsOffTheRecord())
    return;
  base::HistogramBase* counter = base::Histogram::FactoryGet(
      "NewTabPage.ActioniOS", 0, NUM_ACTION_TYPES, NUM_ACTION_TYPES + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(type);
}

void RecordActionFromOmnibox(ios::ChromeBrowserState* browserState,
                             const GURL& url,
                             ui::PageTransition transition) {
  ui::PageTransition coreTransition = static_cast<ui::PageTransition>(
      transition & ui::PAGE_TRANSITION_CORE_MASK);
  if (PageTransitionCoreTypeIs(coreTransition, ui::PAGE_TRANSITION_GENERATED)) {
    RecordAction(browserState, ACTION_SEARCHED_USING_OMNIBOX);
  } else {
    if (google_util::IsGoogleHomePageUrl(GURL(url))) {
      RecordAction(browserState, ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE);
    } else {
      RecordAction(browserState, ACTION_NAVIGATED_USING_OMNIBOX);
    }
  }
}
}
