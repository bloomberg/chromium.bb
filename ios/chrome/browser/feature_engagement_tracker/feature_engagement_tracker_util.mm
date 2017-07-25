// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/feature_engagement_tracker/feature_engagement_tracker_util.h"

#include "components/feature_engagement_tracker/public/event_constants.h"
#include "components/feature_engagement_tracker/public/feature_engagement_tracker.h"
#include "ios/chrome/browser/feature_engagement_tracker/feature_engagement_tracker_factory.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"

namespace feature_engagement_tracker {

void NotifyNewTabEvent(ios::ChromeBrowserState* browserState,
                       bool isIncognito) {
  const char* const event =
      isIncognito ? feature_engagement_tracker::events::kIncognitoTabOpened
                  : feature_engagement_tracker::events::kNewTabOpened;
  FeatureEngagementTrackerFactory::GetForBrowserState(browserState)
      ->NotifyEvent(std::string(event));
}

void NotifyNewTabEventForCommand(ios::ChromeBrowserState* browserState,
                                 OpenNewTabCommand* command) {
  if (command.isUserInitiated) {
    NotifyNewTabEvent(browserState, command.incognito);
  }
}

}  // namespace feature_engagement_tracker
