// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_XCALLBACK_PARAMETERS_H_
#define IOS_CHROME_BROWSER_XCALLBACK_PARAMETERS_H_

#import <Foundation/Foundation.h>

#include "url/gurl.h"

// This class contains the defining parameters for an XCallback request from
// another app.
@interface XCallbackParameters : NSObject<NSCoding, NSCopying>

// The id of the calling app.
@property(nonatomic, readonly, copy) NSString* sourceAppId;

// The user visible name of the calling app. Can be nil.
@property(nonatomic, readonly, copy) NSString* sourceAppName;

// x-callback-url::x-success URL. If the app is opened using a x-callback-url
// compliant URL, the value of this parameter is used as callback URL when the
// user taps the back button.
@property(nonatomic, readonly) const GURL& successURL;

// Flag to force the creation of a new tab. Default YES.
@property(nonatomic, readonly) BOOL createNewTab;

- (instancetype)initWithSourceAppId:(NSString*)sourceAppId
                      sourceAppName:(NSString*)sourceAppName
                         successURL:(const GURL&)successURL
                       createNewTab:(BOOL)createNewTab
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_XCALLBACK_PARAMETERS_H_
