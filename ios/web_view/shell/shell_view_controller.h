// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_SHELL_SHELL_VIEW_CONTROLLER_H_
#define IOS_WEB_VIEW_SHELL_SHELL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/web_view/public/cwv_ui_delegate.h"
#import "ios/web_view/public/cwv_web_view_delegate.h"

// Accessibility identifier added to the text field of JavaScript prompts.
extern NSString* const
    kWebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier;

// Implements the main UI for web_view_shell, including the toolbar and web
// view.
@interface ShellViewController : UIViewController

@end

#endif  // IOS_WEB_VIEW_SHELL_SHELL_VIEW_CONTROLLER_H_
