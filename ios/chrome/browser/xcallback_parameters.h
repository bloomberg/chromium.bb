// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_XCALLBACK_PARAMETERS_H_
#define IOS_CHROME_BROWSER_XCALLBACK_PARAMETERS_H_

#import <Foundation/Foundation.h>

// This class contains the defining parameters for an XCallback request from
// another app.
@interface XCallbackParameters : NSObject<NSCoding, NSCopying>

// The id of the calling app.
@property(nonatomic, readonly, copy) NSString* sourceAppId;

// Designated initializer. |sourceAppId| is the string identifier for the app
// that launched Chrome and cannot be nil.
- (instancetype)initWithSourceAppId:(NSString*)sourceAppId
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_XCALLBACK_PARAMETERS_H_
