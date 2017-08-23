// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_IOS_CAPTIVE_PORTAL_BLOCKING_PAGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_SSL_IOS_CAPTIVE_PORTAL_BLOCKING_PAGE_DELEGATE_H_

#import <Foundation/Foundation.h>

class IOSCaptivePortalBlockingPage;
class GURL;

// Delegate for IOSCaptivePortalBlockingPage.
@protocol IOSCaptivePortalBlockingPageDelegate<NSObject>

// Notifies the delegate the user selected the connect button on the captive
// portal blocking page and that the page with the specified |landingURL| should
// be displayed to the user.
- (void)captivePortalBlockingPage:(IOSCaptivePortalBlockingPage*)blockingPage
            connectWithLandingURL:(GURL)landingURL;

@end

#endif  // IOS_CHROME_BROWSER_SSL_IOS_CAPTIVE_PORTAL_BLOCKING_PAGE_DELEGATE_H_
