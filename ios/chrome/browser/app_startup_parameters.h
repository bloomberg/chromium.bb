// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_STARTUP_PARAMETERS_H_
#define IOS_CHROME_BROWSER_APP_STARTUP_PARAMETERS_H_

#import <Foundation/Foundation.h>

class GURL;
@class XCallbackParameters;

// This class stores all the parameters relevant to the app startup in case
// of launch from another app.
@interface AppStartupParameters : NSObject

// The URL received that should be opened.
@property(nonatomic, readonly, assign) const GURL& externalURL;

// Parameters representing an x-callback-url request from another app.
// Can be nil.
@property(nonatomic, readonly, strong) XCallbackParameters* xCallbackParameters;

// Boolean to track if a voice search is requested at startup.
@property(nonatomic, readwrite, assign) BOOL launchVoiceSearch;
// Boolean to track if the app should launch in incognito mode.
@property(nonatomic, readwrite, assign) BOOL launchInIncognito;
// Boolean to track if the omnibox should be focused on startup.
@property(nonatomic, readwrite, assign) BOOL launchFocusOmnibox;
// Boolean to track if a QR scanner is requested at startup.
@property(nonatomic, readwrite, assign) BOOL launchQRScanner;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithExternalURL:(const GURL&)externalURL;

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                xCallbackParameters:(XCallbackParameters*)xCallbackParameters
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_APP_STARTUP_PARAMETERS_H_
