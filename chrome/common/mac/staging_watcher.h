// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_STAGING_WATCHER_H_
#define CHROME_COMMON_MAC_STAGING_WATCHER_H_

#import <Foundation/Foundation.h>

// Chrome update works by staging a copy of Chrome near to the current bundle,
// and then applying it when Chrome restarts to apply the update. Currently,
// this state of "update pending" is indicated outside of Keystone by a key in
// the CFPreferences.

@interface CrStagingKeyWatcher : NSObject

- (instancetype)init;

// Sleeps until the staging key is clear. If there is no staging key set,
// returns immediately.
- (void)waitForStagingKey;

@end

@interface CrStagingKeyWatcher (TestingInterface)

- (instancetype)initWithUserDefaults:(NSUserDefaults*)defaults;

- (void)disableKVOForTesting;

- (BOOL)lastWaitWasBlockedForTesting;

+ (NSString*)stagingKeyForTesting;

@end

#endif  // CHROME_COMMON_MAC_STAGING_WATCHER_H_
