// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ABOUT_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_ABOUT_WINDOW_CONTROLLER_H_
#pragma once

#import <AppKit/AppKit.h>

@class BackgroundTileView;
class Profile;

// This simple subclass of |NSTextView| just doesn't show the (text) cursor
// (|NSTextView| displays the cursor with full keyboard accessibility enabled).
@interface AboutLegalTextView : NSTextView
@end

// A window controller that handles the About box.
@interface AboutWindowController : NSWindowController {
 @private
  IBOutlet NSTextField* version_;
  IBOutlet BackgroundTileView* backgroundView_;
  IBOutlet NSImageView* logoView_;
  IBOutlet NSView* legalBlock_;
  IBOutlet AboutLegalTextView* legalText_;

  // updateBlock_ holds the update image or throbber, update text, and update
  // button.
  IBOutlet NSView* updateBlock_;

  IBOutlet NSProgressIndicator* spinner_;
  IBOutlet NSImageView* updateStatusIndicator_;
  IBOutlet NSTextField* updateText_;
  IBOutlet NSButton* updateNowButton_;
  IBOutlet NSButton* promoteButton_;

  Profile* profile_;  // Weak, probably the default profile.

  // The window frame height.  During an animation, this will contain the
  // height being animated to.
  CGFloat windowHeight_;
}

// Initialize the controller with the given profile, but does not show it.
// Callers still need to call showWindow: to put it on screen.
- (id)initWithProfile:(Profile*)profile;

// Trigger an update right now, as initiated by a button.
- (IBAction)updateNow:(id)sender;

// Install a system Keystone if necessary and promote the ticket to a system
// ticket.
- (IBAction)promoteUpdater:(id)sender;

@end  // @interface AboutWindowController

@interface AboutWindowController(JustForTesting)

- (NSTextView*)legalText;
- (NSButton*)updateButton;
- (NSTextField*)updateText;

// Returns an NSAttributedString that contains locale-specific legal text.
+ (NSAttributedString*)legalTextBlock;

@end  // @interface AboutWindowController(JustForTesting)

#endif  // CHROME_BROWSER_UI_COCOA_ABOUT_WINDOW_CONTROLLER_H_
