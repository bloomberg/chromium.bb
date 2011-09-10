// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_GLOBAL_ERROR_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_GLOBAL_ERROR_BUBBLE_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

class GlobalError;

// This is a bubble view shown from the wrench menu to display information
// about a global error.
@interface GlobalErrorBubbleController : BaseBubbleController {
 @private
  GlobalError* error_;

  IBOutlet NSImageView* iconView_;
  IBOutlet NSTextField* title_;
  IBOutlet NSTextField* message_;
  IBOutlet NSButton* acceptButton_;
  IBOutlet NSButton* cancelButton_;
}

- (IBAction)onAccept:(id)sender;
- (IBAction)onCancel:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_GLOBAL_ERROR_BUBBLE_CONTROLLER_H_
