// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_TAB_USAGE_RECORDER_DELEGATE_H_
#define IOS_CHROME_BROWSER_METRICS_TAB_USAGE_RECORDER_DELEGATE_H_

#import <Foundation/Foundation.h>

// A delegate which provides to the TabUsageRecorder a count of how many alive
// tabs it is monitoring.
@protocol TabUsageRecorderDelegate

// A count of how many alive tabs the TabUsageRecorder is monitoring.
// NOTE: This should be used for metrics-gathering only; for any other purpose
// callers should not know or care how many tabs are alive.
- (NSUInteger)liveTabsCount;

@end

#endif  // IOS_CHROME_BROWSER_METRICS_TAB_USAGE_RECORDER_DELEGATE_H_
