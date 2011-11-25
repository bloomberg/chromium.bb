// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_PANEL_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "ui/base/accelerators/accelerator_cocoa.h"

@class ConfirmQuitFrameView;

namespace confirm_quit {

enum ConfirmQuitMetric {
  // The user quit without having the feature enabled.
  kNoConfirm = 0,
  // The user held Cmd+Q for the entire duration.
  kHoldDuration,
  // The user hit Cmd+Q twice for the accelerated path.
  kDoubleTap,
  // The user tapped Cmd+Q once and then held it.
  kTapHold,

  kSampleCount
};

// Records the histogram value for the above metric.
void RecordHistogram(ConfirmQuitMetric sample);

}  // namespace confirm_quit

// The ConfirmQuitPanelController manages the black HUD window that tells users
// to "Hold Cmd+Q to Quit".
@interface ConfirmQuitPanelController : NSWindowController<NSWindowDelegate> {
 @private
  // The content view of the window that this controller manages.
  ConfirmQuitFrameView* contentView_;  // Weak, owned by the window.
}

// Returns a singleton instance of the Controller. This will create one if it
// does not currently exist.
+ (ConfirmQuitPanelController*)sharedController;

// Checks whether the |event| should trigger the feature.
+ (BOOL)eventTriggersFeature:(NSEvent*)event;

// Runs a modal loop that brings up the panel and handles the logic for if and
// when to terminate. Returns NSApplicationTerminateReply for use in
// -[NSApplicationDelegate applicationShouldTerminate:].
- (NSApplicationTerminateReply)runModalLoopForApplication:(NSApplication*)app;

// Shows the window.
- (void)showWindow:(id)sender;

// If the user did not confirm quit, send this message to give the user
// instructions on how to quit.
- (void)dismissPanel;

// Returns the Accelerator for the Quit menu item.
+ (ui::AcceleratorCocoa)quitAccelerator;

// Returns a string representation fit for display of |+quitAccelerator|.
+ (NSString*)keyCommandString;

@end

@interface ConfirmQuitPanelController (UnitTesting)
+ (NSString*)keyCombinationForAccelerator:(const ui::AcceleratorCocoa&)item;
@end

#endif  // CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_PANEL_CONTROLLER_H_
