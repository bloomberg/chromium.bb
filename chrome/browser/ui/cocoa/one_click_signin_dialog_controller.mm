// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/one_click_signin_dialog_controller.h"

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

namespace {

// Shift the origin of |view|'s frame by the given amount in the
// positive y direction (up).
void ShiftOriginY(NSView* view, CGFloat amount) {
  NSPoint origin = [view frame].origin;
  origin.y += amount;
  [view setFrameOrigin:origin];
}

}  // namespace

@implementation OneClickSigninDialogController

@synthesize useDefaultSettingsCheckbox = useDefaultSettingsCheckbox_;

- (id)initWithParentWindow:(NSWindow*)parentWindow
            acceptCallback:(const OneClickAcceptCallback&)acceptCallback {
  NSString* nibPath =
      [base::mac::FrameworkBundle()
           pathForResource:@"OneClickSigninDialog"
                    ofType:@"nib"];

  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    parentWindow_ = parentWindow;
    acceptCallback_ = acceptCallback;
  }
  return self;
}

- (void)runAsModalSheet {
  [NSApp beginSheet:[self window]
     modalForWindow:parentWindow_
      modalDelegate:self
     didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
        contextInfo:nil];
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window]];
}

- (IBAction)ok:(id)sender {
  [NSApp endSheet:[self window]];
  const bool useDefaultSettings =
      [useDefaultSettingsCheckbox_ state] == NSOnState;
  acceptCallback_.Run(useDefaultSettings);
}

- (void)awakeFromNib {
  // Make sure we're the window's delegate as set in the nib.
  DCHECK_EQ(self, [[self window] delegate]);

  // Lay out the text controls from the bottom up.

  CGFloat totalYOffset = 0.0;

  [GTMUILocalizerAndLayoutTweaker
      wrapButtonTitleForWidth:useDefaultSettingsCheckbox_];
  totalYOffset +=
      [GTMUILocalizerAndLayoutTweaker
          sizeToFitView:useDefaultSettingsCheckbox_].height;

  ShiftOriginY(messageField_, totalYOffset);
  totalYOffset +=
      [GTMUILocalizerAndLayoutTweaker
          sizeToFitFixedWidthTextField:messageField_];

  ShiftOriginY(headingField_, totalYOffset);
  totalYOffset +=
      [GTMUILocalizerAndLayoutTweaker
          sizeToFitFixedWidthTextField:headingField_];

  // Adjust the window to fit the text controls.
  [GTMUILocalizerAndLayoutTweaker
      resizeWindowWithoutAutoResizingSubViews:[self window]
                                        delta:NSMakeSize(0.0, totalYOffset)];
}

- (void)didEndSheet:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  [sheet close];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

@end  // OneClickSigninDialogController

void ShowOneClickSigninDialog(
    gfx::NativeWindow parent_window,
    const OneClickAcceptCallback& accept_callback) {
  OneClickSigninDialogController* controller =
      [[OneClickSigninDialogController alloc]
          initWithParentWindow:parent_window
                acceptCallback:accept_callback];
  [controller runAsModalSheet];
}
