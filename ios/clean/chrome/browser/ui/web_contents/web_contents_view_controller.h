// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_CONTENTS_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_CONTENTS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace web {
class WebState;
}

// A view controller for displaying a web view with no other controls or
// decoration.
@interface WebContentsViewController : UIViewController

// Designated initializer. |webState| must not be null.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aCoder NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// The webState the receiver was initialized with.
@property(nonatomic, readonly) web::WebState* webState;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_CONTENTS_VIEW_CONTROLLER_H_
