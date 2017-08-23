// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/captive_portal/captive_portal_login_view_controller_delegate.h"

class GURL;

// View controller which displays a web view used to complete the connection to
// a captive portal network.
@interface CaptivePortalLoginViewController : UIViewController

// This login view controller's delegate.
@property(nonatomic, weak) id<CaptivePortalLoginViewControllerDelegate>
    delegate;

// Initializes the login web view and navigates to |landingURL|. |landingURL|
// is the web page which allows the user to complete their connection to the
// network.
- (instancetype)initWithLandingURL:(const GURL&)landingURL
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_VIEW_CONTROLLER_H_
