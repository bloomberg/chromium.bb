// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_BROWSING_DATA_MANAGERS_CRW_BROWSING_DATA_MANAGER_H_
#define IOS_WEB_BROWSING_DATA_MANAGERS_CRW_BROWSING_DATA_MANAGER_H_

#import <Foundation/Foundation.h>

// A protocol implemented by a class that can manage browsing data between
// BrowserStates. Callers of these methods must do so on a background thread.
@protocol CRWBrowsingDataManager<NSObject>

// Removes browsing data at the associated BrowserState's state path.
- (void)removeDataAtStashPath;

// Removes browsing data at the canonical path that a web view stores its data.
- (void)removeDataAtCanonicalPath;

// Stashes browsing data to the associated BrowserState's state path.
- (void)stashData;

// Restores browsing data from the associated BrowserState's state path.
- (void)restoreData;

@end

#endif  // IOS_WEB_BROWSING_DATA_MANAGERS_CRW_BROWSING_DATA_MANAGER_H_
