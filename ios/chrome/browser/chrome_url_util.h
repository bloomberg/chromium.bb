// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_URL_UTIL_H_
#define IOS_CHROME_BROWSER_CHROME_URL_UTIL_H_

#import <Foundation/Foundation.h>

#include <string>

class GURL;

// Returns whether |url| is an external file reference.
bool UrlIsExternalFileReference(const GURL& url);

// Returns a URL that launches Chrome.
NSURL* UrlToLaunchChrome();

// Returns the URL for the iOS App Icon for Chrome.
NSURL* UrlOfChromeAppIcon(int width, int height);

// Returns true if the scheme has a chrome scheme.
bool UrlHasChromeScheme(const GURL& url);
bool UrlHasChromeScheme(NSURL* url);

// Returns true if |scheme| is handled in Chrome, or by default handlers in
// net::URLRequest.
bool IsHandledProtocol(const std::string& scheme);

// Singleton object that generates constants for Chrome iOS applications.
// Behavior of this object can be overridden by unit tests.
@interface ChromeAppConstants : NSObject

// Class method returning the singleton instance.
+ (ChromeAppConstants*)sharedInstance;

// Returns the URL scheme that launches Chrome.
- (NSString*)getBundleURLScheme;

// Returns the URL string to the Chrome application icon.
- (NSString*)getChromeAppIconURLOfWidth:(int)width height:(int)height;

// Method to set the scheme to callback Chrome iOS for testing.
- (void)setCallbackSchemeForTesting:(NSString*)callbackScheme;

// Method to set a different block to provider the App Icon URL.
typedef NSString* (^AppIconURLProvider)(int, int);
- (void)setAppIconURLProviderForTesting:(AppIconURLProvider)block;

@end

#endif  // IOS_CHROME_BROWSER_CHROME_URL_UTIL_H_
