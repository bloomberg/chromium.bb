// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_VALIDATION_MESSAGE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_VALIDATION_MESSAGE_BUBBLE_CONTROLLER_H_

#include "base/strings/string16.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

// A bubble controller implementation for ValidationMessageBubbleCocoa class.
@interface ValidationMessageBubbleController : BaseBubbleController

- (id)init:(NSWindow*)parentWindow
anchoredAt:(NSPoint)anchorPoint
  mainText:(const base::string16&)mainText
   subText:(const base::string16&)subText;

// This is exposed for testing.
+ (NSView*)constructContentView:(const base::string16&)mainText
                        subText:(const base::string16&)subText;

@end

#endif  // CHROME_BROWSER_UI_COCOA_VALIDATION_MESSAGE_BUBBLE_CONTROLLER_H_
