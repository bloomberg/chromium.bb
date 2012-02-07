// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
class WebIntentPickerModel;

// Controller for intent picker bubble. This bubble pops up whenever a web
// page invokes ActivateIntent and lets the user choose which service should
// be used to handle this action.
@interface WebIntentBubbleController : BaseBubbleController {
 @private
  // C++ <-> ObjectiveC bridge. Weak reference.
  WebIntentPickerCocoa* picker_;

  // Inline disposition tab contents. Weak reference.
  TabContentsWrapper* contents_;
}

// Initialize the window, and connect to bridge.
- (id)initWithPicker:(WebIntentPickerCocoa*)picker
        parentWindow:(NSWindow*)parent
          anchoredAt:(NSPoint)point;

// Set the contents for inline disposition intents.
- (void)setInlineDispositionTabContents:(TabContentsWrapper*)wrapper;

- (void)performLayoutWithModel:(WebIntentPickerModel*)model;

@end  // WebIntentBubbleController

#endif  // CHROME_BROWSER_UI_COCOA_WEB_INTENT_BUBBLE_CONTROLLER_H_
