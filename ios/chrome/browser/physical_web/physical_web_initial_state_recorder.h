// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHYSICAL_WEB_PHYSICAL_WEB_INITIAL_STATE_RECORDER_H_
#define IOS_CHROME_BROWSER_PHYSICAL_WEB_PHYSICAL_WEB_INITIAL_STATE_RECORDER_H_

#import <CoreBluetooth/CoreBluetooth.h>

class PrefService;

@interface PhysicalWebInitialStateRecorder : NSObject<CBCentralManagerDelegate>

- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Fetches and logs the current state of settings relevant to the Physical Web
// feature.
- (void)collectAndRecordState;

// If the initial state has not yet been recorded, abort collection and
// invalidate the timer.
- (void)invalidate;

@end

#endif  // IOS_CHROME_BROWSER_PHYSICAL_WEB_PHYSICAL_WEB_INITIAL_STATE_RECORDER_H_
