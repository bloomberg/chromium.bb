// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WEB_INTENT_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_WEB_INTENT_BUBBLE_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"

class WebIntentPickerCocoa;

// Controller for intent picker bubble. This bubble pops up whenever a web
// page invokes ActivateIntent and lets the user choose which service should
// be used to handle this action.
@interface WebIntentBubbleController : BaseBubbleController {
 @private
  // Images for all icons shown in bubble.
  scoped_nsobject<NSPointerArray> iconImages_;

  // URLs associated with the individual services.
  scoped_nsobject<NSArray> serviceURLs_;

  // Default icon to use if no icon is specified.
  scoped_nsobject<NSImage> defaultIcon_;

  // C++ <-> ObjectiveC bridge. Weak reference.
  WebIntentPickerCocoa* picker_;
}

// Initialize the window, and connect to bridge.
- (id)initWithPicker:(WebIntentPickerCocoa*)picker
        parentWindow:(NSWindow*)parent
          anchoredAt:(NSPoint)point;

// Replaces the |image| for service at |index|.
- (void)replaceImageAtIndex:(size_t)index withImage:(NSImage*)image;

// Set the service |urls| for all services.
- (void)setServiceURLs:(NSArray*)urls;

@end  // WebIntentBubbleController

#endif  // CHROME_BROWSER_UI_COCOA_WEB_INTENT_BUBBLE_CONTROLLER_H_
