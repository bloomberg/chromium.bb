// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WEB_INTENT_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_WEB_INTENT_SHEET_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"

class WebIntentPickerCocoa;
class WebIntentPickerModel;

@class IntentView;
@class WaitingView;

// Controller for intent picker constrained dialog. This dialog pops up
// whenever a web page invokes ActivateIntent and lets the user choose which
// service should be used to handle this action.
@interface WebIntentPickerSheetController : NSWindowController {
 @private
  // C++ <-> ObjectiveC bridge. Weak reference.
  WebIntentPickerCocoa* picker_;

  // Inline disposition tab contents. Weak reference.
  TabContents* contents_;

  // The intent picker data to be rendered. Weak reference.
  WebIntentPickerModel* model_;

  scoped_nsobject<WaitingView> waitingView_;
  scoped_nsobject<NSTextField> actionTextField_;
  scoped_nsobject<IntentView> intentView_;
  scoped_nsobject<NSButton> closeButton_;
  scoped_nsobject<NSView> flipView_;
  scoped_nsobject<NSTextField> inlineDispositionTitleField_;
}
- (IBAction)installExtension:(id)sender;

// Initialize the constrained dialog, and connect to picker.
- (id)initWithPicker:(WebIntentPickerCocoa*)picker;

// Set the contents for inline disposition intents.
- (void)setInlineDispositionTabContents:(TabContents*)tabContents;

// Set the size of the inline disposition view.
- (void)setInlineDispositionFrameSize:(NSSize)size;

- (void)performLayoutWithModel:(WebIntentPickerModel*)model;

// Sets the action string of the picker, e.g.,
// "Which service should be used for sharing?".
- (void)setActionString:(NSString*)actionString;

// Sets the service title for inline disposition.
- (void)setInlineDispositionTitle:(NSString*)title;

// Stop displaying throbber. Called when extension isntallation is complete.
- (void)stopThrobber;

// Close the current sheet (and by extension, the constrained dialog).
- (void)closeSheet;

// Notification handler - called when sheet has been closed.
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end  // WebIntentPickerSheetController

#endif  // CHROME_BROWSER_UI_COCOA_WEB_INTENT_SHEET_CONTROLLER_H_
