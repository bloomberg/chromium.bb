// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_EVENT_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_EVENT_CONSTANTS_H_

#include "build/build_config.h"

namespace feature_engagement_tracker {

namespace events {

#if defined(OS_WIN) || defined(OS_LINUX)
// All the events declared below are the string names
// of deferred onboarding events for the New Tab.

// The user has interacted with the omnibox.
extern const char kOmniboxInteraction[];

// All the events declared below are the string names
// of deferred onboarding events for the Incognito Window

// The user has deleted browsing history.
extern const char kHistoryDeleted[];
// The user has opened an incognito window.
extern const char kIncognitoWindowOpened[];

// All the events declared below are the string names
// of common deferred onboarding events

// The user has accumulated 2 hours of active session time (one-off event).
extern const char kSessionTime[];

#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_IOS)
// This event is included in the deferred onboarding events for the New Tab
// described above, but it is also used on iOS, so it must be compiled
// separately.

// The user has opened a new tab.
extern const char kNewTabOpened[];

#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_IOS)

#if defined(OS_IOS)

// The user has opened Chrome (cold start or from background).
extern const char kChromeOpened[];

// The user has opened an incognito tab.
extern const char kIncognitoTabOpened[];

// The user has cleared their browsing data.
extern const char kClearedBrowsingData[];

// The user has viewed their reading list.
extern const char kViewedReadingList[];

#endif  // defined(OS_IOS)

}  // namespace events

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_EVENT_CONSTANTS_H_
