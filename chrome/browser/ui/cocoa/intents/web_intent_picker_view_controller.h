// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "ui/gfx/size.h"

class WebIntentPickerCocoa2;

// Manages the web intent picker UI. The view is meant to be embedded in either
// a constrained window or a bubble.
@interface WebIntentPickerViewController : NSViewController {
 @private
  WebIntentPickerCocoa2* picker_;  // weak
  scoped_nsobject<NSButton> closeButton_;
}

- (id)initWithPicker:(WebIntentPickerCocoa2*)picker;

- (NSButton*)closeButton;

// Update the dialog state and perform layout.
- (void)update;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_VIEW_CONTROLLER_H_
