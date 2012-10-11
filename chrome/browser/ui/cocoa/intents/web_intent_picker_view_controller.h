// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "ui/gfx/size.h"

class WebIntentPickerCocoa2;
@class WebIntentChooseServiceViewController;
@class WebIntentMessageViewController;
@class WebIntentProgressViewController;

// The different states a picker dialog can be in.
enum WebIntentPickerState {
  PICKER_STATE_WAITING,
  PICKER_STATE_NO_SERVICE,
  PICKER_STATE_CHOOSE_SERVICE,
  PICKER_STATE_INSTALLING_EXTENSION,
};

// Manages the web intent picker UI. The view is meant to be embedded in either
// a constrained window or a bubble.
@interface WebIntentPickerViewController : NSViewController {
 @private
  WebIntentPickerCocoa2* picker_;  // weak
  WebIntentPickerState state_;
  scoped_nsobject<NSButton> closeButton_;
  scoped_nsobject<WebIntentChooseServiceViewController>
     chooseServiceViewController_;
  scoped_nsobject<WebIntentMessageViewController>
      messageViewController_;
  scoped_nsobject<WebIntentProgressViewController>
      progressViewController_;
}

- (id)initWithPicker:(WebIntentPickerCocoa2*)picker;

- (NSButton*)closeButton;

// Get the current state.
- (WebIntentPickerState)state;

- (WebIntentChooseServiceViewController*)chooseServiceViewController;
- (WebIntentMessageViewController*)messageViewController;
- (WebIntentProgressViewController*)progressViewController;

// Update the dialog state and perform layout.
- (void)update;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_VIEW_CONTROLLER_H_
