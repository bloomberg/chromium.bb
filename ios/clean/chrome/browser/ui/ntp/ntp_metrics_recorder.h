// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_METRICS_RECORDER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/metrics/metrics_recorder.h"

class PrefService;

// Metrics Recorder for the NTP.
@interface NTPMetricsRecorder : NSObject<MetricsRecorder>

// Initializes this MetricsRecorder. |prefs| should be the PrefService used
// to store NTP-relevant prefs and must live for the lifetime of this object.
- (instancetype)initWithPrefService:(PrefService*)prefs
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_METRICS_RECORDER_H_
