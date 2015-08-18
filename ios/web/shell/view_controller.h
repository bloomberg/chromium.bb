// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_VIEW_CONTROLLER_H_
#define IOS_WEB_SHELL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/web/public/web_state/ui/crw_web_delegate.h"

namespace web {
class BrowserState;
}

// Implements the main UI for ios_web_shell, including a toolbar and web view.
@interface ViewController
    : UIViewController<CRWWebDelegate, UITextFieldDelegate>

@property(nonatomic, retain) IBOutlet UIView* containerView;
@property(nonatomic, retain) IBOutlet UIToolbar* toolbarView;

// Initializes a new ViewController from |MainView.xib| using the given
// |browserState|.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState;

@end

#endif  // IOS_WEB_SHELL_VIEW_CONTROLLER_H_
