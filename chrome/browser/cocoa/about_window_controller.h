// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_ABOUT_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_ABOUT_WINDOW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"
#import "chrome/app/keystone_glue.h"

@class BackgroundTileView;
class Profile;

// Returns an NSAttributedString that contains the locale specific legal text.
NSAttributedString* BuildAboutWindowLegalTextBlock();

// A window controller that handles the branded (Chrome.app) about
// window.  The branded about window has a few features beyond the
// standard Cocoa about panel.  For example, opening the about window
// will check to see if this version is current and tell the user.
// There is also an "update me now" button with a progress spinner.
@interface AboutWindowController : NSWindowController<KeystoneGlueCallbacks> {
 @private
  IBOutlet NSTextField* version_;
  IBOutlet BackgroundTileView* backgroundView_;
  IBOutlet NSImageView* logoView_;
  IBOutlet NSView* legalBlock_;
  IBOutlet NSTextView* legalText_;
  IBOutlet NSView* updateBlock_;  // Holds everything related to updates
  IBOutlet NSProgressIndicator* spinner_;
  IBOutlet NSImageView* updateStatusIndicator_;
  IBOutlet NSTextField* updateText_;
  IBOutlet NSButton* updateNowButton_;

  BOOL updateTriggered_;  // Has an update ever been triggered?
  Profile* profile_;  // Weak, probably the default profile.

  // The version we got told about by Keystone
  scoped_nsobject<NSString> newVersionAvailable_;
}

// Initialize the controller with the given profile, but does not show it.
// Callers still need to call showWindow: to put it on screen.
- (id)initWithProfile:(Profile*)profile;

// Trigger an update right now, as initiated by a button.
- (IBAction)updateNow:(id)sender;

@end


@interface AboutWindowController (JustForTesting)
- (NSTextView*)legalText;
- (NSButton*)updateButton;
- (NSTextField*)updateText;
@end


// NSNotification sent when the about window is closed.
extern NSString* const kUserClosedAboutNotification;

#endif  // CHROME_BROWSER_COCOA_ABOUT_WINDOW_CONTROLLER_H_
